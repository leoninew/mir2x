#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <asio.hpp>
#include "aibridge/gameadapter.hpp"
#include "aibridge/runtime.hpp"
#include "bgmusicdb.hpp"
#include "cerealf.hpp"
#include "client.hpp"
#include "clientargparser.hpp"
#include "emojidb.hpp"
#include "fontexdb.hpp"
#include "imeboard.hpp"
#include "log.hpp"
#include "mapbindb.hpp"
#include "messagestackboard.hpp"
#include "pngtexdb.hpp"
#include "pngtexoffdb.hpp"
#include "processrun.hpp"
#include "soundeffectdb.hpp"
#ifdef CHECK
#undef CHECK
#endif
#include <catch2/catch_test_macros.hpp>

ClientArgParser   *g_clientArgParser = nullptr;
Log               *g_mir2xLog        = nullptr;
PNGTexDB          *g_progUseDB       = nullptr;
PNGTexDB          *g_itemDB          = nullptr;
PNGTexDB          *g_mapDB           = nullptr;
PNGTexOffDB       *g_heroDB          = nullptr;
PNGTexOffDB       *g_hairDB          = nullptr;
PNGTexOffDB       *g_monsterDB       = nullptr;
PNGTexOffDB       *g_weaponDB        = nullptr;
PNGTexOffDB       *g_helmetDB        = nullptr;
PNGTexOffDB       *g_equipDB         = nullptr;
PNGTexOffDB       *g_magicDB         = nullptr;
PNGTexOffDB       *g_standNPCDB      = nullptr;
PNGTexOffDB       *g_selectCharDB    = nullptr;
EmojiDB           *g_emojiDB         = nullptr;
BGMusicDB         *g_bgmDB           = nullptr;
SoundEffectDB     *g_seffDB          = nullptr;
MapBinDB          *g_mapBinDB        = nullptr;
FontexDB          *g_fontexDB        = nullptr;
SDLDevice         *g_sdlDevice       = nullptr;
IMEBoard          *g_imeBoard        = nullptr;
MessageStackBoard *g_notifyBoard     = nullptr;
Client            *g_client          = nullptr;

struct AiCombatTestAccess
{
    static void attach(Client &client, std::unique_ptr<ProcessRun> scene)
    {
        client.m_currentProcess = std::move(scene);
    }
    static void detach(Client &client)
    {
        client.m_currentProcess.reset();
    }
};

namespace
{
    struct CombatGuiResources
    {
        std::unique_ptr<ClientArgParser> args;
        std::unique_ptr<Log> log;
        std::unique_ptr<SDLDevice> device;
        std::unique_ptr<PNGTexDB> prog, items, map;
        std::array<std::unique_ptr<PNGTexOffDB>, 9> sprites;
        std::unique_ptr<FontexDB> fonts;
        std::unique_ptr<MapBinDB> maps;
        std::unique_ptr<EmojiDB> emojis;
        std::unique_ptr<BGMusicDB> music;
        std::unique_ptr<SoundEffectDB> sounds;
        std::unique_ptr<Client> client;
        std::unique_ptr<IMEBoard> ime;
        std::unique_ptr<MessageStackBoard> notices;

        CombatGuiResources()
        {
            const char *overridePath = std::getenv("MIR2X_TEST_RES_DIR");
            std::string resourceArg = "--res-path=" + std::string(overridePath ? overridePath : MIR2X_TEST_DEFAULT_RES_DIR);
            char name[] = "combat_gui_test", audio[] = "--disable-audio", profiler[] = "--disable-profiler";
            char bridge[] = "--enable-ai-bridge=0";
            char *argv[] = {name, audio, profiler, bridge, resourceArg.data()};
            args = std::make_unique<ClientArgParser>(argf::parser(5, argv)); g_clientArgParser = args.get();
            logDisableProfiler();
            log = std::make_unique<Log>("mir2x-combat-gui-test", "."); g_mir2xLog = log.get();
            device = std::make_unique<SDLDevice>(); g_sdlDevice = device.get();
            prog = std::make_unique<PNGTexDB>(1024); g_progUseDB = prog.get();
            items = std::make_unique<PNGTexDB>(1024); g_itemDB = items.get();
            map = std::make_unique<PNGTexDB>(8192); g_mapDB = map.get();
            for(auto &sprite: sprites){ sprite = std::make_unique<PNGTexOffDB>(1024); }
            g_heroDB = sprites[0].get(); g_hairDB = sprites[1].get(); g_monsterDB = sprites[2].get();
            g_weaponDB = sprites[3].get(); g_helmetDB = sprites[4].get(); g_equipDB = sprites[5].get();
            g_magicDB = sprites[6].get(); g_standNPCDB = sprites[7].get(); g_selectCharDB = sprites[8].get();
            fonts = std::make_unique<FontexDB>(64 * 1024 * 1024); g_fontexDB = fonts.get();
            maps = std::make_unique<MapBinDB>(); g_mapBinDB = maps.get();
            emojis = std::make_unique<EmojiDB>(); g_emojiDB = emojis.get();
            music = std::make_unique<BGMusicDB>(5); g_bgmDB = music.get();
            sounds = std::make_unique<SoundEffectDB>(128); g_seffDB = sounds.get();
            // This Client only loads real resources. Its NetIO is never started or polled:
            // sends exercise the real serializer/sequence path without a live connection.
            client = std::make_unique<Client>(); g_client = client.get();
            ime.reset(new IMEBoard{{}}); g_imeBoard = ime.get();
            notices.reset(new MessageStackBoard{{.width = 400}}); g_notifyBoard = notices.get();
        }
        ~CombatGuiResources()
        {
            notices.reset(); ime.reset(); client.reset();
            prog->clear(); items->clear(); map->clear(); fonts->clear(); emojis->clear();
            maps->clear(); music->clear(); sounds->clear();
            for(auto &sprite: sprites){ sprite->clear(); }
            g_client = nullptr; g_imeBoard = nullptr; g_notifyBoard = nullptr;
            g_progUseDB = nullptr; g_itemDB = nullptr; g_mapDB = nullptr; g_fontexDB = nullptr;
            g_heroDB = nullptr; g_hairDB = nullptr; g_monsterDB = nullptr; g_weaponDB = nullptr;
            g_helmetDB = nullptr; g_equipDB = nullptr; g_magicDB = nullptr; g_standNPCDB = nullptr;
            g_selectCharDB = nullptr; g_mapBinDB = nullptr; g_emojiDB = nullptr;
            g_bgmDB = nullptr; g_seffDB = nullptr;
        }
    };

