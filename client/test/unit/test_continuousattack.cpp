#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include "bgmusicdb.hpp"
#include "client.hpp"
#include "clientargparser.hpp"
#include "emojidb.hpp"
#include "fontexdb.hpp"
#include "imeboard.hpp"
#include "log.hpp"
#include "magicdb.hpp"
#include "mapbindb.hpp"
#include "messagestackboard.hpp"
#include "pngtexdb.hpp"
#include "pngtexoffdb.hpp"
#include "processrun.hpp"
#include "progusedb.hpp"
#include "soundeffectdb.hpp"

ClientArgParser   *g_clientArgParser = nullptr;
Log               *g_mir2xLog        = nullptr;
ProgUseDB         *g_progUseDB       = nullptr;
PNGTexDB          *g_itemDB          = nullptr;
PNGTexDB          *g_mapDB           = nullptr;
PNGTexOffDB       *g_heroDB          = nullptr;
PNGTexOffDB       *g_hairDB          = nullptr;
PNGTexOffDB       *g_monsterDB       = nullptr;
PNGTexOffDB       *g_weaponDB        = nullptr;
PNGTexOffDB       *g_helmetDB        = nullptr;
PNGTexOffDB       *g_equipDB         = nullptr;
MagicDB           *g_magicDB         = nullptr;
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

