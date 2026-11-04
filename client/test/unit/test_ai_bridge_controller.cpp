#include "aibridge/controller.hpp"
#include "aibridge/events.hpp"
#include "aibridge/audit.hpp"
#include "aibridge/instancefile.hpp"
#include "aibridge/runtime.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace aibridge;
namespace
{
    Json evidence(std::string kind = "control")
    {
        return Json::array({{{"source","client"},{"kind",kind},{"observed_at","1000"},{"snapshot_seq","1"},
            {"server_code",nullptr},{"description","fake adapter evidence"}}});
    }
    OperationUpdate success()
    {
        OperationUpdate value;
        value.outcome = OperationOutcome::Succeeded;
        value.completionScope = "observed_postcondition";
        value.evidence = evidence();
        return value;
    }
    std::string uuid(size_t n)
    {
        std::ostringstream s;
        s << "00000000-0000-4000-8000-" << std::hex << std::setw(12) << std::setfill('0') << n;
        return s.str();
    }
    Policy fullPolicy(bool callerGrants = false)
    {
        auto policy = Json::parse(R"({"allowed_actions":[],"allow_npc_dialogue":true,"allowed_peer_uids":"*","allowed_item_ids":"*","max_gold_per_operation":"100","max_gold_per_session":"200","max_item_count_per_operation":99})");
        for(const auto &[name, meta]: contract().at("x-methods").items()){
            if((meta.at("action") == true || name == "game_stop" || name == "game_grant_control")
                && (callerGrants || name != "game_grant_control")){
                policy["allowed_actions"].push_back(name);
            }
        }
        return Policy::parse(policy.dump());
    }
    struct Fake
    {
        uint64_t now = 1000;
        uint64_t sessionCounter = 0;
        size_t validates = 0;
        size_t begins = 0;
        size_t ticks = 0;
        size_t cancels = 0;
        size_t stops = 0;
        bool submitted = false;
        bool immediate = false;
        bool validationFails = false;
        bool beginThrows = false;
        bool ownsIntent = false;
        BudgetRequest cost;
        std::optional<OperationUpdate> nextTick;
        std::function<void(const OperationView &)> duringCancel;
        std::function<void()> duringValidate;
        std::unique_ptr<AiBridgeController> controller;

        explicit Fake(bool ready = true, Policy policy = fullPolicy(), bool enabled = true, bool automatic = false)
        {
            ControllerOptions options;
            options.instanceId = std::string(32,'a'); options.build = "controller-test";
            options.policy = std::move(policy); options.enabled = enabled;
            options.autoEnable = automatic;
            options.monotonicMs = [&]{return now;}; options.unixMs = [&]{return now + 100000;};
            options.newSessionId = [&]{std::ostringstream s;s<<std::hex<<std::setw(32)<<std::setfill('0')<<++sessionCounter;return s.str();};
            controller = std::make_unique<AiBridgeController>(std::move(options));
            controller->setStopHook([&]{
                if(!ownsIntent){return std::vector<std::string>{};}
                ownsIntent=false;++stops;return std::vector<std::string>{"move","attack","follow","queued"};
            });
            if(ready){controller->onConnected(1);controller->observe({"run",100,11,true});REQUIRE(controller->enableByUser());}
        }
        void registerAction(std::string name = "game_move_to")
        {
            ActionAdapter adapter;
            adapter.validate = [&](const Json &,const Json &){
                ++validates;
                if(duringValidate){duringValidate();}
                ValidationResult result;
                result.budget = cost;
                if(validationFails){result.error = OperationError{"stale_target","target changed"};}
                return result;
            };
            adapter.begin = [&](const OperationView &){
                ++begins;
                ownsIntent=true;
                if(beginThrows){throw std::runtime_error("private token or NPC text");}
                auto value = immediate ? success() : OperationUpdate{};
                value.submitted = submitted;
                return value;
            };
            adapter.tick = [&](const OperationView &)->std::optional<OperationUpdate>{++ticks;auto value=nextTick;nextTick.reset();return value;};
            adapter.cancel = [&](const OperationView &view){
                ++cancels;
                if(duringCancel){duringCancel(view);}
                OperationUpdate value;value.outcome=OperationOutcome::Cancelled;value.submitted=submitted;return value;
            };
            controller->registerAction(std::move(name),std::move(adapter));
        }
        Json request(std::string method,Json params = Json::object(),size_t n=1)
        {
            Json value={{"v",1},{"id","ipc-"+std::to_string(n)},{"method",method},{"params",params}};
            if(isAction(method)){
                value["request_id"]=uuid(n);value["context"]=controller->context();value["timeout_ms"]=1000;
            }
            return value;
        }
        Json call(const Json &r,uint64_t connection=1)
        {
            auto result=controller->handleRequest({connection,r});
            validateDefinition(result,result.at("ok")==true ? r.at("method").get<std::string>()+"Response" : "transportError");
            return result;
        }
        Json move(size_t n=1){return request("game_move_to",{{"x",10},{"y",20}},n);}
        Json buy(size_t n=1){return request("game_shop_buy",{{"quote_handle","quote"},{"count",1},{"max_total_gold","100"}},n);}
        Json operation(std::string id){return call(request("game_get_operation",{{"operation_id",id}})).at("data");}
    };
    std::string accepted(const Json &response)
    {
        REQUIRE(response.at("ok")==true);
        REQUIRE(response.at("data").at("state")=="accepted");
        return response.at("data").at("operation_id").get<std::string>();
    }
    void code(const Json &response,std::string value)
    {
        REQUIRE(response.at("ok")==false);
        CHECK(response.at("error").at("code")==value);
    }
}

