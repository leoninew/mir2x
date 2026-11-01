#include "aibridge/economy.hpp"
#include "aibridge/snapshot.hpp"
#include "aibridge/controller.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <iostream>
using namespace aibridge;
namespace
{
    Json context(){ return {{"instance_id",std::string(32,'a')},{"session_id",std::string(32,'b')},{"world_epoch","1"},{"control_epoch","1"}}; }
    ShopQuote quote(){ return {1,2,3,UINT64_MAX,{{4,5}},false}; }
}
TEST_CASE("economy quotes preserve exact uint64 costs and expire at thirty seconds")
{
    QuoteStore store("test"); auto q=store.publish(quote(),context(),"shop",7,100,1000);
    auto json=quoteJson(q); CHECK(json["gold"]=="18446744073709551615"); CHECK(json["unit_count"]==3);
    CHECK(store.resolve(q.handle,context(),"shop",7,30099).value.materials[0].count==5);
    CHECK_THROWS_AS(store.resolve(q.handle,context(),"shop",7,30100),ProtocolError);
    CHECK_THROWS_AS(store.resolve(q.handle,context(),"shop",8,200),ProtocolError);
    CHECK_THROWS_AS(store.resolve(q.handle,context(),"other",7,200),ProtocolError);
    auto changed=context(); changed["control_epoch"]="2";
    CHECK_NOTHROW(store.resolve(q.handle,changed,"shop",7,200));
    changed["world_epoch"]="2"; CHECK_THROWS_AS(store.resolve(q.handle,changed,"shop",7,200),ProtocolError);
    CHECK_THROWS_AS(store.resolve(q.handle,context(),"shop",7,99),ProtocolError);
}
TEST_CASE("economy quote capacity and clear never resurrect old handles")
{
    QuoteStore store("test"); const auto first=store.publish(quote(),context(),"shop",1,0,0);
    for(size_t i=0;i<QuoteStore::capacity;++i){ store.publish(quote(),context(),"shop",1,0,0); }
    CHECK_THROWS_AS(store.resolve(first.handle,context(),"shop",1,0),ProtocolError);
    store.clear(); CHECK(store.publish(quote(),context(),"shop",1,0,0).handle!=first.handle);
}
TEST_CASE("economy uncorrelated refresh merges requests and quarantines timed out keys")
{
    EconomyRefresh cache; CHECK(cache.begin(1)); CHECK_FALSE(cache.begin(2));
    CHECK(cache.receive(100)); CHECK(cache.known); CHECK(cache.revision==1);
    CHECK(cache.begin(101)); cache.expire(10101);
    CHECK(cache.quarantined); CHECK_FALSE(cache.pending); CHECK(cache.error->code=="timeout");
    CHECK_FALSE(cache.receive(10102)); CHECK(cache.revision==1); CHECK_FALSE(cache.begin(20000));
    EconomyRefresh otherCategory; CHECK(otherCategory.begin(20000)); CHECK(otherCategory.receive(20001));
}
TEST_CASE("economy handles bind full observation scope revision identity and lifetime")
{
    EconomicHandles handles; const auto h=handles.publish({{"auction_id","18446744073709551615"}},context(),9,10);
    CHECK(handles.resolve(h,context(),30009).revision==9);
    CHECK_THROWS_AS(handles.resolve(h,context(),30010),ProtocolError);
    auto next=context(); next["session_id"]=std::string(32,'c'); CHECK_THROWS_AS(handles.resolve(h,next,11),ProtocolError);
    handles.clear(); CHECK_THROWS_AS(handles.resolve(h,context(),11),ProtocolError);
}
TEST_CASE("economy pagination freezes metadata filters and newest two snapshots")
{
    EconomicPages pages;
    std::vector<Json> rows; for(int i=0;i<201;++i){ rows.push_back({{"value",i}}); }
    Json meta={{"snapshot_seq","1"},{"unavailable_fields",Json::array({"items[].name"})}};
    auto first=pages.first("auction",context(),{{"category",1}},meta,rows);
    auto cursor=first.at("next_cursor").get<std::string>();
    auto second=pages.next("auction",context(),{{"category",1}},cursor);
    CHECK(second["items"][0]["value"]==100); CHECK(second["snapshot_seq"]=="1"); CHECK(second["unavailable_fields"]==meta["unavailable_fields"]);
    CHECK_THROWS_AS(pages.next("auction",context(),{{"category",2}},cursor),ProtocolError);
    pages.first("delivery",context(),{},meta,rows);
    pages.first("auction",context(),{},meta,rows);
    CHECK_NOTHROW(pages.next("auction",context(),{{"category",1}},cursor));
    pages.first("auction",context(),{},meta,rows);
    CHECK_THROWS_AS(pages.next("auction",context(),{{"category",1}},cursor),ProtocolError);
    CHECK_THROWS_AS(pages.first("auction",context(),{},meta,std::vector<Json>(8193)),ProtocolError);
}
TEST_CASE("economy system delivery records cannot be forged by ordinary chat XML")
{
    const std::string xml="<layout><par>gold &amp; item</par><par><event id='_RSVD_NAME_DELIVERY_5639021748316' record='12345678901234567890123456789012'>claim</event></par></layout>";
    CHECK(systemDeliveries(false,xml).empty());
    const auto records=systemDeliveries(true,xml); REQUIRE(records.size()==1);
    CHECK(records[0].record.size()==32); CHECK(records[0].text=="gold & itemclaim");
    CHECK(systemDeliveries(true,"<layout><event id='fake' record='12345678901234567890123456789012'>x</event></layout>").empty());
    CHECK(systemDeliveries(true,"<layout><event id='_RSVD_NAME_DELIVERY_5639021748316' record='short'>x</event></layout>").empty());
    CHECK_THROWS_AS(systemDeliveries(true,std::string(65537,'x')),ProtocolError);
}
TEST_CASE("economy correlated receipt classification keeps sent unknown uncertain")
{
    EconomicRequest request;
    CHECK(economicUpdate(request).outcome==OperationOutcome::Running);
    request.submitted=true;
    CHECK(economicUpdate(request).outcome==OperationOutcome::Running);
    request.receipt=EconomicReceipt::Unknown;
    CHECK(economicUpdate(request).outcome==OperationOutcome::Unknown);
    request.receipt=EconomicReceipt::Ok;
    auto success=economicUpdate(request,true);
    CHECK(success.completionScope=="server_ack"); CHECK(success.result["delivery_pending"]==true);
    for(const auto &e:success.evidence){ CHECK_NOTHROW(validateDefinition(e,"evidence")); }
    CHECK_NOTHROW(validateDefinition(success.result,"actionResult"));
    request.receipt=EconomicReceipt::Rejected; request.error=3;
    auto rejected=economicUpdate(request); CHECK(rejected.outcome==OperationOutcome::Failed); CHECK_NOTHROW(rejected.error->json());
    request.submitted=false; request.receipt=EconomicReceipt::Unknown;
    CHECK(economicUpdate(request).outcome==OperationOutcome::Failed);
}
TEST_CASE("economy shared final send guard refuses deadline boundary manual takeover and changed context")
{
    OperationView view{.operationId="op",.context=context(),.deadlineMs=1000};
    CHECK_FALSE(validateEconomicSend(view,context(),true,999));
    CHECK(validateEconomicSend(view,context(),true,1000)->code=="timeout");
    CHECK(validateEconomicSend(view,context(),false,999)->code=="stale_context");
    auto next=context(); next["control_epoch"]="2";
    CHECK(validateEconomicSend(view,next,true,999)->code=="stale_context");
    next=context(); next["world_epoch"]="2";
    CHECK(validateEconomicSend(view,next,true,999)->code=="stale_context");
}
TEST_CASE("economy trade stages cannot mistake lock confirmation or unrelated error for transfer")
{
    CHECK(tradeUpdate("game_trade_offer",TradeReceipt::OfferAccepted,true).outcome==OperationOutcome::Succeeded);
    CHECK(tradeUpdate("game_trade_lock",TradeReceipt::Locked,true).completionScope=="observed_postcondition");
    CHECK(tradeUpdate("game_trade_commit",TradeReceipt::Locked,true).outcome==OperationOutcome::Running);
    CHECK(tradeUpdate("game_trade_commit",TradeReceipt::UnrelatedError,true,7).outcome==OperationOutcome::Running);
    CHECK(tradeUpdate("game_trade_commit",TradeReceipt::Complete,true).outcome==OperationOutcome::Succeeded);
    CHECK(tradeUpdate("game_trade_cancel",TradeReceipt::Closed,true).outcome==OperationOutcome::Succeeded);
    CHECK(tradeUpdate("game_trade_commit",TradeReceipt::Closed,true).outcome==OperationOutcome::Failed);
    CHECK(tradeUpdate("game_trade_request",TradeReceipt::Started,true).outcome==OperationOutcome::Succeeded);
    CHECK(tradeUpdate("game_trade_respond",TradeReceipt::Started,true).outcome==OperationOutcome::Succeeded);
}
TEST_CASE("economy controller reserves unknown spend deduplicates and records late receipt without replay")
{
    uint64_t now=100; size_t sends=0;
    ControllerOptions options;
    options.instanceId=std::string(32,'a'); options.build="economy-test";
    options.policy=Policy::parse(R"({"allowed_actions":"*","allowed_item_ids":"*","allowed_peer_uids":"*","max_gold_per_operation":"100","max_gold_per_session":"200","max_item_count_per_operation":100})");
    options.monotonicMs=[&]{return now;}; options.unixMs=[&]{return now;}; options.newSessionId=[]{return std::string(32,'b');};
    AiBridgeController controller(options);
    controller.registerAction("game_auction_buy",{
        .validate=[](const Json &,const Json &){return ValidationResult{.budget={.economic=true,.gold=70}};},
        .begin=[&](const OperationView &){++sends; return OperationUpdate{.submitted=true};},
        .tick=[](const OperationView &)->std::optional<OperationUpdate>{return {};},
        .cancel=[](const OperationView &v){return OperationUpdate{.outcome=OperationOutcome::Unknown,.submitted=v.submitted};}});
    controller.onConnected(1); controller.observe({"run",100,11,true}); REQUIRE(controller.enableByUser());
    const Json request={{"v",1},{"id","one"},{"method","game_auction_buy"},{"params",{{"auction_handle","h"},{"max_total_gold","70"}}},
        {"request_id","00000000-0000-4000-8000-000000000001"},{"context",controller.context()},{"timeout_ms",1000}};
    const auto accepted=controller.handleRequest({1,request}); REQUIRE(accepted.at("ok")==true);
    const auto id=accepted.at("data").at("operation_id").get<std::string>();
    controller.advance(); CHECK(sends==1); CHECK(controller.handleRequest({1,request}).at("ok")==true); CHECK(sends==1);
    now=1100; controller.checkDeadlines();
    auto policy=controller.session().at("capabilities").at("policy");
    CHECK(policy.at("economic_uncertain")==true); CHECK(policy.at("remaining_gold_budget")=="130");
    controller.report(id,economicUpdate({true,EconomicReceipt::Ok,0},true));
    CHECK(controller.session().at("capabilities").at("policy").at("remaining_gold_budget")=="130");
    CHECK(controller.session().at("capabilities").at("policy").at("economic_uncertain")==true);
    controller.advance(); CHECK(sends==1);
    auto op=controller.handleRequest({1,{{"v",1},{"id","op"},{"method","game_get_operation"},{"params",{{"operation_id",id}}}}});
    REQUIRE(op.at("ok")==true); CHECK(op.at("data").at("state")=="unknown"); CHECK(op.at("data").at("late_resolution").at("state")=="succeeded");
}