TEST_CASE("Continuous attack respects hit frames, cancellation, targets, and shortcuts", "[continuousattack]")
{
    const char *resourceOverride = std::getenv("MIR2X_TEST_RES_DIR");
    std::string resourceOption = std::string("--res-path=") + (resourceOverride ? resourceOverride : MIR2X_TEST_DEFAULT_RES_DIR);
    char name[] = "continuousattack_test";
    char audio[] = "--disable-audio";
    char profiler[] = "--disable-profiler";
    char *options[] = {name, audio, profiler, resourceOption.data()};
    const argf::parser parser(4, options);
    ClientArgParser clientArgs(parser);
    g_clientArgParser = &clientArgs;
    logDisableProfiler();
    Log log("mir2x-continuousattack-test", ".");
    g_mir2xLog = &log;
    SDLDevice device;
    g_sdlDevice = &device;
    ProgUseDB progUse(1024);
    g_progUseDB = &progUse;
    PNGTexDB items(1024);
    g_itemDB = &items;
    PNGTexDB mapTextures(8192);
    g_mapDB = &mapTextures;
    PNGTexOffDB heroes(1024);
    g_heroDB = &heroes;
    PNGTexOffDB hair(1024);
    g_hairDB = &hair;
    PNGTexOffDB monsters(1024);
    g_monsterDB = &monsters;
    PNGTexOffDB weapons(1024);
    g_weaponDB = &weapons;
    PNGTexOffDB helmets(1024);
    g_helmetDB = &helmets;
    PNGTexOffDB equipment(1024);
    g_equipDB = &equipment;
    MagicDB magic(1024);
    g_magicDB = &magic;
    PNGTexOffDB npcs(1024);
    g_standNPCDB = &npcs;
    PNGTexOffDB selection(512);
    g_selectCharDB = &selection;
    FontexDB fonts(64 * 1024 * 1024);
    g_fontexDB = &fonts;
    MapBinDB maps;
    g_mapBinDB = &maps;
    EmojiDB emoji;
    g_emojiDB = &emoji;
    BGMusicDB music(5);
    g_bgmDB = &music;
    SoundEffectDB sounds(128);
    g_seffDB = &sounds;
    Client client;
    g_client = &client;
    IMEBoard ime{{}};
    g_imeBoard = &ime;
    MessageStackBoard notifications{{.width = 400, .showTime = 0, .entryLimit = 5}};
    g_notifyBoard = &notifications;

    const auto mapUID = uidf::getBaseMapUID(23, 0);
    const auto map = maps.retrieve(23);
    REQUIRE(map);
    const auto origin = [&]() -> std::tuple<int, int>
    {
        for(int gridY = 2; gridY + 2 < to_d(map->h()); ++gridY){
            for(int gridX = 2; gridX + 2 < to_d(map->w()); ++gridX){
                bool clear = true;
                for(int offsetY = -1; offsetY <= 1; ++offsetY){
                    for(int offsetX = -1; offsetX <= 1; ++offsetX){
                        clear = clear && map->cell(gridX + offsetX, gridY + offsetY).land.canThrough();
                    }
                }
                if(clear){
                    return {gridX, gridY};
                }
            }
        }
        throw fflpanic("No open combat test location");
    }();
    const auto [originX, originY] = origin;
    SMOnlineOK online{};
    online.uid = uidf::getPlayerUID(1);
    online.name.assign("combat-test");
    online.job = JOB_WARRIOR;
    online.mapUID = mapUID;
    online.action = ActionStand{.direction = DIR_DOWN, .x = originX, .y = originY};
    ProcessRun scene(online);
    auto hero = scene.getMyHero();
    hero->updateHealth(SDHealth{.uid = online.uid, .hp = 100, .mp = 100});
    hero->moveNextMotion();
    auto settings = dynamic_cast<RuntimeConfigBoard *>(scene.getWidget("RuntimeConfigBoard"));
    const auto monsterUID = uidf::buildMonsterUID(DBCOM_MONSTERID(u8"稻草人"), 0);
    const auto otherUID = uidf::buildMonsterUID(DBCOM_MONSTERID(u8"稻草人"), 0);
    const auto spawn = [&](uint64_t targetUID, int gridX, int gridY)
    {
        const SMAction message{.UID = targetUID, .mapUID = mapUID, .action = ActionStand{.direction = DIR_UP, .x = gridX, .y = gridY}};
        scene.on_SM_ACTION(reinterpret_cast<const uint8_t *>(&message), sizeof(message));
        REQUIRE(scene.findUID(targetUID));
    };
    spawn(monsterUID, originX + 1, originY);
    spawn(otherUID, originX, originY + 1);

    const auto beginHit = [&](uint64_t targetUID)
    {
        REQUIRE(scene.trackAttack(true, targetUID));
        REQUIRE(hero->parseActionQueue());
        const SMAction message{.UID = online.uid, .mapUID = mapUID, .action = ActionHitted{.x = originX, .y = originY, .fromUID = monsterUID}};
        scene.on_SM_ACTION(reinterpret_cast<const uint8_t *>(&message), sizeof(message));
        REQUIRE(hero->moveNextMotion());
        REQUIRE(hero->currMotion()->type == MOTION_HITTED);
        REQUIRE(hero->currMotion()->frame == 0);
    };
    const auto finishHit = [&]
    {
        const auto frameCount = hero->getFrameCountEx(hero->currMotion());
        for(int frame = 0; frame < frameCount; ++frame){
            REQUIRE(hero->currMotion()->type == MOTION_HITTED);
            REQUIRE(hero->currMotion()->frame == frame);
            REQUIRE(hero->stayIdle());
            hero->currMotion()->runTrigger();
            REQUIRE(hero->stayIdle());
            REQUIRE(hero->updateMotion(true));
        }
    };

    beginHit(monsterUID);
    finishHit();
    REQUIRE(scene.getFocusUID(FOCUS_ATTACK) == monsterUID);
    REQUIRE(hero->currMotion()->type != MOTION_HITTED);
    REQUIRE(hero->currMotion()->type != MOTION_STAND);

    settings->setContinuousAttack(false);
    beginHit(monsterUID);
    finishHit();
    REQUIRE(hero->currMotion()->type == MOTION_STAND);
    REQUIRE(scene.getFocusUID(FOCUS_ATTACK) == 0);
    settings->setContinuousAttack(true);

    beginHit(monsterUID);
    REQUIRE(hero->emplaceAction(ActionMove{.speed = SYS_DEFSPEED, .x = originX, .y = originY, .aimX = originX - 1, .aimY = originY}));
    REQUIRE(scene.getFocusUID(FOCUS_ATTACK) == 0);
    REQUIRE_FALSE(scene.trackAttack(false, monsterUID));
    hero->clearActionQueue();
    finishHit();
    REQUIRE(hero->currMotion()->type == MOTION_STAND);

    beginHit(monsterUID);
    REQUIRE(scene.trackAttack(true, otherUID));
    REQUIRE_FALSE(scene.trackAttack(false, monsterUID));
    REQUIRE(scene.getFocusUID(FOCUS_ATTACK) == otherUID);
    REQUIRE(hero->moveNextMotion());
    REQUIRE(hero->currMotion()->type != MOTION_HITTED);
    REQUIRE(hero->currMotion()->type != MOTION_STAND);
    REQUIRE(scene.getFocusUID(FOCUS_ATTACK) == otherUID);

    beginHit(monsterUID);
    const SMCODisappear disappearance{.UID = monsterUID, .mapUID = mapUID};
    scene.on_SM_CODISAPPEAR(reinterpret_cast<const uint8_t *>(&disappearance), sizeof(disappearance));
    REQUIRE(scene.getFocusUID(FOCUS_ATTACK) == 0);
    spawn(monsterUID, originX + 1, originY);
    finishHit();
    REQUIRE(hero->currMotion()->type == MOTION_STAND);

    beginHit(monsterUID);
    REQUIRE(scene.trackAttack(true, monsterUID));
    const SMNotifyDead death{.UID = monsterUID};
    scene.on_SM_NOTIFYDEAD(reinterpret_cast<const uint8_t *>(&death), sizeof(death));
    REQUIRE(scene.getFocusUID(FOCUS_ATTACK) == 0);
    REQUIRE(hero->stayIdle());
    finishHit();
    REQUIRE(hero->currMotion()->type == MOTION_STAND);

    beginHit(otherUID);
    hero->flushForcedMotion();
    REQUIRE(scene.getFocusUID(FOCUS_ATTACK) == 0);
    REQUIRE(hero->currMotion()->type == MOTION_STAND);

    SMCORecord player{};
    player.UID = uidf::getPlayerUID(2);
    player.mapUID = mapUID;
    player.action = ActionStand{.direction = DIR_UP, .x = originX + 1, .y = originY - 1};
    player.Player.job = JOB_WARRIOR;
    scene.on_SM_CORECORD(reinterpret_cast<const uint8_t *>(&player), sizeof(player));
    beginHit(player.UID);
    finishHit();
    REQUIRE(hero->currMotion()->type == MOTION_STAND);
    REQUIRE(scene.getFocusUID(FOCUS_ATTACK) == 0);

    SDL_Event shortcut{};
    shortcut.type = SDL_EVENT_KEY_DOWN;
    shortcut.key.key = SDLK_TAB;
    shortcut.key.mod = SDL_KMOD_LSHIFT;
    scene.processEvent(shortcut);
    REQUIRE_FALSE(scene.getRuntimeConfig<RTCFG_持续攻击>());
    shortcut.key.repeat = true;
    scene.processEvent(shortcut);
    REQUIRE_FALSE(scene.getRuntimeConfig<RTCFG_持续攻击>());
    shortcut.key.repeat = false;
    shortcut.key.mod = SDL_KMOD_NONE;
    scene.processEvent(shortcut);
    REQUIRE_FALSE(scene.getRuntimeConfig<RTCFG_持续攻击>());
    shortcut.key.mod = SDL_KMOD_RSHIFT;
    scene.processEvent(shortcut);
    REQUIRE(scene.getRuntimeConfig<RTCFG_持续攻击>());

    beginHit(otherUID);
    REQUIRE(hero->parseAction(ActionDie{.x = originX, .y = originY}));
    REQUIRE(scene.getFocusUID(FOCUS_ATTACK) == 0);
    REQUIRE(hero->moveNextMotion());
    REQUIRE(hero->currMotion()->type == MOTION_DIE);
}
