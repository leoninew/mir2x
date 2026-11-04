#include <cstdlib>
#include <SDL3_image/SDL_image.h>
#include "clientargparser.hpp"
#include "fontexdb.hpp"
#include "log.hpp"
#include "sdldevice.hpp"
#include "pngtexdb.hpp"
#include "layoutboard.hpp"
#include "inputstringboard.hpp"
#include "uidf.hpp"
#include "aibridge/controlpanel.hpp"
#include "aibridge/controller.hpp"
#include "dbcomid.hpp"
#undef CHECK
#include <catch2/catch_test_macros.hpp>
class PNGTexDB;
class EmojiDB;
class PNGTexOffDB;
class MapBinDB;
class BGMusicDB;
class SoundEffectDB;
class IMEBoard;
class MessageStackBoard;
class Client;

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

namespace
{
    struct TestFonts: FontexDB
    {
        TestFonts(): FontexDB(1024 * 1024 * 64) {}
        ~TestFonts() override { clear(); }
    };
    struct EmptyImages: PNGTexDB
    {
        EmptyImages(): PNGTexDB(16) {}
        std::optional<std::tuple<PNGTexElement, size_t>> loadResource(uint32_t) override { return {}; }
    };
    struct ControlGuiFixture
    {
        std::unique_ptr<ClientArgParser> args;
        std::unique_ptr<Log> log;
        std::unique_ptr<SDLDevice> device;
        std::unique_ptr<TestFonts> fonts;
        std::unique_ptr<EmptyImages> images;
        ControlGuiFixture()
        {
            char name[]="control_gui_test", audio[]="--disable-audio", profiler[]="--disable-profiler";
            char *argv[]={name,audio,profiler};
            const argf::parser parser(3,argv);
            args=std::make_unique<ClientArgParser>(parser); g_clientArgParser=args.get();
            logDisableProfiler();
            log=std::make_unique<Log>("mir2x-control-gui-test","."); g_mir2xLog=log.get();
            device=std::make_unique<SDLDevice>(); g_sdlDevice=device.get(); device->createMainWindow();
            fonts=std::make_unique<TestFonts>(); g_fontexDB=fonts.get();
            const char *overridePath=std::getenv("MIR2X_TEST_RES_DIR");
            const std::string res=overridePath ? overridePath : MIR2X_TEST_DEFAULT_RES_DIR;
            fonts->load((res+"/font/fontex.zsdb").c_str());
            images=std::make_unique<EmptyImages>(); g_progUseDB=images.get();
        }
        ~ControlGuiFixture()
        {
            images.reset(); fonts.reset(); device.reset(); log.reset(); args.reset();
            g_progUseDB=nullptr; g_fontexDB=nullptr; g_sdlDevice=nullptr; g_mir2xLog=nullptr; g_clientArgParser=nullptr;
        }
    };
}

namespace
{
    aibridge::ControllerOptions startupOptions()
    {
        aibridge::ControllerOptions options;
        options.instanceId=std::string(32,'a'); options.build="control-gui-test";
        options.policy=aibridge::Policy::parse(R"({"allowed_actions":["game_move_to","game_stop"]})");
        options.newSessionId=[]{return std::string(32,'b');};
        return options;
    }
    SDL_Event pointerEvent(Uint32 type, SDL_FRect rect)
    {
        SDL_Event event{};
        event.type=type;
        event.button.windowID=SDL_GetWindowID(SDL_GetRenderWindow(g_sdlDevice->getRenderer()));
        event.button.button=SDL_BUTTON_LEFT;
        event.button.x=rect.x+rect.w/2; event.button.y=rect.y+rect.h/2;
        return event;
    }
    void click(aibridge::AiControlPanel &panel,SDL_FRect rect)
    {
        REQUIRE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_DOWN,rect)));
        REQUIRE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_UP,rect)));
    }
    void savePanel(aibridge::AiControlPanel &panel,const char *path)
    {
        REQUIRE(SDL_SetRenderDrawColor(g_sdlDevice->getRenderer(),0,0,0,255));
        REQUIRE(SDL_RenderClear(g_sdlDevice->getRenderer()));
        panel.draw();
        const auto image=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>(
            SDL_RenderReadPixels(g_sdlDevice->getRenderer(),nullptr),SDL_DestroySurface);
        REQUIRE(image);
        REQUIRE(IMG_SavePNG(image.get(),path));
    }
}