TEST_CASE("task grant admits fresh authority and never replays queued work", "[task_grant]")
{
    Fake f(true, fullPolicy(true));
    f.registerAction();
    f.controller->disableByUser();
    const auto grant = [&](std::string mode, size_t id){
        return f.request("game_grant_control", {{"mode",mode},{"grant_id",uuid(800)},
            {"request_id",uuid(id)},{"character_uid","100"},{"context",f.controller->context()}}, id);
    };
    const auto first = grant("new_task", 801);
    REQUIRE(f.call(first).at("ok") == true);
    CHECK(f.begins == 0);
    const auto old = f.move(802);
    const auto stale = f.move(810);
    const auto queued = accepted(f.call(old));
    REQUIRE(f.operation(queued).at("state") == "accepted");
    f.controller->manualInput();
    CHECK(f.controller->controlState() == ControlState::Active);
    CHECK(f.operation(queued).at("state") == "cancelled");
    CHECK(f.operation(queued).at("side_effects_possible") == false);
    for(int i=0; i<100; ++i){ f.controller->advance(); }
    code(f.call(stale), "stale_context");
    const auto duplicate = f.call(old);
    REQUIRE(duplicate.at("ok") == true);
    CHECK(duplicate.at("data").at("operation_id") == queued);
    CHECK(duplicate.at("data").at("state") == "cancelled");
    CHECK(f.begins == 0);
    SECTION("same task can reobserve map and life recovery"){
        const auto initialSession=f.controller->context().at("session_id");
        f.controller->observe({"loading",std::nullopt,std::nullopt,false});
        REQUIRE(f.controller->context().is_null());
        f.controller->observe({"run",100,12,true});
        CHECK(f.controller->context().at("session_id") == initialSession);
        REQUIRE(f.call(grant("continue_task",803)).at("ok") == true);
        f.controller->observe({"run",100,12,false});
        code(f.call(grant("continue_task",804)), "paused");
        f.controller->observe({"run",100,12,true});
        CHECK(f.call(grant("continue_task",805)).at("ok") == true);
    }
    SECTION("explicit stop rejects old root but accepts a new local authorization"){
        REQUIRE(f.call(f.request("game_stop")).at("ok") == true);
        code(f.call(grant("continue_task",806)), "paused");
        REQUIRE(f.controller->enableByUser());
        code(f.call(grant("continue_task",809)), "paused");
        f.controller->manualInput();
        CHECK(f.controller->controlState() == ControlState::Active);
        CHECK(f.operation(queued).at("state") == "cancelled");
        CHECK(f.begins == 0);
        auto next = grant("new_task",807);
        next["params"]["grant_id"] = uuid(900);
        CHECK(f.call(next).at("ok") == true);
    }
    SECTION("consumed grant cannot be rebased as a fresh task"){
        code(f.call(grant("new_task",808)), "duplicate_conflict");
    }
}

TEST_CASE("task grants preserve uncertainty and reject identity and lifetime crossings", "[task_grant]")
{
    Fake f(true, fullPolicy(true));
    const auto grant = [&](size_t root, size_t request, std::string mode = "new_task"){
        return f.request("game_grant_control", {{"mode",mode},{"grant_id",uuid(root)},
            {"request_id",uuid(request)},{"character_uid","100"},{"context",f.controller->context()}},request);
    };
    REQUIRE(f.call(grant(1000,1001)).at("ok") == true);
    SECTION("manual fences every input while retaining the task"){
        const auto before = std::stoull(f.controller->context().at("control_epoch").get<std::string>());
        f.controller->manualInput(); f.controller->manualInput();
        CHECK(std::stoull(f.controller->context().at("control_epoch").get<std::string>()) == before+2);
        CHECK(f.controller->controlState() == ControlState::Active);
    }
    SECTION("economic unknown and reserved budget survive a new grant"){
        f.registerAction("game_shop_buy"); f.cost.gold=40; f.submitted=true;
        const auto id=accepted(f.call(f.buy(1002))); f.controller->advance();
        f.controller->manualInput();
        REQUIRE(f.operation(id).at("state") == "unknown");
        const auto policy=f.controller->session().at("capabilities").at("policy");
        REQUIRE(policy.at("economic_uncertain") == true);
        REQUIRE(f.call(grant(1100,1101)).at("ok") == true);
        CHECK(f.controller->session().at("capabilities").at("policy") == policy);
        CHECK(f.operation(id).at("side_effects_possible") == true);
        code(f.call(f.buy(1102)), "economic_uncertain");
        CHECK(f.begins == 1);
    }
    SECTION("local authorization preserves unknown economic facts and revokes the old root"){
        f.registerAction("game_shop_buy"); f.cost.gold=40; f.submitted=true;
        const auto id=accepted(f.call(f.buy(1300))); f.controller->advance();
        REQUIRE(f.call(f.request("game_stop")).at("ok") == true);
        REQUIRE(f.operation(id).at("state") == "unknown");
        const auto policy=f.controller->session().at("capabilities").at("policy");
        REQUIRE(f.controller->enableByUser());
        CHECK(f.controller->session().at("capabilities").at("policy") == policy);
        CHECK(f.operation(id).at("state") == "unknown");
        code(f.call(grant(1000,1301,"continue_task")), "paused");
        code(f.call(f.buy(1302)), "economic_uncertain");
        CHECK(f.begins == 1);
    }
    SECTION("explicit off invalidates continuation"){
        f.controller->disableByUser();
        code(f.call(grant(1000,1003,"continue_task")), "paused");
    }
    SECTION("wrong character cannot acquire authority"){
        auto request=grant(1200,1201); request["params"]["character_uid"]="101";
        code(f.call(request), "stale_context");
    }
    SECTION("unknown life is never assumed recovered"){
        RuntimeState state{"run",100,11,true}; state.lifeKnown=false;
        f.controller->observe(state);
        code(f.call(grant(1000,1202,"continue_task")), "paused");
    }
    SECTION("session change cannot continue an old task"){
        f.controller->observe({"run",101,11,true});
        auto request=grant(1000,1203,"continue_task"); request["params"]["character_uid"]="101";
        code(f.call(request), "paused");
    }
    SECTION("transport loss cannot continue an old task"){
        f.controller->onDisconnected(1); f.controller->onConnected(2);
        const auto response=f.call(grant(1000,1204,"continue_task"),2);
        code(response,"paused");
    }
    SECTION("an active operation is never replaced by a grant"){
        f.registerAction(); accepted(f.call(f.move(1002)));
        code(f.call(grant(1200,1205)),"busy");
        CHECK(f.begins == 0);
    }
}