    CombatGuiResources &resources()
    {
        static CombatGuiResources fixture;
        return fixture;
    }

    SMOnlineOK online()
    {
        SMOnlineOK result{};
        result.uid = uidf::getPlayerUID(8);
        result.gender = 1;
        result.job = JOB_WARRIOR;
        result.mapUID = uidf::getBaseMapUID(DBCOM_MAPID(u8"道馆_1"), 0);
        result.action = ActionStand{.direction = DIR_RIGHT, .x = 430, .y = 108};
        return result;
    }

    void settle(MyHero &hero)
    {
        for(int i = 0; i < 100 && !hero.aiMotionStopped(); ++i){ hero.update(100); }
        REQUIRE(hero.aiMotionStopped());
    }

    void receive(ProcessRun &scene, uint64_t uid, const ActionNode &action)
    {
        const SMAction message{.UID = uid, .mapUID = scene.mapUID(), .action = action};
        scene.on_SM_ACTION(reinterpret_cast<const uint8_t *>(&message), sizeof(message));
    }

    uint64_t cow(ProcessRun &scene)
    {
        const auto uid = uidf::buildMonsterUID(DBCOM_MONSTERID(u8"牛"), 0);
        scene.onActionSpawn(uid, ActionSpawn{.direction = DIR_LEFT, .x = 431, .y = 108});
        REQUIRE(scene.findUID(uid));
        return uid;
    }

    void health(ProcessRun &scene, uint64_t uid, int hp)
    {
        const auto data = cerealf::serialize(SDHealth{.uid = uid, .hp = hp, .maxHP = hp});
        scene.on_SM_HEALTH(reinterpret_cast<const uint8_t *>(data.data()), data.size());
    }

    struct AdapterScene
    {
        CombatGuiResources &fixture = resources();
        std::filesystem::path root;
        std::unique_ptr<aibridge::AiBridgeRuntime> runtime;
        ProcessRun *scene = nullptr;
        std::string operation;