TEST_CASE("Caller grant mode retains manual authority and accepts a fresh GUI enable gesture")
{
    ControlGuiFixture fixture;
    using namespace aibridge;
    auto options=startupOptions(); options.policy=Policy::parse(R"({"allowed_actions":"*"})");
    AiBridgeController controller(options);
    AiControlPanel panel([&]{return &controller;},[]{return true;});
    controller.onConnected(1); controller.observe({"run",100,11,true});
    REQUIRE(controller.controlState()!=ControlState::Active);
    const auto grant=[&](int n){
        const std::string id="00000000-0000-4000-8000-00000000000"+std::to_string(n);
        return controller.handleRequest({1,{{"v",1},{"id","grant"},{"method","game_grant_control"},
            {"params",{{"mode","new_task"},{"grant_id",id},{"request_id",id},{"character_uid","100"},{"context",controller.context()}}}}});
    };
    REQUIRE(grant(1).at("ok")==true);
    controller.manualInput(); panel.refresh();
    CHECK(controller.controlState()==ControlState::Active);
    CHECK(panel.status()=="AI接管中");
    click(panel,panel.bounds());
    REQUIRE(controller.controlState()==ControlState::Disabled);
    click(panel,panel.bounds());
    REQUIRE(controller.controlState()==ControlState::Active);
    controller.manualInput();
    CHECK(controller.controlState()==ControlState::Active);
    REQUIRE(grant(2).at("ok")==true);
    panel.refresh(); CHECK(panel.status()=="AI接管中");
}

TEST_CASE("Compact top-left AI panel toggles control and retains its original identity")
{
    ControlGuiFixture fixture;
    using namespace aibridge;
    AiBridgeController controller(startupOptions());
    bool connected=true;
    AiControlPanel panel([&]{return &controller;},[&]{return connected;});
    controller.onConnected(1); controller.observe({"run",100,11,true});
    REQUIRE(controller.controlState()==ControlState::Active);
    const auto context=controller.context(); const auto lifetime=controller.lifetime().lock();
    panel.refresh();
    savePanel(panel,"ai-control-panel-status-active.png");
    CHECK(panel.status()=="AI接管中");
    const auto bounds=panel.bounds();
    CHECK(bounds.x==12);
    CHECK(bounds.y==12);
    CHECK(bounds.w>=60);
    CHECK(bounds.w<=100);
    CHECK(bounds.h>=24);
    CHECK(bounds.h<=40);
    click(panel,panel.bounds());
    REQUIRE(controller.controlState()==ControlState::Disabled);
    CHECK(panel.status()=="AI未接管");
    savePanel(panel,"ai-control-panel-status-inactive.png");
    for(int i=0;i<5;++i){ controller.observe({"run",100,11,true}); panel.refresh(); panel.draw(); }
    CHECK(controller.controlState()==ControlState::Disabled);
    CHECK(controller.context()!=context);
    const auto result=controller.handleRequest({1,{{"v",1},{"id","session"},{"method","game_get_session"},{"params",Json::object()}}});
    CHECK(result.at("ok")==true);
    CHECK(result.at("data").at("control_state")=="disabled");
    const auto stop=controller.handleRequest({1,{{"v",1},{"id","stop"},{"method","game_stop"},{"params",Json::object()}}});
    CHECK(stop.at("ok")==true); CHECK(controller.controlState()==ControlState::Disabled);
    click(panel,panel.bounds());
    CHECK(controller.controlState()==ControlState::Active);
    CHECK(panel.status()=="AI接管中");
    CHECK(controller.lifetime().lock()==lifetime);
    CHECK(controller.context().at("session_id")==context.at("session_id"));
    connected=false; controller.onDisconnected(1); panel.refresh();
    CHECK(panel.status()=="AI未接管");
    controller.onConnected(2); connected=true; panel.refresh();
    CHECK(controller.controlState()==ControlState::Paused);
}

TEST_CASE("Real AI button hit testing uses logical coordinates once and invalidates resize gestures")
{
    ControlGuiFixture fixture;
    using namespace aibridge;
    AiBridgeController controller(startupOptions());
    controller.onConnected(1); controller.observe({"run",100,11,true});
    AiControlPanel panel([&]{return &controller;},[]{return true;});
    g_sdlDevice->scaleWindow({960,720},1.5f);
    const auto rect=panel.bounds();
    for(const auto type:{SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP}){
        auto event=pointerEvent(type,rect);
        REQUIRE(SDL_RenderCoordinatesToWindow(g_sdlDevice->getRenderer(),event.button.x,event.button.y,&event.button.x,&event.button.y));
        g_sdlDevice->scaleEvent(event);
        REQUIRE(panel.processEvent(event));
    }
    REQUIRE(controller.controlState()==ControlState::Disabled);
    REQUIRE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_DOWN,panel.bounds())));
    g_sdlDevice->scaleWindow({1100,760},1.5f);
    REQUIRE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_UP,panel.bounds())));
    CHECK(controller.controlState()==ControlState::Disabled);
    CHECK(panel.bounds().x==12);
    CHECK(panel.bounds().y==12);
    click(panel,panel.bounds());
    CHECK(controller.controlState()==ControlState::Active);
    auto ordinary=pointerEvent(SDL_EVENT_MOUSE_BUTTON_DOWN,{10,10,1,1});
    CHECK_FALSE(panel.processEvent(ordinary));
    CHECK(ControlGesture::manual(ordinary)); // Runtime forwards this to manual takeover and the game.
}

