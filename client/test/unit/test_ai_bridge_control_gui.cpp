#include <catch2/catch_test_macros.hpp>
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
        options.policy=aibridge::Policy::parse(R"({"allowed_actions":"*"})");
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
}

TEST_CASE("Real bottom-right AI panel disables startup control and retains its original identity")
{
    ControlGuiFixture fixture;
    using namespace aibridge;
    AiBridgeController controller(startupOptions());
    bool connected=true;
    AiControlPanel panel([&]{return &controller;},[&]{return connected;});
    controller.onConnected(1); controller.observe({"run",100,11,true});
    REQUIRE(controller.controlState()==ControlState::Active);
    const auto context=controller.context(); const auto lifetime=controller.lifetime().lock();
    panel.refresh(); panel.draw();
    const auto image=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>(
        SDL_RenderReadPixels(g_sdlDevice->getRenderer(),nullptr),SDL_DestroySurface);
    REQUIRE(image);
    REQUIRE(IMG_SavePNG(image.get(),"ai-control-panel.png"));
    CHECK(panel.status().find("AI")!=std::string::npos);
    const auto [width,height]=g_sdlDevice->getWindowLogicalSize();
    const auto bounds=panel.bounds();
    CHECK(bounds.x+bounds.w==width-12);
    CHECK(bounds.y+bounds.h==height-162); // Above, rather than on top of, the original HUD.
    click(panel,panel.disableBounds());
    REQUIRE(controller.controlState()==ControlState::Disabled);
    for(int i=0;i<5;++i){ controller.observe({"run",100,11,true}); panel.refresh(); panel.draw(); }
    CHECK(controller.controlState()==ControlState::Disabled);
    CHECK(controller.context()!=context);
    const auto result=controller.handleRequest({1,{{"v",1},{"id","session"},{"method","game_get_session"},{"params",Json::object()}}});
    CHECK(result.at("ok")==true);
    CHECK(result.at("data").at("control_state")=="disabled");
    const auto stop=controller.handleRequest({1,{{"v",1},{"id","stop"},{"method","game_stop"},{"params",Json::object()}}});
    CHECK(stop.at("ok")==true); CHECK(controller.controlState()==ControlState::Disabled);
    click(panel,panel.enableBounds());
    CHECK(controller.controlState()==ControlState::Active);
    CHECK(controller.lifetime().lock()==lifetime);
    CHECK(controller.context().at("session_id")==context.at("session_id"));
    connected=false; controller.onDisconnected(1); panel.refresh();
    CHECK(panel.status().find("未连接")!=std::string::npos);
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
    const auto rect=panel.disableBounds();
    for(const auto type:{SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP}){
        auto event=pointerEvent(type,rect);
        REQUIRE(SDL_RenderCoordinatesToWindow(g_sdlDevice->getRenderer(),event.button.x,event.button.y,&event.button.x,&event.button.y));
        g_sdlDevice->scaleEvent(event);
        REQUIRE(panel.processEvent(event));
    }
    REQUIRE(controller.controlState()==ControlState::Disabled);
    REQUIRE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_DOWN,panel.enableBounds())));
    g_sdlDevice->scaleWindow({1100,760},1.5f);
    REQUIRE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_UP,panel.enableBounds())));
    CHECK(controller.controlState()==ControlState::Disabled);
    click(panel,panel.enableBounds());
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
    click(panel,panel.enableBounds());
    CHECK(controller.controlState()!=ControlState::Active);
    CHECK(controller.pauseReason()=="controller_error");
    panel.close("Internal fault"); panel.draw();
    CHECK_FALSE(panel.processEvent(pointerEvent(SDL_EVENT_MOUSE_BUTTON_DOWN,panel.enableBounds())));
    CHECK(controller.pauseReason()=="controller_error");
}

