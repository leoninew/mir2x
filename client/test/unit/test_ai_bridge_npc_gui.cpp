#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include "clientargparser.hpp"
#include "fontexdb.hpp"
#include "log.hpp"
#include "sdldevice.hpp"
#include "pngtexdb.hpp"
#include "layoutboard.hpp"
#include "inputstringboard.hpp"
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
    struct GuiFixture
    {
        std::unique_ptr<ClientArgParser> args;
        std::unique_ptr<Log> log;
        std::unique_ptr<SDLDevice> device;
        std::unique_ptr<TestFonts> fonts;
        std::unique_ptr<EmptyImages> images;
        GuiFixture()
        {
            char name[]="npc_gui_test", audio[]="--disable-audio", profiler[]="--disable-profiler";
            char *argv[]={name,audio,profiler};
            const argf::parser parser(3,argv);
            args=std::make_unique<ClientArgParser>(parser); g_clientArgParser=args.get();
            logDisableProfiler();
            log=std::make_unique<Log>("mir2x-npc-gui-test","."); g_mir2xLog=log.get();
            device=std::make_unique<SDLDevice>(); g_sdlDevice=device.get(); device->createMainWindow();
            fonts=std::make_unique<TestFonts>(); g_fontexDB=fonts.get();
            const char *overridePath=std::getenv("MIR2X_TEST_RES_DIR");
            const std::string res=overridePath ? overridePath : MIR2X_TEST_DEFAULT_RES_DIR;
            fonts->load((res+"/font/fontex.zsdb").c_str());
            images=std::make_unique<EmptyImages>(); g_progUseDB=images.get();
        }
        ~GuiFixture()
        {
            images.reset(); fonts.reset(); device.reset(); log.reset(); args.reset();
            g_progUseDB=nullptr; g_fontexDB=nullptr; g_sdlDevice=nullptr; g_mir2xLog=nullptr; g_clientArgParser=nullptr;
        }
    };
}

TEST_CASE("NPC GUI enumerates actual event leaves and preserves single-click counters")
{
    GuiFixture fixture;
    size_t calls=0;
    std::unordered_map<std::string,std::string> received;
    LayoutBoard board({.lineWidth=250,.onClickText=[&](const auto &attrs,int event){
        REQUIRE(event==BEVENT_RELEASE); ++calls; received=attrs;
    }});
    board.loadXML("<layout><par><t id='fake'>ordinary</t><event ID='one' single='true' path='real' args='' close='true'>one</event>"
        "<event id='two'><t>format child</t>direct</event></par></layout>");
    const auto events=board.textEvents();
    REQUIRE(events.size()==2);
    CHECK(events[0].attributes.at("id")=="one");
    CHECK(events[1].text=="direct");
    REQUIRE(board.activateTextEvent(events[0].paragraph,events[0].leaf));
    CHECK(calls==1); CHECK(received.at("path")=="real"); CHECK(received.at("args").empty());
    CHECK_FALSE(board.activateTextEvent(events[0].paragraph,events[0].leaf));
    CHECK(board.textEvents().size()==1);
    CHECK(board.activateTextEvent(events[1].paragraph,events[1].leaf));
    CHECK(board.activateTextEvent(events[1].paragraph,events[1].leaf));
    CHECK(calls==3);
    CHECK_FALSE(board.activateTextEvent(-1,0));
    CHECK_FALSE(board.activateTextEvent(0,999));
    CHECK_THROWS_AS(board.textEvents(0),std::length_error);
}

TEST_CASE("NPC GUI event callback may replace layout without reusing leaf references")
{
    GuiFixture fixture;
    LayoutBoard *current=nullptr;
    LayoutBoard board({.onClickText=[&](const auto &,int){current->loadXML("<layout><par>replacement</par></layout>");}});
    current=&board;
    board.loadXML("<layout><par><event id='replace'>replace</event></par></layout>");
    const auto event=board.textEvents().at(0);
    REQUIRE(board.activateTextEvent(event.paragraph,event.leaf));
    CHECK(board.getText()=="replacement");
    CHECK(board.textEvents().empty());
}

TEST_CASE("NPC input uses normal validation trim and reentrant accept lifecycle")
{
    GuiFixture fixture;
    InputStringBoard board(DIR_UPLEFT,0,0,false);
    std::u8string accepted;
    size_t accepts=0;
    board.waitInput({.layoutString=u8"<layout><par>prompt</par></layout>",.security=true,
        .onAccept=[&](std::u8string text){
            accepted=std::move(text); ++accepts;
            board.waitInput({.layoutString=u8"<layout><par>next</par></layout>",.onAccept=[&](std::u8string){++accepts;},.npcUid=33});
        },
        .validate=[](std::string current,std::string added){return (current+added).find_first_not_of(" 0123456789")==std::string::npos;},.npcUid=33});
    const auto first=board.npcInput(); REQUIRE(first.has_value());
    CHECK(first->sensitive); CHECK(first->maxBytes==199);
    CHECK_FALSE(board.submitNpcInput(first->revision,"invalid"));
    CHECK_FALSE(board.submitNpcInput(first->revision,std::string(200,'1')));
    CHECK(accepts==0); CHECK(board.show());
    REQUIRE(board.submitNpcInput(first->revision," 123"));
    CHECK(accepted==u8"123"); CHECK(accepts==1);
    CHECK(board.show());
    const auto next=board.npcInput(); REQUIRE(next.has_value()); CHECK(next->revision!=first->revision);
    CHECK_FALSE(board.submitNpcInput(first->revision,"123"));
    REQUIRE(board.submitNpcInput(next->revision,"ok"));
    CHECK(accepts==2); CHECK_FALSE(board.show()); CHECK_FALSE(board.npcInput().has_value());
}

TEST_CASE("generic input and choice replacement revoke NPC input capability")
{
    GuiFixture fixture;
    InputStringBoard board(DIR_UPLEFT,0,0,false);
    size_t called=0;
    board.waitInput({.layoutString=u8"<layout><par>npc</par></layout>",.onAccept=[&](std::u8string){++called;},.npcUid=33});
    const auto old=board.npcInput()->revision;
    board.waitInput({.layoutString=u8"<layout><par>generic</par></layout>",.onAccept=[&](std::u8string){++called;}});
    CHECK_FALSE(board.npcInput().has_value()); CHECK_FALSE(board.submitNpcInput(old,"injected"));
    board.waitChoice(u8"<layout><par>choice</par></layout>",true,[&]{++called;});
    CHECK_FALSE(board.npcInput().has_value()); CHECK_FALSE(board.submitNpcInput(old,"injected"));
    CHECK(called==0); CHECK(board.showModal());
}