TEST_CASE("economy controller does not spend quoted gold on offers locks or auction asking price")
{
    std::string method;
    Json params;
    SECTION("trade offer") { method="game_trade_offer"; params={{"trade_handle","trade"},{"gold","100"},{"items",Json::array()}}; }
    SECTION("trade lock") { method="game_trade_lock"; params={{"trade_handle","trade"},{"offer_revision","1"}}; }
    SECTION("auction register") { method="game_auction_register"; params={{"item_id",1},{"seq_id",2},{"price","9223372036854775807"},{"note",""}}; }
    ControllerOptions options;
    options.instanceId=std::string(32,'a'); options.build="economy-zero-spend";
    options.policy=Policy::parse(R"({"allowed_actions":"*","allowed_item_ids":"*","allowed_peer_uids":"*","max_gold_per_operation":"100","max_gold_per_session":"200","max_item_count_per_operation":100})");
    options.monotonicMs=[]{return uint64_t(100);}; options.unixMs=options.monotonicMs;
    options.newSessionId=[]{return std::string(32,'b');};
    AiBridgeController controller(options);
    controller.registerAction(method,{
        .validate=[](const Json &,const Json &){return ValidationResult{.budget={.economic=true,.gold=0}};},
        .begin=[&](const OperationView &){
            return method=="game_auction_register" ? economicUpdate({true,EconomicReceipt::Ok,0})
                : tradeUpdate(method,method=="game_trade_offer" ? TradeReceipt::OfferAccepted : TradeReceipt::Locked,true);
        },
        .tick=[](const OperationView &)->std::optional<OperationUpdate>{return {};},
        .cancel=[](const OperationView &){return OperationUpdate{.outcome=OperationOutcome::Cancelled};}});
    controller.onConnected(1); controller.observe({"run",100,11,true}); REQUIRE(controller.enableByUser());
    const Json request={{"v",1},{"id","one"},{"method",method},{"params",params},
        {"request_id","00000000-0000-4000-8000-000000000001"},{"context",controller.context()},{"timeout_ms",1000}};
    REQUIRE(controller.handleRequest({1,request}).at("ok")==true);
    controller.advance();
    CHECK(controller.session().at("capabilities").at("policy").at("remaining_gold_budget")=="200");
    CHECK(controller.pauseReason()!="controller_error");
}

TEST_CASE("economy maximum bounded snapshot probe keeps newest complete frozen page")
{
    const auto start=std::chrono::steady_clock::now();
    EconomicHandles handles; EconomicPages pages;
    std::vector<Json> rows; rows.reserve(8192);
    for(uint64_t i=0;i<8192;++i){
        rows.push_back({{"handle",handles.publish({{"id",i}},context(),1,1)}});
    }
    auto first=pages.first("auction",context(),{},snapshotMetadata({1,1},"client_known"),std::move(rows));
    size_t total=first["items"].size();
    while(!first["next_cursor"].is_null()){
        first=pages.next("auction",context(),{},first["next_cursor"].get<std::string>());
        total+=first["items"].size();
    }
    CHECK(total==8192);
    const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"economy bounded 8192 handles plus frozen pages: "<<elapsed<<" us (pure helper; no GUI timing claim)\n";
}