TEST_CASE("Real AI button cannot turn an internal controller fault back into active control")
{
    ControlGuiFixture fixture;
    using namespace aibridge;
    AiBridgeController controller(startupOptions());
    controller.onConnected(1); controller.observe({"run",100,11,true});
    AiControlPanel panel([&]{return &controller;},[]{return true;});
    controller.registerAction("game_move_to",{
        [](const Json &,const Json &){return ValidationResult{};},
        [](const OperationView &)->OperationUpdate{throw std::runtime_error("internal");},
        [](const OperationView &)->std::optional<OperationUpdate>{return {};},
        [](const OperationView &){return OperationUpdate{};}});
    const auto accepted=controller.handleRequest({1,{{"v",1},{"id","move"},{"method","game_move_to"},
        {"params",{{"x",10},{"y",20}}},{"request_id","00000000-0000-4000-8000-000000000001"},
        {"context",controller.context()},{"timeout_ms",1000}}});
    REQUIRE(accepted.at("ok")==true); controller.advance(); panel.refresh();
    CHECK(controller.pauseReason()=="controller_error");
    click(panel,panel.bounds());
    CHECK(controller.controlState()!=ControlState::Active);
    CHECK(controller.pauseReason()=="controller_error");
    panel.close("Internal fault"); panel.draw();
    CHECK(panel.status()=="AI未接管");
    CHECK_FALSE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_DOWN,panel.bounds())));
    CHECK(controller.pauseReason()=="controller_error");
}

TEST_CASE("Compact AI toggle resumes manual pause but still requires a connected living role")
{
    ControlGuiFixture fixture;
    using namespace aibridge;
    auto options=startupOptions();
    options.autoEnable=false;
    AiBridgeController controller(std::move(options));
    bool connected=false;
    AiControlPanel panel([&]{return &controller;},[&]{return connected;});
    controller.observe({"run",100,11,true}); panel.refresh();
    CHECK(panel.status()=="AI未接管");
    click(panel,panel.bounds());
    CHECK(controller.controlState()!=ControlState::Active);
    controller.onConnected(1); connected=true;
    click(panel,panel.bounds());
    REQUIRE(controller.controlState()==ControlState::Active);
    controller.manualInput(); panel.refresh();
    CHECK(panel.status()=="AI未接管");
    click(panel,panel.bounds());
    REQUIRE(controller.controlState()==ControlState::Active);
    RuntimeState unknown{"run",100,11,false};
    unknown.observeLife(std::nullopt);
    controller.observe(unknown); panel.refresh();
    click(panel,panel.bounds());
    CHECK(controller.controlState()!=ControlState::Active);
    CHECK(panel.status()=="AI未接管");
    controller.observe({"run",100,11,false}); panel.refresh();
    click(panel,panel.bounds());
    CHECK(controller.controlState()!=ControlState::Active);
    controller.observe({"run",100,11,true}); panel.refresh();
    CHECK(panel.status()=="AI未接管");
    click(panel,panel.bounds());
    CHECK(controller.controlState()==ControlState::Active);
    CHECK(panel.status()=="AI接管中");
}

TEST_CASE("Compact AI toggle discards a click when its control action changes before release")
{
    ControlGuiFixture fixture;
    using namespace aibridge;
    AiBridgeController controller(startupOptions());
    controller.onConnected(1); controller.observe({"run",100,11,true});
    AiControlPanel panel([&]{return &controller;},[]{return true;});
    REQUIRE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_DOWN,panel.bounds())));
    controller.manualInput();
    REQUIRE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_UP,panel.bounds())));
    CHECK(controller.controlState()==ControlState::Paused);
    CHECK(panel.status()=="AI未接管");
    click(panel,panel.bounds());
    REQUIRE(controller.controlState()==ControlState::Active);
    REQUIRE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_DOWN,panel.bounds())));
    controller.disableByUser();
    REQUIRE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_UP,panel.bounds())));
    CHECK(controller.controlState()==ControlState::Disabled);
}

