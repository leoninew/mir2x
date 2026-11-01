#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include "clientargparser.hpp"
#include "fontexdb.hpp"
#include "log.hpp"
#include "sdldevice.hpp"
#include "pngtexdb.hpp"
#include "layoutboard.hpp"
#include "inputstringboard.hpp"
#include "gui/auctionboard/auctionregisternote.hpp"
#include "gui/auctionboard/auctionregisterbox.hpp"
#include "gui/auctionboard/auctionregisterprice.hpp"
#include "aibridge/economy.hpp"
#include "dbcomid.hpp"
#include "gui/auctionboard/auctionregisterboard.hpp"
#include "gui/auctionboard/auctionboard.hpp"
#include "invpack.hpp"
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
        EmptyImages(): PNGTexDB(64*1024*1024) {}
        ~EmptyImages() override { clear(); }
        std::optional<std::tuple<PNGTexElement, size_t>> loadResource(uint32_t id) override
        {
            const int w=(id==0x1400 || id==0x1410) ? 800 : 20;
            const int h=(id==0x1400 || id==0x1410) ? 450 : 20;
            auto texture=SDL_CreateTexture(g_sdlDevice->getRenderer(),SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_STATIC,w,h);
            if(!texture){ return {}; }
            return std::tuple{PNGTexElement{texture},size_t(w*h*4)};
        }
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
            char name[]="economy_gui_test", audio[]="--disable-audio", profiler[]="--disable-profiler";
            char *argv[]={name,audio,profiler};
            const argf::parser parser(3,argv);
            args=std::make_unique<ClientArgParser>(parser); g_clientArgParser=args.get();
            logDisableProfiler();
            log=std::make_unique<Log>("mir2x-economy-gui-test","."); g_mir2xLog=log.get();
            device=std::make_unique<SDLDevice>(); g_sdlDevice=device.get(); device->createMainWindow();
            fonts=std::make_unique<TestFonts>(); g_fontexDB=fonts.get();
            const char *overridePath=std::getenv("MIR2X_TEST_RES_DIR");
            const std::string res=overridePath ? overridePath : MIR2X_TEST_DEFAULT_RES_DIR;
            fonts->load((res+"/font/fontex.zsdb").c_str());
            images=std::make_unique<EmptyImages>(); g_progUseDB=images.get(); g_itemDB=images.get();
        }
        ~GuiFixture()
        {
            images.reset(); fonts.reset(); device.reset(); log.reset(); args.reset();
            g_itemDB=nullptr; g_progUseDB=nullptr; g_fontexDB=nullptr; g_sdlDevice=nullptr; g_mir2xLog=nullptr; g_clientArgParser=nullptr;
        }
    };
}

TEST_CASE("economy GUI auction note typed input renders plain text without injecting event XML")
{
    GuiFixture fixture;
    AuctionRegisterNote note({});
    note.setText("seller <event id='bad'>offer</event> & 123");
    CHECK(note.getText()=="seller <event id='bad'>offer</event> & 123");
    note.setText(""); CHECK(note.getText().empty());
    note.setText(std::string(128,'a')); CHECK(note.getText().size()==128);
    note.clear(); CHECK(note.getText().empty());
}

TEST_CASE("economy GUI auction reservation box disables clicks without discarding held item")
{
    GuiFixture fixture;
    size_t clicks=0;
    AuctionRegisterBox box({.onClick=[&]{++clicks;}});
    SDItem item; item.itemID=DBCOM_ITEMID(u8"金创药（小）"); item.seqID=9; item.count=3;
    box.setItem(item);
    CHECK(box.item().seqID==9); CHECK(box.item().count==3);
    box.setInputEnabled(false);
    SDL_Event e{}; e.type=SDL_EVENT_MOUSE_BUTTON_DOWN; e.button.button=SDL_BUTTON_LEFT; e.button.x=50; e.button.y=50;
    box.processEventRoot(e,true,{});
    CHECK(clicks==0); CHECK(box.item().seqID==9);
    box.setInputEnabled(true); box.processEventRoot(e,true,{}); CHECK(clicks==1);
    const auto taken=box.takeItem(); CHECK(taken.seqID==9); CHECK_FALSE(box.item());
}

TEST_CASE("economy GUI auction price preserves full signed 64 bit asking price")
{
    GuiFixture fixture;
    AuctionRegisterPrice price({}); price.setPrice(uint64_t(INT64_MAX));
    CHECK(price.price()==uint64_t(INT64_MAX));
    price.setInputEnabled(false); CHECK(price.price()==uint64_t(INT64_MAX));
    price.clear(); CHECK(price.price()==0);
}

