#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
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

        AdapterScene(int x, int y)
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
            { std::ofstream out(policy); out << R"({"allowed_actions":["game_attack"]})"; }
            // Only the bridge listener is started, in a dedicated test directory.
            // Requests enter the real controller directly; game NetIO remains unpolled.
            runtime = std::make_unique<aibridge::AiBridgeRuntime>(*fixture.client);
            runtime->start({root / "bridge", policy, "combat-gui-test"});
            runtime->observe(scene->aiState());
            controller().onConnected(1);
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
        void step()
        {
            hero().update(100);
            controller().advance();
        }
    };
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