TEST_CASE("targeted interruption preserves authority and fences only the owned operation", "[task_grant][internal_stop]")
{
    Fake f(true, fullPolicy(true));
    const auto grant = [&](size_t id, std::string mode = "continue_task"){
        return f.request("game_grant_control", {{"mode",mode},{"grant_id",uuid(1400)},
            {"request_id",uuid(id)},{"character_uid","100"},{"context",f.controller->context()}},id);
    };
    REQUIRE(f.call(grant(1401,"new_task")).at("ok") == true);
    f.registerAction();
    const auto old=f.move(1402);
    const auto stale=f.move(1403);
    const auto id=accepted(f.call(old));
    const auto interrupt=[&]{return f.request("game_stop",{{"interrupt_only",true},
        {"operation_id",id},{"context",f.controller->context()}});};
    SECTION("accepted intent is cancelled before begin"){
        const auto before=f.controller->context();
        const auto result=f.call(interrupt());REQUIRE(result.at("ok") == true);
        CHECK(result.at("data").at("paused") == false);
        CHECK(f.controller->context() != before);
        CHECK(f.controller->controlState() == ControlState::Active);
        CHECK(f.controller->session().at("pause_reason").is_null());
        CHECK(f.operation(id).at("state") == "cancelled");
        CHECK(f.operation(id).at("side_effects_possible") == false);
        REQUIRE(f.call(grant(1404)).at("ok") == true);
        code(f.call(stale),"stale_context");
        CHECK(f.call(old).at("data").at("state") == "cancelled");
        for(int i=0;i<100;++i){f.controller->advance();}
        CHECK(f.begins == 0);CHECK(f.stops == 0);
    }
    SECTION("running move stops once and only a new action begins"){
        f.submitted=true;f.controller->advance();REQUIRE(f.begins == 1);
        REQUIRE(f.call(interrupt()).at("ok") == true);
        CHECK(f.operation(id).at("state") == "cancelled");
        CHECK(f.operation(id).at("side_effects_possible") == true);
        CHECK(f.controller->controlState() == ControlState::Active);
        REQUIRE(f.call(grant(1404)).at("ok") == true);
        CHECK(f.call(old).at("data").at("state") == "cancelled");
        for(int i=0;i<100;++i){f.controller->advance();}
        CHECK(f.begins == 1);CHECK(f.stops == 1);
        accepted(f.call(f.move(1405)));f.controller->advance();
        CHECK(f.begins == 2);
    }
    SECTION("wrong operation context or connection has no effect"){
        auto request=interrupt();
        SECTION("wrong id"){request["params"]["operation_id"]="missing";code(f.call(request),"operation_not_found");}
        SECTION("stale context"){request["params"]["context"]["control_epoch"]="0";code(f.call(request),"stale_context");}
        SECTION("wrong connection"){code(f.call(request,2),"unauthorized");}
        CHECK(f.operation(id).at("state") == "accepted");CHECK(f.begins == 0);
        CHECK(f.controller->context() == old.at("context"));
    }
    SECTION("explicit stop or local disable cannot be undone by interruption"){
        SECTION("explicit first"){REQUIRE(f.call(f.request("game_stop")).at("ok") == true);}
        SECTION("local disable first"){f.controller->disableByUser();}
        const auto before=f.controller->session();
        REQUIRE(f.call(interrupt()).at("ok") == true);
        CHECK(f.controller->session() == before);
        code(f.call(grant(1404)),"paused");
    }
    SECTION("interruption then explicit stop still revokes"){
        REQUIRE(f.call(interrupt()).at("ok") == true);
        REQUIRE(f.call(f.request("game_stop")).at("ok") == true);
        code(f.call(grant(1404)),"paused");
    }
    SECTION("terminal replay cannot stop a newer operation"){
        auto first=interrupt();REQUIRE(f.call(first).at("ok") == true);
        code(f.call(first),"stale_context");
        const auto next=accepted(f.call(f.move(1405)));f.controller->advance();
        const auto before=f.controller->context();
        REQUIRE(f.call(interrupt()).at("ok") == true);
        CHECK(f.controller->context() == before);CHECK(f.operation(next).at("state") == "running");
        CHECK(f.begins == 1);CHECK(f.cancels == 0);
    }
    SECTION("dead unknown and disabled states are never activated"){
        SECTION("death"){f.controller->observe({"run",100,11,false});}
        SECTION("unknown life"){RuntimeState state{"run",100,11,true};state.lifeKnown=false;f.controller->observe(state);}
        SECTION("disabled"){f.controller->disableByUser();}
        const auto before=f.controller->session();
        REQUIRE(f.call(interrupt()).at("ok") == true);
        CHECK(f.controller->session() == before);
        code(f.call(grant(1404)),"paused");
    }
}

TEST_CASE("targeted interruption retains unknown economics and requires grant policy", "[task_grant][internal_stop]")
{
    SECTION("unknown economic outcome preserves reservation and budget"){
        Fake f(true,fullPolicy(true));f.registerAction("game_shop_buy");f.cost.gold=40;f.submitted=true;
        const auto root=uuid(1500);
        const auto grant=[&](size_t n,std::string mode){return f.request("game_grant_control",
            {{"mode",mode},{"grant_id",root},{"request_id",uuid(n)},{"character_uid","100"},{"context",f.controller->context()}});};
        REQUIRE(f.call(grant(1501,"new_task")).at("ok") == true);
        const auto id=accepted(f.call(f.buy(1502)));f.controller->advance();
        REQUIRE(f.call(f.request("game_stop",{{"interrupt_only",true},{"operation_id",id},{"context",f.controller->context()}})).at("ok") == true);
        REQUIRE(f.operation(id).at("state") == "unknown");
        const auto policy=f.controller->session().at("capabilities").at("policy");
        REQUIRE(policy.at("economic_uncertain") == true);
        REQUIRE(f.call(grant(1504,"continue_task")).at("ok") == true);
        CHECK(f.controller->session().at("capabilities").at("policy") == policy);
        CHECK(f.operation(id).at("state") == "unknown");
        code(f.call(f.buy(1505)),"economic_uncertain");CHECK(f.begins == 1);
    }
    SECTION("legacy policy cannot opt into interruption"){
        Fake f;f.registerAction();const auto id=accepted(f.call(f.move()));
        code(f.call(f.request("game_stop",{{"interrupt_only",true},{"operation_id",id},{"context",f.controller->context()}})),"policy_denied");
        CHECK(f.operation(id).at("state") == "accepted");CHECK(f.begins == 0);
    }
}

TEST_CASE("controller rechecks deadline and cancellation after adapter validation")
{
    Fake f;
    f.registerAction();
    const auto id=accepted(f.call(f.move()));
    SECTION("validation crosses deadline")
    {
        f.duringValidate=[&]{f.now+=1000;};
        f.controller->advance();
        CHECK(f.operation(id).at("state")=="failed");
        CHECK(f.operation(id).at("error").at("code")=="timeout");
    }
    SECTION("validation reenters manual takeover")
    {
        f.duringValidate=[&]{f.controller->manualInput();};
        f.controller->advance();
        CHECK(f.operation(id).at("state")=="cancelled");
    }
    CHECK(f.begins==0);
}

TEST_CASE("controller exposes only real registered capabilities and readonly lifecycle")
{
    Fake f(false);
    auto session=f.call(f.request("game_get_session")).at("data");
    CHECK(session.at("control_state")=="read_only");
    CHECK(session.at("context").is_null());
    CHECK(session.at("capabilities").at("tools").size()==5);
    CHECK_FALSE(f.controller->enableByUser());
    code(f.call(f.request("game_get_state")),"not_in_game");
    f.controller->observe({"run",100,11,true});
    code(f.call(f.request("game_get_state")),"unavailable");
    f.registerAction();
    CHECK(f.controller->session().at("capabilities").at("tools").size()==6);
    code(f.call(f.move()),"paused");
    REQUIRE(f.controller->enableByUser());
    CHECK(f.controller->controlState()==ControlState::Active);
    auto attemptedResume=f.request("game_resume");
    code(f.call(attemptedResume),"invalid_argument");
    Fake disabled(false,fullPolicy(),false);
    CHECK(disabled.controller->controlState()==ControlState::Disabled);
    CHECK_FALSE(disabled.controller->enableByUser());
}