        AdapterScene(int x, int y, bool callerGrants = false)
        {
            auto initial = online();
            initial.action = ActionStand{.direction = DIR_DOWN, .x = x, .y = y};
            auto process = std::make_unique<ProcessRun>(initial);
            scene = process.get();
            AiCombatTestAccess::attach(*fixture.client, std::move(process));
            const auto start = cerealf::serialize(SDStartGameScene{
                .uid = initial.uid, .mapUID = initial.mapUID, .x = x, .y = y, .name = "combat-test"});
            scene->on_SM_STARTGAMESCENE(reinterpret_cast<const uint8_t *>(start.data()), start.size());
            health(*scene, initial.uid, 300);
            settle(hero());
            const auto nonce = std::chrono::system_clock::now().time_since_epoch().count();
            root = std::filesystem::current_path() / ("adapter-" + std::to_string(nonce));
            std::filesystem::create_directory(root);
            const auto policy = root / "policy.json";
            {
                aibridge::Json config={{"allowed_actions",{"game_attack","game_move_to"}}};
                if(callerGrants){config["allowed_actions"].push_back("game_grant_control");}
                std::ofstream out(policy);out << config.dump();
            }
            // Only the bridge listener is started, in a dedicated test directory.
            // Requests enter the real controller directly; game NetIO remains unpolled.
            runtime = std::make_unique<aibridge::AiBridgeRuntime>(*fixture.client);
            runtime->start({root / "bridge", policy, "combat-gui-test"});
            runtime->observe(scene->aiState());
            controller().onConnected(1);
            if(callerGrants){REQUIRE(controller().enableByUser());}
            REQUIRE(controller().controlState() == aibridge::ControlState::Active);
        }
        ~AdapterScene()
        {
            runtime.reset();
            AiCombatTestAccess::detach(*fixture.client);
        }
        MyHero &hero() { return *scene->getMyHero(); }
        aibridge::AiBridgeController &controller() { return *runtime->controller(); }
        uint64_t pig(int x, int y, bool knownHealth = true)
        {
            const auto uid = uidf::buildMonsterUID(DBCOM_MONSTERID(u8"猪"), 0);
            scene->onActionSpawn(uid, ActionSpawn{.direction = DIR_UP, .x = x, .y = y});
            REQUIRE(scene->findUID(uid));
            if(knownHealth){ health(*scene, uid, 20); }
            return uid;
        }
        aibridge::Json attack(uint64_t uid, int timeout = 30000)
        {
            const auto handle = runtime->game()->targetHandle(uid, controller().context());
            const auto response = controller().handleRequest({1, {
                {"v", 1}, {"id", "attack"}, {"method", "game_attack"},
                {"params", {{"target_handle", handle}, {"mode", "until_dead"}}},
                {"context", controller().context()}, {"timeout_ms", timeout},
                {"request_id", "00000000-0000-4000-8000-000000000001"}}});
            if(response.at("ok") == true){ operation = response.at("data").at("operation_id").get<std::string>(); }
            return response;
        }
        aibridge::Json status()
        {
            return controller().handleRequest({1, {{"v", 1}, {"id", "status"}, {"method", "game_get_operation"},
                {"params", {{"operation_id", operation}}}}}).at("data");
        }
        aibridge::Json move(aibridge::GridPoint destination, int timeout = 30000)
        {
            const auto response = controller().handleRequest({1, {
                {"v", 1}, {"id", "move"}, {"method", "game_move_to"},
                {"params", {{"x", destination.x}, {"y", destination.y}, {"tolerance", 0}}},
                {"context", controller().context()}, {"timeout_ms", timeout},
                {"request_id", "00000000-0000-4000-8000-000000000002"}}});
            if(response.at("ok") == true){ operation = response.at("data").at("operation_id").get<std::string>(); }
            return response;
        }
        void step()
        {
            hero().update(100);
            controller().advance();
        }
    };

    std::vector<aibridge::GridPoint> freeNeighbors(AdapterScene &test)
    {
        std::vector<aibridge::GridPoint> result;
        const auto motion = test.hero().currMotion();
        for(int dx = -1; dx <= 1; ++dx){
            for(int dy = -1; dy <= 1; ++dy){
                if((dx || dy) && test.scene->canMove(true, 2, motion->endX, motion->endY, motion->endX + dx, motion->endY + dy)){
                    result.push_back({motion->endX + dx, motion->endY + dy});
                }
            }
        }
        REQUIRE(result.size() >= 2);
        return result;
    }

    void enterStationaryHit(AdapterScene &test)
    {
        const auto motion = test.hero().currMotion();
        receive(*test.scene, test.hero().UID(), ActionHitted{
            .direction = motion->direction, .x = motion->endX, .y = motion->endY});
        for(int i = 0; i < 10 && test.hero().currMotion()->type != MOTION_HITTED; ++i){ test.hero().update(100); }
        REQUIRE(test.hero().currMotion()->type == MOTION_HITTED);
        REQUIRE(test.hero().currMotion()->frame == 0);
        REQUIRE(test.hero().stayIdle());
        REQUIRE(test.hero().dead() == false);
        REQUIRE(test.hero().actionSendSequence() == 0);
    }

    void rightClickMove(AdapterScene &test, aibridge::GridPoint destination)
    {
        const auto [viewX, viewY] = test.scene->getViewShift();
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.windowID = SDL_GetWindowID(SDL_GetRenderWindow(g_sdlDevice->getRenderer()));
        event.button.button = SDL_BUTTON_RIGHT;
        event.button.x = destination.x * SYS_MAPGRIDXP + SYS_MAPGRIDXP / 2 - viewX;
        event.button.y = destination.y * SYS_MAPGRIDYP + SYS_MAPGRIDYP / 2 - viewY;
        REQUIRE(test.scene->getFocusUID(FOCUS_MOUSE) == 0);
        test.scene->processEvent(event);
    }
}