TEST_CASE("economy GUI delivery event renders the same received trusted record")
{
    GuiFixture fixture;
    const std::string xml="<layout><par>system delivery</par><par><event id='_RSVD_NAME_DELIVERY_5639021748316' record='12345678901234567890123456789012'>claim</event></par></layout>";
    const auto records=aibridge::systemDeliveries(true,xml);
    REQUIRE(records.size()==1);
    std::string activated;
    LayoutBoard board({.onClickText=[&](const auto &attrs,int){activated=attrs.at("record");}});
    board.loadXML(xml.c_str()); const auto leaves=board.textEvents(); REQUIRE(leaves.size()==1);
    CHECK(board.activateTextEvent(leaves[0].paragraph,leaves[0].leaf)); CHECK(activated==records[0].record);
    CHECK(aibridge::systemDeliveries(false,xml).empty());
}

namespace
{
    struct ItemImages: PNGTexDB
    {
        ItemImages(): PNGTexDB(1024*1024) {}
        ~ItemImages() override { clear(); }
        std::optional<std::tuple<PNGTexElement,size_t>> loadResource(uint32_t) override
        {
            auto texture=SDL_CreateTexture(g_sdlDevice->getRenderer(),SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_STATIC,20,20);
            if(!texture){ return {}; }
            return std::tuple{PNGTexElement{texture},size_t(1600)};
        }
    };
}
TEST_CASE("actual auction registration GUI reserves whole item and settles only explicit receipts")
{
    GuiFixture fixture;
    ItemImages images; g_itemDB=&images;
    struct Reset { ~Reset(){ g_itemDB=nullptr; } } reset;
    InvPack inventory;
    const auto itemId=DBCOM_ITEMID(u8"金创药（小）");
    inventory.add(SDItem{.itemID=itemId,.seqID=77,.count=10},false);
    unsigned sends=0;
    auto request=std::make_shared<aibridge::EconomicRequest>();
    AuctionRegisterBoard::Access access{
        .inventory=[&]() -> InvPack & { return inventory; },
        .restore=[&](SDItem item){ inventory.add(std::move(item),false); },
        .submit=[&](const SDItem &item,uint64_t price,const std::string &note){
            ++sends; CHECK(item.count==10); CHECK(item.seqID==77); CHECK(price==uint64_t(INT64_MAX)); CHECK(note=="test & note");
            request->submitted=true; return request;
        },
        .log=[](const char8_t *){},
    };
    AuctionRegisterBoard board(nullptr,nullptr,false,std::move(access));
    board.prepareItem(itemId,77,uint64_t(INT64_MAX),"test & note");
    CHECK(inventory.getPackBinList().empty()); CHECK(board.reservedItem().count==10);
    CHECK(board.submitRegister()==request); CHECK(sends==1); CHECK(board.pending());
    CHECK_FALSE(board.cancelUnsubmitted());
    CHECK_THROWS_AS(board.submitRegister(),aibridge::ProtocolError); CHECK(sends==1);
    SECTION("explicit successful receipt clears reservation without returning it"){
        request->receipt=aibridge::EconomicReceipt::Ok; board.updateDefault(0);
        CHECK_FALSE(board.reservedItem()); CHECK(inventory.getPackBinList().empty()); CHECK_FALSE(board.show());
    }
    SECTION("explicit rejection keeps box until ordinary cancellation returns it"){
        request->receipt=aibridge::EconomicReceipt::Rejected; request->error=1; board.updateDefault(0);
        CHECK(board.reservedItem().count==10); CHECK_FALSE(board.pending()); CHECK(board.cancelUnsubmitted());
        REQUIRE(inventory.getPackBinList().size()==1); CHECK(inventory.getPackBinList()[0].item.count==10);
    }
    SECTION("timeout after server execution never returns or resubmits the item"){
        request->receipt=aibridge::EconomicReceipt::Unknown; board.updateDefault(0);
        CHECK_FALSE(board.pending()); CHECK(board.uncertain()); CHECK(board.reservedItem().count==10);
        CHECK_FALSE(board.cancelUnsubmitted()); CHECK_THROWS_AS(board.submitRegister(),aibridge::ProtocolError);
        CHECK(sends==1); CHECK(inventory.getPackBinList().empty());
    }
}