TEST_CASE("initial automatic control requires a live RUN and an authenticated binding in either order")
{
    Fake f(false,fullPolicy(),true,true);
    SECTION("binding first"){
        f.controller->onConnected(1);
        CHECK(f.controller->controlState()==ControlState::ReadOnly);
        f.controller->observe({"run",100,11,false});
        CHECK(f.controller->controlState()==ControlState::Paused);
        f.controller->observe({"run",100,11,true});
        CHECK(f.controller->controlState()==ControlState::Paused); // Death never arms startup again.
    }
    SECTION("live RUN first"){
        f.controller->observe({"run",100,11,true});
        CHECK(f.controller->controlState()==ControlState::ReadOnly);
        const auto before=f.controller->context();
        const auto session=f.call(f.request("game_get_session")).at("data");
        CHECK(session.at("control_state")=="active");
        CHECK(session.at("context")!=before);
        const auto granted=f.controller->context();
        for(int i=0;i<5;++i){ f.controller->observe({"run",100,11,true}); }
        CHECK(f.controller->context()==granted);
    }
    SECTION("authentication before live RUN"){
        f.controller->onConnected(1);
        f.controller->observe({"loading",{},{},false});
        f.controller->observe({"run",100,11,true});
        CHECK(f.controller->controlState()==ControlState::Active);
    }
}

TEST_CASE("scene readiness waits for known health before the initial authenticated startup grant")
{
    Fake f(false,fullPolicy(),true,true);
    int bindingOrder=0;
    SECTION("authentication before scene"){ bindingOrder=0; }
    SECTION("authentication after scene before health"){ bindingOrder=1; }
    SECTION("authentication after health"){ bindingOrder=2; }
    if(bindingOrder==0){ f.controller->onConnected(1); }
    RuntimeState scene{"run",100,11,false};
    scene.observeLife(std::nullopt);
    REQUIRE_FALSE(scene.alive);
    REQUIRE_FALSE(scene.lifeKnown);
    f.controller->observe(scene);
    const auto pendingContext=f.controller->context();
    if(bindingOrder==1){ f.controller->onConnected(1); }
    for(int i=0;i<3;++i){ f.controller->observe(scene); }
    CHECK(f.controller->controlState()==ControlState::ReadOnly);
    CHECK(f.controller->pauseReason().empty());
    scene.observeLife(false);
    f.controller->observe(scene);
    if(bindingOrder==2){ f.controller->onConnected(1); }
    REQUIRE(f.controller->controlState()==ControlState::Active);
    CHECK(f.controller->pauseReason().empty());
    CHECK(f.controller->context().at("session_id")==pendingContext.at("session_id"));
    const auto granted=f.controller->context();
    for(int i=0;i<3;++i){ f.controller->observe(scene); }
    CHECK(f.controller->context()==granted);
}

TEST_CASE("unknown startup health becoming confirmed dead consumes the grant even after revival")
{
    Fake f(false,fullPolicy(),true,true);
    f.controller->onConnected(1);
    RuntimeState scene{"run",100,11,false};
    scene.observeLife(std::nullopt);
    f.controller->observe(scene);
    scene.observeLife(false,true); // A genuine death notification overrides a stale live health sample.
    f.controller->observe(scene);
    REQUIRE(f.controller->controlState()==ControlState::Paused);
    CHECK(f.controller->pauseReason()=="death");
    CHECK_FALSE(f.controller->enableByUser());
    scene.observeLife(false);
    for(int i=0;i<3;++i){ f.controller->observe(scene); }
    CHECK(f.controller->controlState()==ControlState::Paused);
    CHECK(f.controller->pauseReason()=="death");
}

TEST_CASE("explicit startup interruption while health is unknown never re-arms on a live sample")
{
    Fake f(false,fullPolicy(),true,true);
    f.controller->onConnected(1);
    RuntimeState scene{"run",100,11,false};
    scene.observeLife(std::nullopt);
    f.controller->observe(scene);
    bool disabled=false;
    SECTION("explicit disable"){ f.controller->disableByUser(); disabled=true; }
    SECTION("manual takeover"){ f.controller->manualInput(); }
    SECTION("stop"){ f.call(f.request("game_stop")); }
    SECTION("transport loss"){ f.controller->onDisconnected(1); f.controller->onConnected(2); }
    scene.observeLife(false);
    for(int i=0;i<3;++i){ f.controller->observe(scene); }
    CHECK(f.controller->controlState()==(disabled ? ControlState::Disabled : ControlState::Paused));
}

TEST_CASE("losing known life after activation stops control without automatically resuming")
{
    Fake f;
    RuntimeState scene{"run",100,11,false};
    scene.observeLife(std::nullopt);
    f.controller->observe(scene);
    CHECK(f.controller->controlState()==ControlState::Paused);
    CHECK(f.controller->pauseReason()=="life_unknown");
    CHECK_FALSE(f.controller->enableByUser());
    scene.observeLife(false);
    for(int i=0;i<3;++i){ f.controller->observe(scene); }
    CHECK(f.controller->controlState()==ControlState::Paused);
}

TEST_CASE("explicit disable cancels work but preserves diagnostics and cannot be undone by observation or stop")
{
    Fake f(false,fullPolicy(),true,true);
    f.registerAction();
    f.controller->observe({"run",100,11,true});
    f.controller->onConnected(1);
    const auto old=f.move();
    const auto id=accepted(f.call(old));
    const auto lifetime=f.controller->lifetime().lock();
    f.controller->disableByUser();
    CHECK(f.controller->controlState()==ControlState::Disabled);
    CHECK(f.controller->pauseReason()=="disabled_by_user");
    CHECK(f.operation(id).at("state")=="cancelled");
    for(int i=0;i<5;++i){
        f.controller->observe({"run",100,11,true}); f.controller->advance();
        CHECK(f.call(f.request("game_get_session")).at("data").at("control_state")=="disabled");
    }
    CHECK(f.call(f.request("game_get_events",{{"after_seq","0"}})).at("ok")==true);
    CHECK(f.call(f.request("game_stop")).at("ok")==true);
    CHECK(f.controller->controlState()==ControlState::Disabled);
    CHECK(f.begins==0);
    CHECK(f.controller->lifetime().lock()==lifetime);
    REQUIRE(f.controller->enableByUser());
    auto stale=f.request("game_move_to",{{"x",10},{"y",20}},2);
    stale["context"]=old.at("context");
    code(f.call(stale),"stale_context");
}