TEST_CASE("Runtime targeted cancellation precedes queued movement and explicit stop retains priority")
{
    AdapterScene test(430,108,true);
    using aibridge::Json;
    using namespace std::chrono_literals;
    const auto context=test.controller().context();
    std::ifstream file(test.root/"bridge"/(context.at("instance_id").get<std::string>()+".json"));
    const auto instance=Json::parse(file);
    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    socket.connect({asio::ip::make_address_v4("127.0.0.1"),instance.at("port").get<uint16_t>()});
    socket.non_blocking(true);
    std::vector<uint8_t> received;
    const auto send=[&](const std::vector<Json> &requests){
        std::vector<uint8_t> frames;
        for(const auto &request:requests){
            const auto frame=aibridge::encodeFrame(request);
            frames.insert(frames.end(),frame.begin(),frame.end());
        }
        asio::write(socket,asio::buffer(frames));
    };
    const auto collect=[&](size_t count){
        std::vector<Json> responses;
        const auto deadline=std::chrono::steady_clock::now()+2s;
        while(responses.size()<count && std::chrono::steady_clock::now()<deadline){
            test.runtime->poll();
            std::array<uint8_t,8192> buffer{};asio::error_code error;
            const auto n=socket.read_some(asio::buffer(buffer),error);
            if(!error){received.insert(received.end(),buffer.begin(),buffer.begin()+n);}
            else{REQUIRE((error==asio::error::would_block || error==asio::error::try_again));}
            while(received.size()>=4){
                const auto length=aibridge::decodeLength(std::span<const uint8_t,4>(received.data(),4),aibridge::maxResponseBytes);
                if(received.size()<length+4){break;}
                responses.push_back(aibridge::strictParse(std::string_view(reinterpret_cast<const char *>(received.data()+4),length)));
                received.erase(received.begin(),received.begin()+length+4);
            }
            if(responses.size()<count){std::this_thread::sleep_for(1ms);}
        }
        REQUIRE(responses.size()==count);
        return responses;
    };
    send({{{"v",1},{"id","hello"},{"method","hello"},{"params",{{"instance_id",instance.at("instance_id")},{"token",instance.at("token")}}}}});
    REQUIRE(collect(1).front().at("ok")==true);
    const auto grant=[&](std::string mode,std::string request){
        return test.controller().handleRequest({1,{{"v",1},{"id","grant"},{"method","game_grant_control"},
            {"params",{{"mode",mode},{"grant_id","00000000-0000-4000-8000-000000000100"},
                {"request_id",request},{"character_uid",std::to_string(test.hero().UID())},{"context",test.controller().context()}}}}});
    };
    REQUIRE(grant("new_task","00000000-0000-4000-8000-000000000101").at("ok")==true);
    REQUIRE(test.move(freeNeighbors(test).front()).at("ok")==true);
    REQUIRE(test.status().at("state")=="accepted");
    const auto before=test.controller().context();
    const Json interrupt={{"v",1},{"id","interrupt"},{"method","game_stop"},
        {"params",{{"interrupt_only",true},{"operation_id",test.operation},{"context",before}}}};
    std::vector<Json> requests={{{"v",1},{"id","ordinary-read"},{"method","game_get_session"},{"params",Json::object()}},interrupt};
    bool explicitStop=false;
    SECTION("targeted cancellation is processed before active advance"){}
    SECTION("explicit stop overtakes the targeted cancellation in the same batch"){
        requests.push_back({{"v",1},{"id","explicit"},{"method","game_stop"},{"params",Json::object()}});
        explicitStop=true;
    }
    SECTION("explicit stop stays revoked when sent before the targeted cancellation"){
        requests.insert(requests.begin(),Json{{"v",1},{"id","explicit"},{"method","game_stop"},{"params",Json::object()}});
        explicitStop=true;
    }
    send(requests);
    const auto responses=collect(requests.size());
    CAPTURE(responses);
    const auto stopped=std::find_if(responses.begin(),responses.end(),[&](const auto &response){
        return response.at("id")==(explicitStop ? "explicit" : "interrupt");
    });
    REQUIRE(stopped!=responses.end());REQUIRE(stopped->at("ok")==true);
    // Only an accepted, never-begun native operation reports exactly this stop set.
    CHECK(stopped->at("data").at("stopped")==Json::array({"queued"}));
    CHECK(stopped->at("data").at("operation").at("operation_id")==test.operation);
    CHECK(stopped->at("data").at("operation").at("state")=="cancelled");
    const auto interrupted=std::find_if(responses.begin(),responses.end(),[](const auto &response){
        return response.at("id")=="interrupt";
    });
    REQUIRE(interrupted!=responses.end());
    CHECK(interrupted->at("ok")==!explicitStop);
    if(explicitStop){CHECK(interrupted->at("error").at("code")=="stale_context");}
    else{CHECK(interrupted->at("data").at("paused")==false);}
    CHECK(test.status().at("state")=="cancelled");
    CHECK(test.status().at("side_effects_possible")==false);
    CHECK(test.controller().context()!=before);
    const auto continued=grant("continue_task","00000000-0000-4000-8000-000000000102");
    CHECK(continued.at("ok")==!explicitStop);
    if(explicitStop){CHECK(continued.at("error").at("code")=="paused");}
    CHECK(test.controller().controlState()==(explicitStop ? aibridge::ControlState::Paused : aibridge::ControlState::Active));
    for(int i=0;i<20;++i){test.runtime->poll();test.hero().update(100);}
    CHECK(test.hero().actionSendSequence()==0);
    CHECK(test.hero().actionTypeSendSequence(ACTION_MOVE)==0);
    CHECK(test.status().at("state")=="cancelled");
}

