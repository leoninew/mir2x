#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <string>
#include "clientargparser.hpp"
#include "emojidb.hpp"
#include "fontexdb.hpp"
#include "inputline.hpp"
#include "layoutboard.hpp"
#include "log.hpp"
#include "sdldevice.hpp"

class PNGTexDB;
class ProgUseDB;
class PNGTexOffDB;
class MagicDB;
class MapBinDB;
class BGMusicDB;
class SoundEffectDB;
class IMEBoard;
class MessageStackBoard;
class Client;

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

namespace
{
    SDL_Event keyEvent(SDL_Keycode key = SDLK_V, SDL_Keymod modifiers = SDL_KMOD_LCTRL)
    {
        SDL_Event event{};
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.key = key;
        event.key.mod = modifiers;
        return event;
    }
}

TEST_CASE("Clipboard editing respects input focus, IME, validation, and submission", "[textinput]")
{
    struct ResetGlobals
    {
        ~ResetGlobals()
        {
            g_clientArgParser = nullptr;
            g_mir2xLog = nullptr;
            g_sdlDevice = nullptr;
            g_fontexDB = nullptr;
            g_emojiDB = nullptr;
        }
    } resetGlobals;

    const char *resourceOverride = std::getenv("MIR2X_TEST_RES_DIR");
    const std::string resDir = resourceOverride ? resourceOverride : MIR2X_TEST_DEFAULT_RES_DIR;

    char name[] = "textinput_test";
    char audio[] = "--disable-audio";
    char profiler[] = "--disable-profiler";
    char *options[] = {name, audio, profiler};
    const argf::parser parser(3, options);
    ClientArgParser clientArgs(parser);
    g_clientArgParser = &clientArgs;
    logDisableProfiler();

    Log log("mir2x-textinput-test", ".");
    g_mir2xLog = &log;
    SDLDevice device;
    g_sdlDevice = &device;
    device.createMainWindow();

    FontexDB fonts(1024 * 1024 * 64);
    g_fontexDB = &fonts;
    fonts.load((resDir + "/font/fontex.zsdb").c_str());
    EmojiDB emoji;
    g_emojiDB = &emoji;
    emoji.load((resDir + "/emoji/emoji.zsdb").c_str());

    SECTION("Only the paste shortcut reads text and repeats do not duplicate it")
    {
        REQUIRE(SDL_SetClipboardText("hello\r\nworld\rnext\nline\tend\x01\x7F"));
        REQUIRE(SDLDeviceHelper::getPasteText(keyEvent(), true).value() == "hello\nworld\nnext\nline end");
        REQUIRE(SDLDeviceHelper::getPasteText(keyEvent(SDLK_V, SDL_KMOD_RCTRL), false).value() == "hello world next line end");
        REQUIRE(SDLDeviceHelper::getPasteText(keyEvent(SDLK_V, SDL_KMOD_CTRL | SDL_KMOD_SHIFT), true).has_value());
        REQUIRE_FALSE(SDLDeviceHelper::getPasteText(keyEvent(SDLK_V, SDL_KMOD_NONE), true).has_value());
        REQUIRE_FALSE(SDLDeviceHelper::getPasteText(keyEvent(SDLK_V, SDL_KMOD_CTRL | SDL_KMOD_ALT), true).has_value());
        REQUIRE_FALSE(SDLDeviceHelper::getPasteText(keyEvent(SDLK_C), true).has_value());
        auto repeat = keyEvent();
        repeat.key.repeat = true;
        REQUIRE(SDLDeviceHelper::getPasteText(repeat, true).value().empty());
    }

    SECTION("Commands paste literally under every IME mode and require Return to submit")
    {
        for(const int ime: {IME_DISABLE, IME_EMBEDED, IME_SYSTEM}){
            CAPTURE(ime);
            int submissions = 0;
            LayoutBoard board
            {{
                .lineWidth = 80,
                .canEdit = true,
                .enableIME = ime,
                .onCR = [&submissions](bool){ ++submissions; return true; },
            }};
            board.setFocus(true);
            const std::string command = "$print(\"你好 <t> & 中文\")";
            REQUIRE(SDL_SetClipboardText(command.c_str()));
            REQUIRE(board.processEventDefault(keyEvent(), true, {}));
            REQUIRE(board.getText() == command);
            REQUIRE(submissions == 0);
            const auto [cursorPar, cursorX, cursorY] = board.getCursorLoc();
            REQUIRE(board.cursorLocValid(cursorPar, cursorX, cursorY));
            REQUIRE(board.processEventDefault(keyEvent(SDLK_RETURN, SDL_KMOD_NONE), true, {}));
            REQUIRE(submissions == 1);
            board.setFocus(false);
        }
    }

    SECTION("Multiline paste splits at the cursor without submitting any line")
    {
        int submissions = 0;
        LayoutBoard board
        {{
            .lineWidth = 80,
            .canEdit = true,
            .onCR = [&submissions](bool){ ++submissions; return true; },
        }};
        board.setFocus(true);
        REQUIRE(SDL_SetClipboardText("前后"));
        REQUIRE(board.processEventDefault(keyEvent(), true, {}));
        REQUIRE(board.processEventDefault(keyEvent(SDLK_LEFT, SDL_KMOD_NONE), true, {}));
        REQUIRE(SDL_SetClipboardText("甲\r\n\r\n乙\r丙\n"));
        REQUIRE(board.processEventDefault(keyEvent(), true, {}));
        REQUIRE(board.getText() == "前甲\n\n乙\n丙\n后");
        REQUIRE(board.parCount() == 5);
        REQUIRE(board.getCursorOff() == std::tuple{4, 0});
        REQUIRE(SDL_SetClipboardText("丁"));
        REQUIRE(board.processEventDefault(keyEvent(), true, {}));
        REQUIRE(board.getText() == "前甲\n\n乙\n丙\n丁后");
        REQUIRE(board.processEventDefault(keyEvent(SDLK_BACKSPACE, SDL_KMOD_NONE), true, {}));
        REQUIRE(board.processEventDefault(keyEvent(SDLK_BACKSPACE, SDL_KMOD_NONE), true, {}));
        REQUIRE(board.getText() == "前甲\n\n乙\n丙后");
        REQUIRE(submissions == 0);
    }

    SECTION("Typing and Return still edit paragraphs through the same insertion path")
    {
        LayoutBoard board{{.canEdit = true}};
        board.setFocus(true);
        REQUIRE(board.processEventDefault(keyEvent(SDLK_A, SDL_KMOD_NONE), true, {}));
        REQUIRE(board.processEventDefault(keyEvent(SDLK_RETURN, SDL_KMOD_NONE), true, {}));
        REQUIRE(board.processEventDefault(keyEvent(SDLK_B, SDL_KMOD_NONE), true, {}));
        REQUIRE(board.getText() == "a\nb");
        REQUIRE(board.processEventDefault(keyEvent(SDLK_LEFT, SDL_KMOD_NONE), true, {}));
        REQUIRE(board.processEventDefault(keyEvent(SDLK_BACKSPACE, SDL_KMOD_NONE), true, {}));
        REQUIRE(board.getText() == "ab");
    }

    SECTION("Paste requires a valid focused editable widget and ignores an empty clipboard")
    {
        REQUIRE(SDL_SetClipboardText("message"));
        LayoutBoard board{{.canEdit = true}};
        REQUIRE_FALSE(board.processEventDefault(keyEvent(), true, {}));
        board.setFocus(true);
        REQUIRE_FALSE(board.processEventDefault(keyEvent(), false, {}));
        REQUIRE(board.getText().empty());
        LayoutBoard readonly{{.initXML = "<layout><par>original</par></layout>"}};
        readonly.setFocus(true);
        REQUIRE_FALSE(readonly.processEventDefault(keyEvent(), true, {}));
        REQUIRE(readonly.getText() == "original");
        REQUIRE(SDL_SetClipboardText(""));
        REQUIRE(board.processEventDefault(keyEvent(), true, {}));
        REQUIRE(board.getText().empty());
        REQUIRE(board.parCount() == 1);
    }

    SECTION("Single-line paste uses the existing validator and change callback")
    {
        int changes = 0;
        int submissions = 0;
        InputLine input
        {{
            .w = 200,
            .h = 24,
            .onCR = [&submissions]{ ++submissions; },
            .onChange = [&changes](std::string){ ++changes; },
            .validate = [](const std::string &current, const std::string &added)
            {
                return current.size() + added.size() <= 4 && added.find_first_not_of("0123456789") == std::string::npos;
            },
        }};
        REQUIRE(SDL_SetClipboardText("34"));
        REQUIRE_FALSE(input.processEventDefault(keyEvent(), true, {}));
        input.setFocus(true);
        REQUIRE_FALSE(input.processEventDefault(keyEvent(), false, {}));
        input.setInput("12");
        changes = 0;
        REQUIRE(input.processEventDefault(keyEvent(SDLK_LEFT, SDL_KMOD_NONE), true, {}));
        REQUIRE(input.processEventDefault(keyEvent(), true, {}));
        REQUIRE(input.getRawString() == "1342");
        REQUIRE(changes == 1);
        for(const char *rejected: {"5", "invalid", ""}){
            REQUIRE(SDL_SetClipboardText(rejected));
            REQUIRE(input.processEventDefault(keyEvent(), true, {}));
            REQUIRE(input.getRawString() == "1342");
            REQUIRE(changes == 1);
        }
        REQUIRE(submissions == 0);
    }

    SECTION("Single-line paste normalizes newlines and keeps UTF-8 cursor editing")
    {
        for(const int ime: {IME_DISABLE, IME_EMBEDED, IME_SYSTEM}){
            CAPTURE(ime);
            InputLine input{{.w = 200, .h = 24, .enableIME = ime}};
            input.setFocus(true);
            REQUIRE(SDL_SetClipboardText("甲\r\n乙\t丙"));
            REQUIRE(input.processEventDefault(keyEvent(), true, {}));
            REQUIRE(input.getRawString() == "甲 乙 丙");
            REQUIRE(input.processEventDefault(keyEvent(SDLK_BACKSPACE, SDL_KMOD_NONE), true, {}));
            REQUIRE(input.getRawString() == "甲 乙 ");
            auto repeat = keyEvent();
            repeat.key.repeat = true;
            REQUIRE(input.processEventDefault(repeat, true, {}));
            REQUIRE(input.getRawString() == "甲 乙 ");
            input.setFocus(false);
        }
    }
}