TEST_CASE("takeover stop world changes and transport loss consume initial startup rather than resume")
{
    Fake f(false,fullPolicy(),true,true);
    f.controller->observe({"run",100,11,true});
    SECTION("manual input before host"){
        f.controller->manualInput();
        f.controller->onConnected(1);
    }
    SECTION("world changes before host"){
        f.controller->observe({"run",100,12,true});
        f.controller->onConnected(1);
    }
    SECTION("stop before live control"){
        f.call(f.request("game_stop"));
    }
    SECTION("transport lost"){
        f.controller->onConnected(1);
        f.controller->onDisconnected(1);
        f.controller->onConnected(2);
    }
    for(int i=0;i<4;++i){ f.controller->observe({"run",100,12,true}); }
    CHECK(f.controller->controlState()==ControlState::Paused);
    CHECK(f.stops==0);
}

TEST_CASE("explicit disable preserves the uncertainty and budget of submitted economic work")
{
    Fake f(false,fullPolicy(),true,true);
    f.cost.gold=40; f.cost.itemCount=1; f.cost.itemIds={1}; f.submitted=true;
    f.registerAction("game_shop_buy");
    f.controller->onConnected(1); f.controller->observe({"run",100,11,true});
    const auto id=accepted(f.call(f.buy())); f.controller->advance();
    REQUIRE(f.begins==1);
    f.controller->disableByUser();
    CHECK(f.operation(id).at("state")=="unknown");
    CHECK(f.operation(id).at("side_effects_possible")==true);
    CHECK(f.stops==1);
    for(int i=0;i<3;++i){ f.controller->observe({"run",100,11,true}); f.controller->advance(); }
    const auto policy=f.controller->session().at("capabilities").at("policy");
    CHECK(policy.at("economic_uncertain")==true);
    CHECK(policy.at("remaining_gold_budget")=="160");
    CHECK(f.controller->controlState()==ControlState::Disabled);
    REQUIRE(f.controller->enableByUser());
    code(f.call(f.buy(2)),"economic_uncertain");
    CHECK(f.begins==1);
}

TEST_CASE("automatic startup never reopens an internal fault or a stale accepted intent")
{
    Fake f(false,fullPolicy(),true,true);
    f.registerAction(); f.beginThrows=true;
    f.controller->onConnected(1); f.controller->observe({"run",100,11,true});
    const auto id=accepted(f.call(f.move()));
    f.controller->advance();
    CHECK(f.operation(id).at("state")=="unknown");
    for(int i=0;i<3;++i){ f.controller->observe({"run",100,11,true}); f.controller->advance(); }
    CHECK(f.controller->pauseReason()=="controller_error");
    CHECK_FALSE(f.controller->enableByUser());
    CHECK(f.begins==1);
}

TEST_CASE("manual takeover cancels accepted work before begin and preserves human input")
{
    Fake f;f.registerAction();
    const auto old=f.move();
    const auto id=accepted(f.call(old));
    CHECK(f.begins==0);
    f.controller->manualInput();
    bool humanAction=true;
    f.controller->advance();
    CHECK(humanAction);
    CHECK(f.begins==0);
    CHECK(f.stops==0);
    CHECK(f.operation(id).at("state")=="cancelled");
    auto stale=old;stale["request_id"]=uuid(2);
    code(f.call(stale),"stale_context");
    code(f.call(f.move(3)),"paused");
    f.controller->manualInput();
    CHECK(f.stops==0);
    REQUIRE(f.controller->enableByUser());
    auto next=accepted(f.call(f.move(4)));f.controller->advance();
    CHECK(f.begins==1);
    f.controller->manualInput();
    CHECK(f.stops==1);
    CHECK(f.operation(next).at("state")=="cancelled");
}

TEST_CASE("action preconditions are revalidated before execution and busy does not enqueue")
{
    Fake f;f.registerAction();
    const auto id=accepted(f.call(f.move()));
    code(f.call(f.move(2)),"busy");
    f.validationFails=true;
    f.controller->advance();
    CHECK(f.validates==2);
    CHECK(f.begins==0);
    CHECK(f.operation(id).at("state")=="failed");
    CHECK(f.operation(id).at("error").at("code")=="stale_target");
}

TEST_CASE("deduplication uses canonical defaults and conflicts never execute twice")
{
    Fake f;f.registerAction();
    const auto request=f.move();const auto id=accepted(f.call(request));
    auto duplicate=request;duplicate["id"]="new-ipc";duplicate["params"]["tolerance"]=0;
    CHECK(f.call(duplicate).at("data").at("operation_id")==id);
    auto conflict=duplicate;conflict["params"]["x"]=11;
    code(f.call(conflict),"duplicate_conflict");
    f.controller->advance();
    CHECK(f.begins==1);
    f.controller->report(id,success());
    CHECK(f.call(request).at("data").at("state")=="succeeded");
    f.controller->advance();CHECK(f.begins==1);
}

TEST_CASE("4096 entry ledger never evicts and stop bypasses exhaustion")
{
    Fake f;f.registerAction();f.immediate=true;
    auto first=f.move();
    double totalUs=0;
    double maxUs=0;
    for(size_t i=1;i<=4096;++i){
        const auto start=std::chrono::steady_clock::now();
        accepted(f.call(f.move(i)));f.controller->advance();
        const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        totalUs+=elapsed;maxUs=std::max(maxUs,elapsed);
    }
    // Diagnostic only: schema validation and host scheduling make the runtime budget soft.
    std::cout << "Fake controller request+advance: samples=4096 mean_us=" << totalUs/4096
        << " max_us=" << maxUs << "; excludes transport, disk audit and game rendering\n";
    CHECK(f.begins==4096);
    code(f.call(f.move(4097)),"ledger_full");
    CHECK(f.call(first).at("data").at("state")=="succeeded");
    const auto stop=f.call(f.request("game_stop"));
    CHECK(stop.at("data").at("paused")==true);
    CHECK(f.controller->controlState()==ControlState::Paused);
    f.controller->onDisconnected(1);f.controller->onConnected(2);
    REQUIRE(f.controller->enableByUser());
    const auto request=f.move(4098);
    accepted(f.call(request,2));
}