TEST_CASE("actual auction board correlates categories and keeps unknown purchases nonrepeatable")
{
    GuiFixture fixture;
    ItemImages images; g_itemDB=&images;
    struct Reset { ~Reset(){ g_itemDB=nullptr; } } reset;
    uint64_t now=1; unsigned queries=0,sends=0;
    aibridge::Json context={{"instance_id",std::string(32,'a')},{"session_id",std::string(32,'b')},{"world_epoch","1"},{"control_epoch","1"}};
    auto request=std::make_shared<aibridge::EconomicRequest>();
    AuctionBoard::Access access{
        .dbid=[]{ return uint32_t(1); },.gold=[]{ return uint64_t(100); },
        .query=[&](int){ ++queries; },
        .submit=[&](uint64_t id,bool cancel){ CHECK(id==99); CHECK_FALSE(cancel); ++sends; request->submitted=true; return request; },
        .log=[](const char8_t *){},.clock=[&]{return now;},.scope=[&]{return context;},
    };
    AuctionBoard board(nullptr,nullptr,false,std::move(access));
    SDAuctionItem item; item.auctionID=99; item.price=70;
    item.seller={.id=2,.name="seller",.despvar=SDChatPeerPlayerVar{}};
    item.item={.itemID=DBCOM_ITEMID(u8"金创药（小）"),.seqID=77,.count=10};
    CHECK(board.requestList(AUCTIONCAT_ALL)); CHECK_FALSE(board.requestList(AUCTIONCAT_ALL)); CHECK(queries==1);
    board.setItemList({.category=AUCTIONCAT_ALL,.itemList={item}});
    REQUIRE(board.cachedList(AUCTIONCAT_ALL)!=nullptr); CHECK(board.currentCategory()==AUCTIONCAT_ALL);
    CHECK(board.submitAuction(99,false)==request); CHECK(sends==1);
    CHECK_THROWS_AS(board.submitAuction(99,false),aibridge::ProtocolError);
    SECTION("confirmed purchase invalidates old cache and never automatically refreshes"){
        request->receipt=aibridge::EconomicReceipt::Ok; board.updateDefault(0);
        CHECK_FALSE(board.cachedList(AUCTIONCAT_ALL)); CHECK(queries==1); CHECK_FALSE(board.pending());
    }
    SECTION("unknown purchase remains locked with no implicit refund or second send"){
        request->receipt=aibridge::EconomicReceipt::Unknown; board.updateDefault(0);
        CHECK(board.uncertain()); CHECK_FALSE(board.pending());
        CHECK_THROWS_AS(board.submitAuction(99,false),aibridge::ProtocolError); CHECK(sends==1);
    }
    SECTION("timeout category response cannot reopen panel or replace cache"){
        board.requestList(AUCTIONCAT_BOOK); board.closePanel(); now=10001;
        board.setItemList({.category=AUCTIONCAT_BOOK,.itemList={item}});
        CHECK_FALSE(board.show()); CHECK_FALSE(board.cachedList(AUCTIONCAT_BOOK)); CHECK(board.refreshState(AUCTIONCAT_BOOK).quarantined);
    }
    SECTION("new world query delivered before observer preserves its real current panel"){
        context["world_epoch"]="2";
        board.requestList(AUCTIONCAT_BOOK);
        board.setItemList({.category=AUCTIONCAT_BOOK,.itemList={item}});
        board.invalidateScope(context);
        CHECK(board.show()); CHECK(board.currentCategory()==AUCTIONCAT_BOOK);
        CHECK(board.cachedList(AUCTIONCAT_BOOK)!=nullptr); CHECK_FALSE(board.cachedList(AUCTIONCAT_ALL));
    }
}

TEST_CASE("actual auction board only withdraws owned listings and preserves explicit rejection")
{
    GuiFixture fixture;
    ItemImages images; g_itemDB=&images;
    struct Reset { ~Reset(){ g_itemDB=nullptr; } } reset;
    unsigned sends=0; auto request=std::make_shared<aibridge::EconomicRequest>();
    AuctionBoard::Access access{
        .dbid=[]{return uint32_t(1);},.gold=[]{return uint64_t(100);},.query=[](int){},
        .submit=[&](uint64_t,bool cancel){ CHECK(cancel); ++sends; request->submitted=true; return request; },
        .log=[](const char8_t *){},.clock=[]{return uint64_t(1);},
    };
    AuctionBoard board(nullptr,nullptr,false,std::move(access));
    SDAuctionItem item; item.auctionID=99; item.price=70;
    item.seller={.id=1,.name="self",.despvar=SDChatPeerPlayerVar{}};
    item.item={.itemID=DBCOM_ITEMID(u8"金创药（小）"),.seqID=77,.count=10};
    board.requestList(AUCTIONCAT_ALL); board.setItemList({.category=AUCTIONCAT_ALL,.itemList={item}});
    CHECK_THROWS_AS(board.submitAuction(99,false),aibridge::ProtocolError); CHECK(sends==0);
    board.submitAuction(99,true); CHECK(sends==1);
    request->receipt=aibridge::EconomicReceipt::Rejected; request->error=1; board.updateDefault(0);
    CHECK_FALSE(board.pending()); CHECK_FALSE(board.uncertain()); CHECK(board.cachedList(AUCTIONCAT_ALL)!=nullptr);
}