TEST_CASE("Combat real client stationary hit preserves the attack correction revision")
{
    resources();
    ProcessRun scene(online());
    auto &hero = *scene.getMyHero();
    settle(hero);
    const auto target = cow(scene), revision = scene.aiServerRevision();
    hero.beginAiIntent();
    hero.setAiActionGuard([&]{ return scene.aiServerRevision() == revision; });
    REQUIRE(scene.trackAttack(true, target));
    hero.update(100);
    REQUIRE(hero.actionTypeSendSequence(ACTION_ATTACK) == 1);
    receive(scene, hero.UID(), ActionHitted{.direction = DIR_RIGHT, .x = 430, .y = 108, .fromUID = target});
    CHECK(scene.aiServerRevision() == revision);
    settle(hero);
    REQUIRE(scene.trackAttack(true, target));
    hero.update(100);
    CHECK_FALSE(hero.aiActionRejected());
    CHECK(hero.actionTypeSendSequence(ACTION_ATTACK) == 2);
    hero.cancelAiIntent();
}

TEST_CASE("Combat real client stand and displaced hit remain authoritative corrections")
{
    resources();
    ProcessRun scene(online());
    auto &hero = *scene.getMyHero();
    settle(hero);
    const auto target = cow(scene), revision = scene.aiServerRevision();
    hero.beginAiIntent();
    hero.setAiActionGuard([&]{ return scene.aiServerRevision() == revision; });
    REQUIRE(scene.trackAttack(true, target));
    receive(scene, hero.UID(), ActionStand{.direction = DIR_RIGHT, .x = 430, .y = 108});
    CHECK(scene.aiServerRevision() == revision + 1);
    settle(hero);
    REQUIRE(scene.trackAttack(true, target));
    hero.update(100);
    CHECK(hero.aiActionRejected());
    CHECK(hero.actionTypeSendSequence(ACTION_ATTACK) == 0);
    hero.cancelAiIntent();
    receive(scene, hero.UID(), ActionHitted{.direction = DIR_RIGHT, .x = 429, .y = 108, .fromUID = target});
    CHECK(scene.aiServerRevision() == revision + 2);
    receive(scene, hero.UID(), ActionDie{.x = 430, .y = 108});
    CHECK(scene.aiServerRevision() == revision + 3);
    CHECK(scene.aiDeathKnown());
}

TEST_CASE("Combat real client hit at a moving endpoint remains a correction")
{
    resources();
    ProcessRun scene(online());
    auto &hero = *scene.getMyHero();
    settle(hero);
    std::optional<std::pair<int, int>> destination;
    for(int dx = -1; dx <= 1 && !destination; ++dx){
        for(int dy = -1; dy <= 1 && !destination; ++dy){
            if((dx || dy) && scene.canMove(true, 1, 430, 108, 430 + dx, 108 + dy)){
                destination = {430 + dx, 108 + dy};
            }
        }
    }
    REQUIRE(destination);
    const auto [x, y] = *destination;
    REQUIRE(hero.parseAction(ActionMove{.x = 430, .y = 108, .aimX = x, .aimY = y}));
    REQUIRE(hero.moveNextMotion());
    REQUIRE(hero.moving());
    const auto revision = scene.aiServerRevision();
    receive(scene, hero.UID(), ActionHitted{.direction = DIR_RIGHT, .x = x, .y = y});
    CHECK(scene.aiServerRevision() == revision + 1);
}

TEST_CASE("Combat real client cancellation cannot revive attack continuations after a hit")
{
    resources();
    ProcessRun scene(online());
    auto &hero = *scene.getMyHero();
    settle(hero);
    const auto target = cow(scene);
    hero.beginAiIntent();
    hero.setAiActionGuard([]{ return true; });
    REQUIRE(scene.trackAttack(true, target));
    hero.update(100);
    REQUIRE(hero.actionTypeSendSequence(ACTION_ATTACK) == 1);
    REQUIRE(hero.cancelAiIntent());
    receive(scene, hero.UID(), ActionHitted{.direction = DIR_RIGHT, .x = 430, .y = 108, .fromUID = target});
    for(int i = 0; i < 100; ++i){ hero.update(100); }
    CHECK(hero.actionTypeSendSequence(ACTION_ATTACK) == 1);
    CHECK(hero.aiMotionStopped());
}

TEST_CASE("Combat real client bounds search on the observed blocked six tile approach")
{
    resources();
    auto initial = online();
    initial.action = ActionStand{.direction = DIR_DOWN, .x = 441, .y = 114};
    ProcessRun scene(initial);
    auto &hero = *scene.getMyHero();
    settle(hero);
    try{
        hero.aiPlanMove(441, 120);
        FAIL("Observed blocked approach must not claim a path after exhausting the bounded search");
    }
    catch(const aibridge::ProtocolError &error){
        CHECK(error.code == "unavailable");
        CHECK(std::string(error.what()) == "Movement search budget exhausted");
    }
    CHECK(hero.actionSendSequence() == 0);
    CHECK(hero.x() == 441);
    CHECK(hero.y() == 114);
}