TEST_CASE("map role death and RUN changes invalidate contexts and never auto resume")
{
    Fake f;f.registerAction();
    auto initial=f.controller->context();
    const auto id=accepted(f.call(f.move()));
    f.controller->observe({"run",100,12,true});
    CHECK(f.operation(id).at("state")=="cancelled");
    CHECK(f.controller->context().at("session_id")==initial.at("session_id"));
    CHECK(f.controller->context().at("world_epoch")!=initial.at("world_epoch"));
    CHECK(f.controller->controlState()==ControlState::Paused);
    REQUIRE(f.controller->enableByUser());
    f.controller->observe({"run",100,12,false});
    CHECK_FALSE(f.controller->enableByUser());
    CHECK(f.controller->pauseReason()=="death");
    f.controller->observe({"run",100,12,true});
    CHECK(f.controller->controlState()==ControlState::Paused);
    f.controller->observe({"select_character",std::nullopt,std::nullopt,false});
    CHECK(f.controller->context().is_null());
    f.controller->observe({"run",101,12,true});
    CHECK(f.controller->context().at("session_id")!=initial.at("session_id"));
    CHECK(f.controller->controlState()==ControlState::Paused);
}

TEST_CASE("readonly input and focus observation do not cancel normal human work")
{
    Fake f(false);f.controller->onConnected(1);f.controller->observe({"run",100,11,true});
    f.controller->manualInput();CHECK(f.stops==0);
    CHECK(f.controller->controlState()==ControlState::ReadOnly);
    REQUIRE(f.controller->enableByUser());
    const auto context=f.controller->context();
    f.controller->observe({"run",100,11,true});
    CHECK(f.controller->context()==context);
    CHECK(f.controller->controlState()==ControlState::Active);
}

TEST_CASE("submitted economic cancellation is unknown and retains budget across reconnect")
{
    Fake f;f.cost.gold=40;f.cost.itemCount=1;f.cost.itemIds={1};f.submitted=true;f.registerAction("game_shop_buy");
    const auto id=accepted(f.call(f.buy()));f.controller->advance();
    f.controller->manualInput();
    auto record=f.operation(id);
    CHECK(record.at("state")=="unknown");CHECK(record.at("side_effects_possible")==true);
    CHECK(record.at("cancel_requested")==true);
    auto policy=f.controller->session().at("capabilities").at("policy");
    CHECK(policy.at("economic_uncertain")==true);CHECK(policy.at("remaining_gold_budget")=="160");
    REQUIRE(f.controller->enableByUser());code(f.call(f.buy(2)),"economic_uncertain");
    f.controller->onDisconnected(1);f.controller->onConnected(2);
    CHECK(f.controller->controlState()==ControlState::Paused);
    CHECK(f.controller->session().at("capabilities").at("policy").at("remaining_gold_budget")=="160");
    code(f.call(f.request("game_get_session"),1),"unauthorized");
    REQUIRE(f.controller->enableByUser());code(f.call(f.buy(3),2),"economic_uncertain");
    f.controller->observe({"disconnected",std::nullopt,std::nullopt,false});
    f.controller->observe({"run",100,11,true});
    CHECK(f.controller->session().at("capabilities").at("policy").at("remaining_gold_budget")=="200");
    CHECK(f.controller->session().at("capabilities").at("policy").at("economic_uncertain")==false);
}

TEST_CASE("known rejection releases reservation and successful cost survives pause resume")
{
    Fake f;f.registerAction("game_shop_buy");f.cost.gold=80;f.cost.itemCount=1;f.submitted=true;
    auto id=accepted(f.call(f.buy()));f.controller->advance();
    OperationUpdate rejected;rejected.outcome=OperationOutcome::Failed;rejected.submitted=true;
    rejected.error=OperationError{"server_rejected","rejected"};rejected.evidence=evidence("server_ack");
    f.controller->report(id,rejected);
    CHECK(f.controller->session().at("capabilities").at("policy").at("remaining_gold_budget")=="200");
    for(size_t i=2;i<=3;++i){id=accepted(f.call(f.buy(i)));f.controller->advance();f.controller->report(id,success());}
    f.controller->pauseByUser();REQUIRE(f.controller->enableByUser());
    CHECK(f.controller->session().at("capabilities").at("policy").at("remaining_gold_budget")=="40");
    code(f.call(f.buy(4)),"budget_exceeded");
}

TEST_CASE("late reply at exact deadline stays unknown and cannot trigger another action")
{
    Fake f;f.registerAction("game_shop_buy");f.cost.gold=20;f.submitted=true;
    const auto id=accepted(f.call(f.buy()));f.controller->advance();f.now+=1000;
    auto reply=success();reply.submitted=true;
    f.controller->report(id,reply);
    const auto op=f.operation(id);
    CHECK(op.at("state")=="unknown");CHECK(op.at("error").at("code")=="timeout");
    CHECK(op.at("late_resolution").at("state")=="succeeded");
    CHECK(f.begins==1);
    CHECK(f.controller->session().at("capabilities").at("policy").at("remaining_gold_budget")=="180");
    f.controller->report(id,reply);
    CHECK(f.controller->session().at("capabilities").at("policy").at("remaining_gold_budget")=="180");
}

TEST_CASE("timeout is established before reentrant cancellation reply")
{
    Fake f;f.registerAction("game_shop_buy");f.cost.gold=30;f.submitted=true;
    const auto id=accepted(f.call(f.buy()));f.controller->advance();
    f.duringCancel=[&](const OperationView &view){auto reply=success();reply.submitted=true;f.controller->report(view.operationId,reply);};
    f.now+=1000;f.controller->advance();
    const auto op=f.operation(id);
    CHECK(op.at("state")=="unknown");CHECK(op.at("late_resolution").at("state")=="succeeded");
    CHECK(f.cancels==1);CHECK(f.stops==1);
}

TEST_CASE("deadline maintenance cancels expired work even when ordinary work has no budget")
{
    Fake f;f.registerAction();
    const auto id=accepted(f.call(f.move()));
    bool begun=false;
    SECTION("accepted work expires without being started"){}
    SECTION("submitted running work stops without ticking")
    {
        f.submitted=true;
        f.controller->advance();
        begun=true;
    }
    for(size_t i=0;i<10;++i){f.now+=99;f.controller->checkDeadlines();}
    CHECK(f.cancels==0);
    f.now+=10;
    f.controller->checkDeadlines();
    f.controller->checkDeadlines();
    const auto op=f.operation(id);
    CHECK(op.at("error").at("code")=="timeout");
    CHECK(op.at("state")== (begun ? "unknown" : "failed"));
    CHECK(f.begins== (begun ? 1 : 0));
    CHECK(f.cancels== (begun ? 1 : 0));
    CHECK(f.ticks==0);
    CHECK_FALSE(f.ownsIntent);
    CHECK_FALSE(f.controller->activeOperation());
}

