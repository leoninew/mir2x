#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdlib>
#include <SDL3_image/SDL_image.h>
#include "bgmusicdb.hpp"
#include "client.hpp"
#include "clientargparser.hpp"
#include "emojidb.hpp"
#include "fontexdb.hpp"
#include "log.hpp"
#include "magicdb.hpp"
#include "mapbindb.hpp"
#include "messagestackboard.hpp"
#include "pngtexdb.hpp"
#include "pngtexoffdb.hpp"
#include "processcreateaccount.hpp"
#include "progusedb.hpp"
#include "soundeffectdb.hpp"

class IMEBoard;

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

TEST_CASE("Registration accepts clicks and Enter, preserves corrections, and prevents duplicate requests", "[createaccount]")
{
    struct ResetGlobals
    {
        ~ResetGlobals()
        {
            g_clientArgParser = nullptr;
            g_mir2xLog = nullptr;
            g_sdlDevice = nullptr;
            g_client = nullptr;
            g_notifyBoard = nullptr;
        }
    } resetGlobals;

    const char *resourceOverride = std::getenv("MIR2X_TEST_RES_DIR");
    std::string resourceOption = std::string("--res-path=") + (resourceOverride ? resourceOverride : MIR2X_TEST_DEFAULT_RES_DIR);
    char name[] = "createaccount_test";
    char audio[] = "--disable-audio";
    char profiler[] = "--disable-profiler";
    char *options[] = {name, audio, profiler, resourceOption.data()};
    const argf::parser parser(4, options);
    ClientArgParser clientArgs(parser);
    g_clientArgParser = &clientArgs;
    logDisableProfiler();
    Log log("mir2x-createaccount-test", ".");
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
    MessageStackBoard notifications{{.width = 400, .showTime = 0, .entryLimit = 5}};
    g_notifyBoard = &notifications;
    ProcessCreateAccount scene;

    const auto press = [&](SDL_Keycode key, SDL_Keymod modifiers = SDL_KMOD_NONE, bool repeat = false)
    {
        SDL_Event event {};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.key = key;
        event.key.mod = modifiers;
        event.key.repeat = repeat;
        scene.processEvent(event);
    };
    const auto paste = [&](const std::string &text)
    {
        REQUIRE(SDL_SetClipboardText(text.c_str()));
        press(SDLK_V, SDL_KMOD_LCTRL);
    };
    const auto fill = [&](const std::string &password, const std::string &confirmation)
    {
        paste("registration@example.com");
        press(SDLK_TAB);
        paste(password);
        press(SDLK_TAB);
        paste(confirmation);
    };
    const auto click = [&](int clickX, int clickY)
    {
        SDL_Event event {};
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.x = clickX;
        event.motion.y = clickY;
        scene.processEvent(event);
        event = {};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.clicks = 1;
        event.button.x = clickX;
        event.button.y = clickY;
        scene.processEvent(event);
        event.type = SDL_EVENT_MOUSE_BUTTON_UP;
        scene.processEvent(event);
    };

    SECTION("A click on the labelled button sends the request once")
    {
        fill("Valid123!", "Valid123!");
        click(400, 395);
        REQUIRE_FALSE(notifications.empty());
        notifications.clear();
        click(400, 395);
        press(SDLK_RETURN);
        scene.update(10001.0);
        press(SDLK_KP_ENTER);
        REQUIRE(notifications.empty());
    }

    SECTION("Both Enter keys work without field focus and ignore held-key repeats")
    {
        const auto submitKey = GENERATE(SDLK_RETURN, SDLK_KP_ENTER);
        fill("Valid123!", "Valid123!");
        click(225, 200);
        press(submitKey, SDL_KMOD_NONE, true);
        REQUIRE(notifications.empty());
        press(submitKey);
        REQUIRE_FALSE(notifications.empty());
        notifications.clear();
        scene.on_SM_CREATEACCOUNTOK(nullptr, 0);
        press(submitKey);
        REQUIRE(notifications.empty());
    }

    SECTION("Validation keeps all values and focuses the invalid password for correction")
    {
        fill("badbad", "badbad");
        press(SDLK_RETURN);
        REQUIRE(notifications.empty());
        paste("A1!");
        press(SDLK_TAB);
        paste("A1!");
        press(SDLK_RETURN);
        REQUIRE_FALSE(notifications.empty());
    }

    SECTION("An empty submission can be corrected immediately")
    {
        press(SDLK_RETURN);
        REQUIRE(notifications.empty());
        fill("Valid123!", "Valid123!");
        press(SDLK_RETURN);
        REQUIRE_FALSE(notifications.empty());
    }

    SECTION("A mismatched confirmation can be corrected without retyping other fields")
    {
        fill("Valid123!", "Valid123");
        press(SDLK_RETURN);
        REQUIRE(notifications.empty());
        paste("!");
        press(SDLK_KP_ENTER);
        REQUIRE_FALSE(notifications.empty());
    }

    SECTION("Shift Tab moves back through the fields")
    {
        paste("registration@example.com");
        press(SDLK_TAB, SDL_KMOD_LSHIFT);
        paste("Valid123!");
        press(SDLK_TAB, SDL_KMOD_RSHIFT);
        paste("Valid123!");
        press(SDLK_RETURN);
        REQUIRE_FALSE(notifications.empty());
    }

    SECTION("Clicking the padded field area focuses its input")
    {
        click(312, 288);
        paste("Valid123!");
        click(548, 343);
        paste("Valid123!");
        click(312, 230);
        paste("registration@example.com");
        press(SDLK_RETURN);
        REQUIRE_FALSE(notifications.empty());
    }

    SECTION("An existing account response preserves the passwords and allows an immediate correction")
    {
        fill("Valid123!", "Valid123!");
        press(SDLK_RETURN);
        REQUIRE_FALSE(notifications.empty());
        notifications.clear();
        const SMCreateAccountError error {.error = CRTACCERR_ACCOUNTEXIST};
        scene.on_SM_CREATEACCOUNTERROR(reinterpret_cast<const uint8_t *>(&error), sizeof(error));
        press(SDLK_RETURN);
        REQUIRE(notifications.empty());
        for(size_t count = 0; count < std::string_view("registration@example.com").size(); ++count){
            press(SDLK_BACKSPACE);
        }
        paste("another@example.com");
        press(SDLK_RETURN);
        REQUIRE_FALSE(notifications.empty());
    }

    SECTION("The form can draw inline feedback and a labelled submit button")
    {
        fill("badbad", "different");
        press(SDLK_RETURN);
        REQUIRE(notifications.empty());
        if(const auto screenshotPath = std::getenv("MIR2X_TEST_SCREENSHOT")){
            auto renderer = device.getRenderer();
            const std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> target(
                    SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 800, 600), SDL_DestroyTexture);
            REQUIRE(target);
            REQUIRE(SDL_SetRenderTarget(renderer, target.get()));
            scene.draw();
            const std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer, nullptr), SDL_DestroySurface);
            REQUIRE(surface);
            REQUIRE(IMG_SavePNG(surface.get(), screenshotPath));
            REQUIRE(SDL_SetRenderTarget(renderer, nullptr));
        }
        else{
            scene.draw();
        }
    }
}