TEST_CASE("Combat actual adapter fails unsubmitted when the blocked approach exhausts its search")
{
    AdapterScene test(441, 114);
    const auto target = test.pig(441, 120);
    REQUIRE(test.attack(target).at("ok") == true);
    REQUIRE(test.status().at("state") == "accepted");
    test.controller().advance();
    const auto failed = test.status();
    CHECK(failed.at("state") == "failed");
    CHECK(failed.at("error").at("code") == "unavailable");
    CHECK(failed.at("side_effects_possible") == false);
    CHECK(failed.at("completion_scope").is_null());
    CHECK(failed.at("result").is_null());
    CHECK(failed.at("evidence").empty());
    CHECK(test.scene->getFocusUID(FOCUS_ATTACK) == 0);
    for(int i = 0; i < 100; ++i){ test.step(); }
    CHECK(test.hero().actionSendSequence() == 0);
    CHECK(test.hero().x() == 441);
    CHECK(test.hero().y() == 114);
    CHECK(test.status() == failed);
}

TEST_CASE("Combat actual adapter pursues a reachable target with real unit moves and attack sends")
{
    // This route was independently traversed by ordinary game_move_to in the live record.
    AdapterScene test(439, 116);
    const auto target = test.pig(431, 124);
    REQUIRE(test.attack(target).at("ok") == true);
    test.controller().advance();
    REQUIRE(test.status().at("state") == "running");
    for(int i = 0; i < 1000 && test.hero().actionTypeSendSequence(ACTION_ATTACK) < 2; ++i){
        const auto motion = test.hero().currMotion();
        if(motion->x != motion->endX || motion->y != motion->endY){
            CHECK(std::max(std::abs(motion->x - motion->endX), std::abs(motion->y - motion->endY)) == 1);
            CHECK(test.scene->groundValid(motion->endX, motion->endY));
        }
        test.step();
    }
    CAPTURE(test.status().dump());
    REQUIRE(test.hero().actionTypeSendSequence(ACTION_MOVE) > 1);
    REQUIRE(test.hero().actionTypeSendSequence(ACTION_ATTACK) == 2);
    CHECK(test.status().at("state") == "running");
    CHECK(std::max(std::abs(test.hero().x() - 431), std::abs(test.hero().y() - 124)) == 1);
    CHECK((test.hero().x() != 431 || test.hero().y() != 124));
    CHECK(test.status().at("completion_scope").is_null());
    health(*test.scene, target, 0);
    test.controller().advance();
    CHECK(test.status().at("state") == "succeeded");
    CHECK(test.status().at("completion_scope") == "observed_postcondition");
    CHECK(test.status().at("result").at("submitted") == true);
}

TEST_CASE("Combat actual adapter cancels pursuit on manual takeover and authoritative correction")
{
    AdapterScene test(439, 116);
    REQUIRE(test.attack(test.pig(431, 124)).at("ok") == true);
    test.controller().advance();
    for(int i = 0; i < 100 && test.hero().actionSendSequence() == 0; ++i){ test.step(); }
    REQUIRE(test.hero().actionTypeSendSequence(ACTION_MOVE) > 0);
    REQUIRE(test.hero().actionTypeSendSequence(ACTION_ATTACK) == 0);
    SECTION("manual takeover revokes queued pursuit"){
        test.controller().manualInput();
        // Controller distinguishes stopped local pursuit from an unretractable send.
        CHECK(test.status().at("state") == "cancelled");
        CHECK(test.status().at("cancel_requested") == true);
        CHECK(test.status().at("result").at("submitted") == true);
        CHECK(test.status().at("completion_scope").is_null());
        CHECK(test.controller().controlState() == aibridge::ControlState::Paused);
        CHECK(test.controller().pauseReason() == "manual_input");
    }
    SECTION("same position stand retains the correction guard"){
        const auto motion = test.hero().currMotion();
        receive(*test.scene, test.hero().UID(), ActionStand{
            .direction = motion->direction, .x = motion->endX, .y = motion->endY});
        test.controller().advance();
        CHECK(test.status().at("state") == "unknown");
        CHECK(test.status().at("error").at("code") == "unavailable");
    }
    CHECK(test.status().at("side_effects_possible") == true);
    const auto sends = test.hero().actionSendSequence();
    for(int i = 0; i < 100; ++i){ test.step(); }
    CHECK(test.hero().actionSendSequence() == sends);
    CHECK(test.scene->getFocusUID(FOCUS_ATTACK) == 0);
}

TEST_CASE("Combat actual adapter refuses unknown target health before creating pursuit")
{
    AdapterScene test(439, 116);
    const auto denied = test.attack(test.pig(431, 124, false));
    REQUIRE(denied.at("ok") == false);
    CHECK(denied.at("error").at("code") == "unavailable");
    CHECK_FALSE(test.controller().activeOperation());
    CHECK(test.hero().actionSendSequence() == 0);
    CHECK(test.scene->getFocusUID(FOCUS_ATTACK) == 0);
}