TEST_CASE("adapter errors cannot fabricate success or disclose private errors")
{
    Fake f;f.registerAction();f.beginThrows=true;
    auto id=accepted(f.call(f.move()));f.controller->advance();
    auto op=f.operation(id);CHECK(op.at("state")=="unknown");
    CHECK(op.dump().find("private token")==std::string::npos);
    CHECK(f.controller->controlState()==ControlState::Paused);
    CHECK_FALSE(f.controller->enableByUser());
    CHECK(f.controller->faultDiagnostic()=="begin/standard");
    Fake g;g.registerAction();
    id=accepted(g.call(g.move(2)));g.controller->advance();
    auto invalid=success();invalid.evidence=Json::array();
    CHECK_THROWS_AS(g.controller->report(id,invalid),ProtocolError);
    CHECK(g.operation(id).at("state")=="running");
    g.nextTick=invalid;
    g.controller->advance();
    CHECK(g.controller->controlState()==ControlState::Paused);
    CHECK(g.controller->faultDiagnostic()=="tick_result/protocol");
    CHECK_FALSE(g.controller->enableByUser());
}

TEST_CASE("event ring reports gaps limits and consistent cursors")
{
    EventRing events;
    for(size_t i=0;i<1100;++i){events.append(i,"control",nullptr,std::nullopt,"state changed");}
    const auto first=events.read(0,100);
    CHECK(first.at("gap")==true);CHECK(first.at("events").size()==100);
    CHECK(first.at("events").front().at("event_seq")=="77");
    const auto next=events.read(std::stoull(first.at("next_seq").get<std::string>()),100);
    CHECK(next.at("gap")==false);CHECK(next.at("events").front().at("event_seq")=="177");
    CHECK(events.read(1100,100).at("events").empty());
    CHECK_THROWS_AS(events.read(1101,100),ProtocolError);
    CHECK_THROWS_AS(events.read(0,101),ProtocolError);
}

TEST_CASE("shutdown invalidates weak callbacks and does not resume on stale connection")
{
    Fake f;f.registerAction();
    const auto id=accepted(f.call(f.move()));f.controller->advance();
    const auto weak=f.controller->lifetime();CHECK_FALSE(weak.expired());
    f.controller->shutdown();f.controller->shutdown();CHECK(weak.expired());
    CHECK(f.cancels==1);CHECK_FALSE(f.controller->enableByUser());
    code(f.call(f.request("game_get_session")),"unavailable");
    f.controller->report(id,success());CHECK(f.begins==1);
}

TEST_CASE("domain snapshot notifications use current context and preserve event gaps")
{
    Fake f(false);
    auto read=[&]{return f.call(f.request("game_get_events",{{"after_seq","0"},{"limit",100}})).at("data");};
    const auto initial=read();
    f.controller->notifySnapshot();
    CHECK(read()==initial);
    f.controller->observe({"run",100,11,true});
    CHECK(f.controller->controlState()==ControlState::ReadOnly);
    f.controller->notifySnapshot();
    const auto snapshots=read();
    CHECK(snapshots.at("events").back().at("kind")=="snapshot");
    CHECK(snapshots.at("events").back().at("context")==f.controller->context());
    CHECK(snapshots.at("events").back().at("operation_id").is_null());
    for(size_t i=0;i<1030;++i){f.controller->notifySnapshot();}
    const auto overflow=read();
    CHECK(overflow.at("gap")==true);
    CHECK(overflow.at("events").size()==100);
    CHECK(overflow.at("events").front().at("kind")=="snapshot");
    const auto cursor=overflow.at("next_seq");
    const auto next=f.call(f.request("game_get_events",{{"after_seq",cursor},{"limit",100}})).at("data");
    CHECK(next.at("gap")==false);
}

TEST_CASE("audit rotates three bounded files and never serializes params or input text")
{
    const auto root=std::filesystem::path(MIR2X_AI_TEST_DIR)/"controller-audit";
    std::filesystem::create_directories(root);
    auto credentials=InstanceFile::create(std::filesystem::absolute(root/"private"),12345,"audit-test");
    AuditLog log(root/"private"/credentials->info().instance_id,512);
    OperationRecord record;record.view.operationId="operation";record.view.method="game_npc_input";
    record.view.params={{"text","NPC_SECRET_TEXT"},{"token","TOKEN_SECRET"}};
    record.error=OperationError{"server_rejected","SECRET_ERROR"};
    for(size_t i=0;i<40;++i){log.write(i,record.view.method,nullptr,&record);}
    size_t count=0;
    for(const auto &entry:std::filesystem::directory_iterator(root/"private"/credentials->info().instance_id)){
        ++count;CHECK(entry.file_size()<=512);
        std::ifstream in(entry.path());std::string text((std::istreambuf_iterator<char>(in)),{});
        CHECK(text.find("SECRET")==std::string::npos);CHECK(text.find("params")==std::string::npos);
        CHECK(text.find("game_npc_input")!=std::string::npos);
    }
    CHECK(count==3);
}

TEST_CASE("loading invalidates context without resetting login budget")
{
    Fake f;f.registerAction("game_shop_buy");f.cost.gold=80;f.submitted=true;
    auto id=accepted(f.call(f.buy()));f.controller->advance();f.controller->report(id,success());
    const auto previous=f.controller->context();
    f.controller->observe({"loading",std::nullopt,std::nullopt,false});
    CHECK(f.controller->context().is_null());
    f.controller->observe({"run",100,12,true});
    CHECK(f.controller->context().at("session_id")!=previous.at("session_id"));
    CHECK(f.controller->session().at("capabilities").at("policy").at("remaining_gold_budget")=="120");
    REQUIRE(f.controller->enableByUser());
    id=accepted(f.call(f.buy(2)));f.controller->advance();f.controller->manualInput();
    f.controller->observe({"loading",std::nullopt,std::nullopt,false});
    f.controller->observe({"run",100,13,true});
    CHECK(f.controller->session().at("capabilities").at("policy").at("economic_uncertain")==true);
    CHECK(f.controller->session().at("capabilities").at("policy").at("remaining_gold_budget")=="40");
}

TEST_CASE("budget quote changes before begin and explicit policy denials never submit")
{
    Fake f;f.registerAction("game_shop_buy");f.cost.gold=20;
    const auto id=accepted(f.call(f.buy()));f.cost.gold=21;f.controller->advance();
    CHECK(f.begins==0);CHECK(f.operation(id).at("error").at("code")=="stale_target");
    CHECK(f.controller->session().at("capabilities").at("policy").at("remaining_gold_budget")=="200");
    Fake denied(true,Policy::parse("{}"));denied.registerAction();
    code(denied.call(denied.move()),"policy_denied");CHECK(denied.begins==0);
    Fake pvp;pvp.registerAction();pvp.cost.pvp=true;pvp.cost.peerUid=123;
    code(pvp.call(pvp.move()),"policy_denied");
}