TEST_CASE("Combat scene retires dead or absent attack focus before MCP movement")
{
    AdapterScene test(430, 108);
    const auto target = test.pig(431, 108);
    test.scene->setFocusUID(FOCUS_ATTACK, target);
    const auto destination = freeNeighbors(test).front();
    const auto blocked = test.move(destination);
    REQUIRE(blocked.at("ok") == false);
    REQUIRE(blocked.at("error").at("code") == "busy");

    SECTION("health confirms the focused target is dead"){
        health(*test.scene, target, 0);
        REQUIRE(test.scene->findUID(target)->dead() == true);
    }
    SECTION("the server removes the focused target"){
        const SMOffline message{.UID = target, .mapUID = test.scene->mapUID()};
        test.scene->on_SM_OFFLINE(reinterpret_cast<const uint8_t *>(&message), sizeof(message));
        REQUIRE(test.scene->findUID(target) == nullptr);
    }
    test.scene->update(0);
    REQUIRE(test.scene->getFocusUID(FOCUS_ATTACK) == 0);
    REQUIRE(test.hero().actionSendSequence() == 0);
    REQUIRE(test.move(destination).at("ok") == true);
    for(int i = 0; i < 100 && test.status().at("state") != "succeeded"; ++i){ test.step(); }
    CHECK(test.status().at("state") == "succeeded");
    CHECK(test.hero().actionTypeSendSequence(ACTION_MOVE) == 1);
    CHECK(test.hero().actionTypeSendSequence(ACTION_ATTACK) == 0);
}

TEST_CASE("Combat scene preserves live and unknown attack focus ownership")
{
    AdapterScene test(430, 108);
    bool knownHealth = true;
    SECTION("a live target retains its focus"){}
    SECTION("unknown target health does not retire the focus"){ knownHealth = false; }
    const auto target = test.pig(431, 108, knownHealth);
    test.scene->setFocusUID(FOCUS_ATTACK, target);
    test.scene->update(0);
    REQUIRE(test.scene->getFocusUID(FOCUS_ATTACK) == target);
    const auto blocked = test.move(freeNeighbors(test).front());
    CHECK(blocked.at("ok") == false);
    CHECK(blocked.at("error").at("code") == "busy");
    CHECK_FALSE(test.controller().activeOperation());
    CHECK(test.hero().actionSendSequence() == 0);
}

TEST_CASE("Hit movement real GUI right click sends a native move during stationary hit")
{
    AdapterScene test(430, 108);
    const auto destination = freeNeighbors(test).front();
    enterStationaryHit(test);
    CHECK_FALSE(test.hero().aiCanStartMove());
    rightClickMove(test, destination);
    REQUIRE_FALSE(test.hero().stayIdle());
    REQUIRE(test.hero().currMotion()->type == MOTION_HITTED);
    REQUIRE(test.hero().currMotion()->frame == 0);
    test.hero().update(100);
    CHECK(test.hero().actionTypeSendSequence(ACTION_MOVE) == 1);
    CHECK(test.hero().currMotion()->type == MOTION_WALK);
    CHECK(test.hero().currMotion()->endX == destination.x);
    CHECK(test.hero().currMotion()->endY == destination.y);
}

TEST_CASE("Hit movement actual adapter queues one native move and waits for stopped arrival")
{
    AdapterScene test(430, 108);
    const auto destination = freeNeighbors(test).front();
    enterStationaryHit(test);
    CHECK_FALSE(test.hero().aiCanStartMove());
    const auto accepted = test.move(destination);
    CAPTURE(accepted.dump());
    REQUIRE(accepted.at("ok") == true);
    test.controller().advance();
    REQUIRE(test.status().at("state") == "running");
    REQUIRE_FALSE(test.hero().stayIdle());
    REQUIRE(test.hero().currMotion()->type == MOTION_HITTED);
    SECTION("several ticks keep one pending native action"){
        for(int i = 0; i < 20; ++i){ test.controller().advance(); }
        CHECK_FALSE(test.hero().stayIdle());
        CHECK(test.hero().actionSendSequence() == 0);
    }
    SECTION("repeated native hit messages do not multiply outgoing moves"){
        for(int i = 0; i < 3; ++i){
            const auto motion = test.hero().currMotion();
            receive(*test.scene, test.hero().UID(), ActionHitted{
                .direction = motion->direction, .x = motion->endX, .y = motion->endY});
            test.controller().advance();
        }
        CHECK(test.hero().actionSendSequence() == 0);
    }
    for(int i = 0; i < 20 && test.hero().actionSendSequence() == 0; ++i){ test.step(); }
    REQUIRE(test.hero().actionTypeSendSequence(ACTION_MOVE) == 1);
    CHECK(test.hero().currMotion()->type == MOTION_WALK);
    CHECK(test.status().at("state") == "running");
    CHECK(test.status().at("completion_scope").is_null());
    for(int i = 0; i < 100 && test.status().at("state") == "running"; ++i){ test.step(); }
    REQUIRE(test.status().at("state") == "succeeded");
    CHECK(test.status().at("completion_scope") == "observed_postcondition");
    CHECK(test.hero().aiMotionStopped());
    CHECK(test.hero().x() == destination.x);
    CHECK(test.hero().y() == destination.y);
    CHECK(test.hero().actionTypeSendSequence(ACTION_MOVE) == 1);
    CHECK(test.hero().actionTypeSendSequence(ACTION_ATTACK) == 0);
}