TEST_CASE("disconnect drops accepted work and stale callbacks cannot reach a new connection")
{
    Fake f;f.registerAction();const auto id=accepted(f.call(f.move()));
    f.controller->onDisconnected(1);f.controller->advance();CHECK(f.begins==0);
    f.controller->onConnected(2);
    code(f.call(f.request("game_get_operation",{{"operation_id",id}}),2),"operation_not_found");
    f.controller->report(id,success());CHECK(f.begins==0);
    CHECK(f.controller->controlState()==ControlState::Paused);
    REQUIRE(f.controller->enableByUser());
    const auto next=accepted(f.call(f.move(2),2));CHECK(next!=id);
}

TEST_CASE("audit failure after acceptance fails closed before any game submission")
{
    const auto root=std::filesystem::path(MIR2X_AI_TEST_DIR)/"controller-audit-fault";
    std::filesystem::create_directories(root);
    auto credentials=InstanceFile::create(std::filesystem::absolute(root/"private"),12345,"audit-test");
    const auto directory=root/"private"/credentials->info().instance_id;
    ControllerOptions options;options.instanceId=credentials->info().instance_id;options.build="audit-fault";
    options.policy=fullPolicy();options.auditDirectory=directory;
    AiBridgeController controller(std::move(options));
    controller.onConnected(1);controller.observe({"run",100,11,true});REQUIRE(controller.enableByUser());
    size_t begins=0;
    controller.registerAction("game_move_to",{
        [](const Json &,const Json &){return ValidationResult{};},
        [&](const OperationView &){++begins;return OperationUpdate{};},
        [](const OperationView &)->std::optional<OperationUpdate>{return std::nullopt;},
        [](const OperationView &){OperationUpdate value;value.outcome=OperationOutcome::Cancelled;return value;}});
    const auto log=directory/"audit.jsonl";
    REQUIRE(std::filesystem::is_regular_file(log));std::filesystem::remove(log);std::filesystem::create_directory(log);
    Json request={{"v",1},{"id","ipc"},{"method","game_move_to"},{"params",{{"x",1},{"y",1}}},
        {"context",controller.context()},{"request_id",uuid(1)},{"timeout_ms",1000}};
    const auto response=controller.handleRequest({1,request});code(response,"unavailable");
    controller.advance();CHECK(begins==0);CHECK(controller.controlState()==ControlState::Paused);
    CHECK_FALSE(controller.enableByUser());CHECK_FALSE(controller.activeOperation());
}

TEST_CASE("SDL input policy covers all conflicts without treating focus or releases as takeover")
{
    for(const auto type:{SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_WHEEL,SDL_EVENT_KEY_DOWN,SDL_EVENT_TEXT_INPUT}){
        SDL_Event event{};event.type=type;CHECK(ControlGesture::manual(event));
    }
    for(const auto type:{SDL_EVENT_MOUSE_MOTION,SDL_EVENT_MOUSE_BUTTON_UP,SDL_EVENT_KEY_UP,SDL_EVENT_WINDOW_FOCUS_LOST,
        SDL_EVENT_WINDOW_FOCUS_GAINED,SDL_EVENT_WINDOW_RESIZED,SDL_EVENT_WINDOW_EXPOSED}){
        SDL_Event event{};event.type=type;CHECK_FALSE(ControlGesture::manual(event));
    }
    SDL_Event repeat{};repeat.type=SDL_EVENT_KEY_DOWN;repeat.key.repeat=true;CHECK_FALSE(ControlGesture::manual(repeat));
}

TEST_CASE("control buttons require matched window context and complete pointer gesture")
{
    ControlGesture gesture;
    const SDL_FRect enable{10,10,100,30},pause{120,10,100,30};
    SDL_Event down{};down.type=SDL_EVENT_MOUSE_BUTTON_DOWN;down.button.button=SDL_BUTTON_LEFT;
    down.button.windowID=1;down.button.x=20;down.button.y=20;
    SDL_Event up=down;up.type=SDL_EVENT_MOUSE_BUTTON_UP;
    CHECK(gesture.route(up,enable,pause,"ctx")==ControlGesture::Result::Pass);
    CHECK(gesture.route(down,enable,pause,"ctx")==ControlGesture::Result::Consumed);
    CHECK(gesture.route(up,enable,pause,"ctx")==ControlGesture::Result::Enable);
    CHECK(gesture.pendingButton()==0);
    gesture.route(down,enable,pause,"ctx");
    CHECK(gesture.route(up,enable,pause,"new-context")==ControlGesture::Result::Consumed);
    gesture.route(down,enable,pause,"ctx");up.button.windowID=2;
    CHECK(gesture.route(up,enable,pause,"ctx")==ControlGesture::Result::Consumed);
    up.button.windowID=1;gesture.route(down,enable,pause,"ctx");up.button.x=250;
    CHECK(gesture.route(up,enable,pause,"ctx")==ControlGesture::Result::Consumed);
    down.button.x=130;up.button.x=130;gesture.route(down,enable,pause,"ctx");
    CHECK(gesture.route(up,enable,pause,"ctx")==ControlGesture::Result::Pause);
    down.button.button=SDL_BUTTON_RIGHT;
    CHECK(gesture.route(down,enable,pause,"ctx")==ControlGesture::Result::Pass);
}

TEST_CASE("typing and focus loss invalidate a pending restore gesture")
{
    ControlGesture gesture;
    const SDL_FRect enable{10,10,100,30},pause{120,10,100,30};
    SDL_Event down{};down.type=SDL_EVENT_MOUSE_BUTTON_DOWN;down.button.button=SDL_BUTTON_LEFT;
    down.button.windowID=1;down.button.x=20;down.button.y=20;
    SDL_Event up=down;up.type=SDL_EVENT_MOUSE_BUTTON_UP;
    for(const auto type:{SDL_EVENT_TEXT_INPUT,SDL_EVENT_KEY_DOWN,SDL_EVENT_WINDOW_FOCUS_LOST}){
        gesture.route(down,enable,pause,"ctx");SDL_Event interrupt{};interrupt.type=type;
        CHECK(gesture.route(interrupt,enable,pause,"ctx")==ControlGesture::Result::Pass);
        CHECK(gesture.route(up,enable,pause,"ctx")==ControlGesture::Result::Pass);
    }
}

TEST_CASE("pause clears completed AI-owned intentions without touching later human intentions")
{
    Fake f;f.registerAction();
    const auto id=accepted(f.call(f.move()));f.controller->advance();f.controller->report(id,success());
    REQUIRE_FALSE(f.controller->activeOperation());REQUIRE(f.ownsIntent);
    f.controller->manualInput();CHECK(f.stops==1);CHECK_FALSE(f.ownsIntent);
    f.controller->manualInput();CHECK(f.stops==1);
    f.call(f.request("game_stop"));CHECK(f.stops==1);
}