TEST_CASE("Hit movement actual adapter preserves life forced queue human queue and focus gates")
{
    AdapterScene test(430, 108);
    const auto neighbors = freeNeighbors(test);
    enterStationaryHit(test);
    std::string expected = "busy";
    bool forced = false, humanQueued = false;
    SECTION("unknown life remains unavailable to an active adapter"){
        test.hero().getSDHealth().reset();
        expected = "paused";
    }
    SECTION("known death remains paused"){
        health(*test.scene, test.hero().UID(), 0);
        test.runtime->observe(test.scene->aiState());
        expected = "paused";
    }
    SECTION("native forced death queue cannot be displaced by movement"){
        // Isolate the real Hero forced queue before the controller observes death.
        REQUIRE(test.hero().parseAction(ActionDie{.x = 430, .y = 108}));
        REQUIRE_FALSE(test.hero().stayIdle());
        forced = true;
    }
    SECTION("a pending ordinary human move is not overwritten"){
        test.hero().emplaceAction(ActionMove{.speed = SYS_DEFSPEED,
            .x = 430, .y = 108, .aimX = neighbors[1].x, .aimY = neighbors[1].y});
        REQUIRE_FALSE(test.hero().stayIdle());
        humanQueued = true;
    }
    SECTION("attack focus remains busy"){
        test.scene->setFocusUID(FOCUS_ATTACK, test.pig(431, 108));
    }
    SECTION("combat admission is unchanged during a hit"){
        const auto denied = test.attack(test.pig(431, 108));
        REQUIRE(denied.at("ok") == false);
        CHECK(denied.at("error").at("code") == "busy");
        CHECK_FALSE(test.controller().activeOperation());
        CHECK(test.hero().actionSendSequence() == 0);
        return;
    }
    const auto denied = test.move(neighbors[0]);
    REQUIRE(denied.at("ok") == false);
    CHECK(denied.at("error").at("code") == expected);
    CHECK_FALSE(test.controller().activeOperation());
    CHECK(test.hero().actionSendSequence() == 0);
    if(forced){
        test.hero().update(100);
        CHECK(test.hero().currMotion()->type == MOTION_DIE);
        CHECK(test.hero().actionSendSequence() == 0);
    }
    if(humanQueued){
        test.hero().update(100);
        CHECK(test.hero().actionTypeSendSequence(ACTION_MOVE) == 1);
        CHECK(test.hero().currMotion()->endX == neighbors[1].x);
        CHECK(test.hero().currMotion()->endY == neighbors[1].y);
    }
}

TEST_CASE("Hit movement actual adapter revokes queued native moves before a send")
{
    AdapterScene test(430, 108);
    const auto destination = freeNeighbors(test).front();
    enterStationaryHit(test);
    REQUIRE(test.move(destination, 100).at("ok") == true);
    test.controller().advance();
    REQUIRE(test.status().at("state") == "running");
    REQUIRE_FALSE(test.hero().stayIdle());
    SECTION("manual takeover cancels pending move"){
        test.controller().manualInput();
        CHECK(test.status().at("state") == "cancelled");
    }
    SECTION("world epoch change cancels pending move"){
        auto changed = test.scene->aiState();
        changed.mapUid = uidf::getBaseMapUID(DBCOM_MAPID(u8"比奇县_0"), 0);
        test.runtime->observe(changed);
        CHECK(test.status().at("state") == "cancelled");
    }
    SECTION("authoritative correction clears the pending native move"){
        receive(*test.scene, test.hero().UID(), ActionStand{.direction = DIR_DOWN, .x = 430, .y = 108});
        test.controller().advance();
    }
    SECTION("deadline cancels the pending native move"){
        SDL_Delay(110);
        test.controller().checkDeadlines();
        CHECK(test.status().at("state") == "failed");
        CHECK(test.status().at("error").at("code") == "timeout");
    }
    for(int i = 0; i < 100; ++i){ test.step(); }
    CHECK(test.hero().actionSendSequence() == 0);
    CHECK(test.hero().x() == 430);
    CHECK(test.hero().y() == 108);
    CHECK(test.status().at("completion_scope").is_null());
}

TEST_CASE("Hit movement actual adapter does not continue a sent route after correction")
{
    AdapterScene test(430, 108);
    const auto destination = freeNeighbors(test).front();
    enterStationaryHit(test);
    REQUIRE(test.move(destination).at("ok") == true);
    test.controller().advance();
    test.step();
    REQUIRE(test.hero().actionTypeSendSequence(ACTION_MOVE) == 1);
    const auto revision = test.scene->aiServerRevision();
    receive(*test.scene, test.hero().UID(), ActionStand{.direction = DIR_DOWN, .x = 430, .y = 108});
    REQUIRE(test.scene->aiServerRevision() != revision);
    for(int i = 0; i < 100; ++i){ test.step(); }
    CHECK(test.hero().actionTypeSendSequence(ACTION_MOVE) == 1);
    CHECK(test.hero().x() == 430);
    CHECK(test.hero().y() == 108);
    CHECK(test.status().at("state") != "succeeded");
    CHECK(test.status().at("completion_scope").is_null());
}
