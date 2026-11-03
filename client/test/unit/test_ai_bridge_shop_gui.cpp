#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include "clientargparser.hpp"
#include "fontexdb.hpp"
#include "log.hpp"
#include "sdldevice.hpp"
#include "pngtexdb.hpp"
#include "layoutboard.hpp"
#include "inputstringboard.hpp"
#include "gui/purchaseboard/purchaseboard.hpp"
#include "inventoryboard.hpp"
#include "invpack.hpp"
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
        EmptyImages(): PNGTexDB(16*1024*1024) {}
        ~EmptyImages() override { clear(); }
        std::optional<std::tuple<PNGTexElement, size_t>> loadResource(uint32_t) override
        {
            auto texture=SDL_CreateTexture(g_sdlDevice->getRenderer(),SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_STATIC,440,520);
            if(!texture){ return {}; }
            return std::tuple{PNGTexElement{texture},size_t(440*520*4)};
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
            char name[]="shop_gui_test", audio[]="--disable-audio", profiler[]="--disable-profiler";
            char *argv[]={name,audio,profiler};
            const argf::parser parser(3,argv);
            args=std::make_unique<ClientArgParser>(parser); g_clientArgParser=args.get();
            logDisableProfiler();
            log=std::make_unique<Log>("mir2x-shop-gui-test","."); g_mir2xLog=log.get();
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

namespace
{
    struct ItemImages: PNGTexDB
    {
        ItemImages():PNGTexDB(1024*1024) {}
        ~ItemImages() override { clear(); }
        std::optional<std::tuple<PNGTexElement,size_t>> loadResource(uint32_t) override
        {
            auto texture=SDL_CreateTexture(g_sdlDevice->getRenderer(),SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_STATIC,20,20);
            if(!texture){ return {}; }
            return std::tuple{PNGTexElement{texture},size_t(1600)};
        }
    };
    struct ShopPorts
    {
        uint64_t now=100;
        uint32_t item=DBCOM_ITEMID(u8"金创药（小）");
        unsigned queries=0,buys=0;
        uint32_t bought=0;
        PurchaseBoard::Access access()
        {
            return {.query=[this](uint64_t,uint32_t){ ++queries; },
                .buy=[this](uint64_t,uint32_t,uint32_t,uint32_t count){ ++buys; bought=count; },
                .now=[this]{ return now; }};
        }
        SDSellItemList reply(uint64_t npc=33)
        {
            return {.npcUID=npc,.list={{.item={.itemID=item,.seqID=0,.count=1},.costList={{DBCOM_ITEMID(u8"金币"),10}}}}};
        }
    };
    struct InventoryPorts
    {
        InvPack pack;
        uint64_t now=100;
        uint32_t item=DBCOM_ITEMID(u8"金创药（小）");
        std::vector<std::string> events;
        InventoryBoard::ShopAccess access()
        {
            return {.inventory=[this]{return &pack;},
                .event=[this](uint64_t,const std::string &event,const std::string &value){events.push_back(event+":"+value);},
                .now=[this]{return now;}};
        }
        SDStartInvOp panel(uint64_t npc=33)
        {
            return {.invOp=INVOP_TRADE,.uid=npc,.queryTag="QUERY",.commitTag="COMMIT",.typeList={DBCOM_ITEMRECORD(item).type}};
        }
        void add(){ pack.add({.itemID=item,.seqID=7,.count=5},false); }
    };
}

TEST_CASE("Shop GUI shares one detail request and sends one validated buy")
{
    GuiFixture fixture;
    ShopPorts ports;
    PurchaseBoard board(nullptr,nullptr,false,ports.access());
    board.loadSell(33,{ports.item}); board.setShow(true);
    REQUIRE(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Sent);
    CHECK(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Coalesced);
    CHECK(ports.queries==1);
    board.setSellItemList(ports.reply(44));
    CHECK(board.detailCache().empty()); CHECK(board.detailQuery().pending());
    board.setSellItemList(ports.reply());
    REQUIRE(board.detailCache().size()==1);
    CHECK_FALSE(board.submitBuy(44,ports.item,0,5));
    CHECK_FALSE(board.submitBuy(33,ports.item,1,5));
    REQUIRE(board.submitBuy(33,ports.item,0,5));
    CHECK(ports.buys==1); CHECK(ports.bought==5); CHECK(board.buyAttempts(33,ports.item,0)==1);
    ports.now+=30000;
    CHECK_FALSE(board.submitBuy(33,ports.item,0,5));
    CHECK(ports.buys==1);
}

TEST_CASE("Shop GUI discards late details across timeout and panel replacement")
{
    GuiFixture fixture;
    ShopPorts ports;
    PurchaseBoard board(nullptr,nullptr,false,ports.access());
    board.loadSell(33,{ports.item}); board.setShow(true);
    REQUIRE(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Sent);
    ports.now+=10000;
    CHECK(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Busy);
    board.setSellItemList(ports.reply());
    CHECK(board.detailCache().empty()); CHECK_FALSE(board.detailQuery().pending());
    REQUIRE(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Sent);
    board.loadSell(44,{ports.item});
    CHECK(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Busy);
    board.setSellItemList(ports.reply());
    CHECK(board.detailCache().empty());
    REQUIRE(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Sent);
    board.setSellItemList(ports.reply(44));
    REQUIRE(board.detailCache().size()==1);
    board.invalidateDetails();
    CHECK_FALSE(board.submitBuy(44,ports.item,0,1));
}

TEST_CASE("Shop GUI repeatable packable stock survives unit purchase success")
{
    GuiFixture fixture;
    ShopPorts ports;
    REQUIRE(ports.item==6);
    REQUIRE(DBCOM_ITEMRECORD(ports.item).packable());
    PurchaseBoard board(nullptr,nullptr,false,ports.access());
    board.loadSell(33,{ports.item}); board.setShow(true);
    REQUIRE(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Sent);
    auto reply=ports.reply();
    reply.list.front().costList={{DBCOM_ITEMID(u8"金币（小）"),80}};
    board.setSellItemList(std::move(reply));
    REQUIRE(board.detailQuery().accepted());
    const auto received=board.detailCache().at(ports.item).receivedAt;
    const auto revision=board.detailsRevision();
    for(unsigned bought=1; bought<=3; ++bought){
        REQUIRE(board.submitBuy(33,ports.item,0,1));
        CHECK(ports.buys==bought);
        CHECK(ports.bought==1);
        board.onBuySucceed(33,ports.item,0);
        REQUIRE(board.details().list.size()==1);
        const auto &cached=board.detailCache().at(ports.item);
        REQUIRE(cached.value.list.size()==1);
        const auto &row=cached.value.list.front();
        CHECK(row.item.itemID==ports.item);
        CHECK(row.item.seqID==0);
        CHECK(row.item.count==1);
        REQUIRE(row.costList.size()==1);
        CHECK(row.costList.front().itemID==DBCOM_ITEMID(u8"金币（小）"));
        CHECK(row.costList.front().count==80);
        CHECK(cached.receivedAt==received);
        CHECK(board.detailsRevision()==revision);
        CHECK(board.catalog()==std::vector<uint32_t>{ports.item});
        CHECK(ports.queries==1);
        ports.now+=100;
    }
    ports.now=received+30000;
    CHECK_FALSE(board.submitBuy(33,ports.item,0,1));
    CHECK(ports.buys==3);
}

TEST_CASE("Shop GUI nonpackable success removes only the purchased exact stock")
{
    GuiFixture fixture;
    ShopPorts ports;
    ports.item=DBCOM_ITEMID(u8"木剑");
    REQUIRE_FALSE(DBCOM_ITEMRECORD(ports.item).packable());
    PurchaseBoard board(nullptr,nullptr,false,ports.access());
    board.loadSell(33,{ports.item}); board.setShow(true);
    REQUIRE(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Sent);
    const auto coin=DBCOM_ITEMID(u8"金币（小）");
    board.setSellItemList({.npcUID=33,.list={
        {.item={.itemID=ports.item,.seqID=7,.count=1},.costList={{coin,1}}},
        {.item={.itemID=ports.item,.seqID=8,.count=1},.costList={{coin,1}}},
    }});
    REQUIRE(board.detailQuery().accepted());
    REQUIRE(board.detailCache().at(ports.item).value.list.size()==2);
    REQUIRE(board.submitBuy(33,ports.item,7,1));
    board.onBuySucceed(33,ports.item,7);
    REQUIRE(board.detailCache().at(ports.item).value.list.size()==1);
    REQUIRE(board.details().list.size()==1);
    CHECK(board.detailCache().at(ports.item).value.list.front().item.seqID==8);
    CHECK(board.details().list.front().item.seqID==8);
    CHECK_FALSE(board.submitBuy(33,ports.item,7,1));
    REQUIRE(board.submitBuy(33,ports.item,8,1));
    board.onBuySucceed(33,ports.item,8);
    CHECK(board.detailCache().at(ports.item).value.list.empty());
    CHECK(board.details().list.empty());
    CHECK_FALSE(board.submitBuy(33,ports.item,8,1));
    CHECK(ports.buys==2);
    CHECK(ports.queries==1);
}

TEST_CASE("Shop GUI manual overlap withdraws the old AI watch receipt qualification")
{
    GuiFixture fixture;
    ShopPorts ports;
    PurchaseBoard board(nullptr,nullptr,false,ports.access());
    board.loadSell(33,{ports.item}); board.setShow(true);
    REQUIRE(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Sent);
    board.setSellItemList(ports.reply());
    aibridge::ShopBuyReceipt receipt;
    receipt.arm(board.buyAttempts(33,ports.item,0));
    REQUIRE(board.submitBuy(33,ports.item,0,1));
    REQUIRE(board.submitBuy(33,ports.item,0,1));
    CHECK_FALSE(receipt.receive(board.buyAttempts(33,ports.item,0),0));
    CHECK_FALSE(receipt.receive(board.buyAttempts(33,ports.item,0),7));
    CHECK_FALSE(receipt.result());
}

TEST_CASE("Shop GUI full attempt history still revokes a repeated purchase receipt")
{
    GuiFixture fixture;
    ShopPorts ports;
    PurchaseBoard board(nullptr,nullptr,false,ports.access());
    const auto open = [&](uint64_t npc){
        board.loadSell(npc,{ports.item}); board.setShow(true);
        REQUIRE(board.queryDetails(ports.item)==aibridge::ShopQuery::Start::Sent);
        board.setSellItemList(ports.reply(npc));
    };
    for(uint64_t npc=1; npc<8192; ++npc){
        open(npc);
        REQUIRE(board.submitBuy(npc,ports.item,0,1));
    }
    open(8192);
    aibridge::ShopBuyReceipt receipt;
    REQUIRE(board.buyAttempts(8192,ports.item,0)==0);
    receipt.arm(board.buyAttempts(8192,ports.item,0));
    REQUIRE(board.submitBuy(8192,ports.item,0,1));
    REQUIRE(board.buyAttempts(8192,ports.item,0)==1);
    REQUIRE(board.submitBuy(8192,ports.item,0,1));
    CHECK(board.buyAttempts(8192,ports.item,0)==2);
    CHECK_FALSE(receipt.receive(board.buyAttempts(8192,ports.item,0),0));
    CHECK_FALSE(receipt.receive(board.buyAttempts(8192,ports.item,0),7));
    CHECK_FALSE(receipt.result());
    REQUIRE(board.submitBuy(8192,ports.item,0,1));
    CHECK(board.buyAttempts(8192,ports.item,0)==2);
    open(8193);
    CHECK(board.buyAttempts(8193,ports.item,0)==UINT64_MAX);
    REQUIRE(board.submitBuy(8193,ports.item,0,1));
    CHECK(board.buyAttempts(8193,ports.item,0)==UINT64_MAX);
    CHECK(ports.buys==8195);
}

TEST_CASE("Inventory new-world real panel remains open while its previous-world quotation is revoked")
{
    GuiFixture fixture;
    ItemImages images; g_progUseDB=&images; g_itemDB=&images;
    InventoryPorts ports; ports.add();
    InventoryBoard board({.shopAccess=ports.access()});
    aibridge::ShopPanelSource source;
    auto context=aibridge::Json{{"instance_id",std::string(32,'a')},{"session_id",std::string(32,'b')},{"world_epoch","1"},{"control_epoch","1"}};
    board.startInvOp(ports.panel()); board.setShow(true);
    if(source.observe(context)){ board.invalidateQuote(); }
    REQUIRE(board.aiInvOp().invOp==INVOP_TRADE); REQUIRE(board.show());
    REQUIRE(source.current(context));
    REQUIRE(board.queryInvOp(ports.item,7)==aibridge::ShopQuery::Start::Sent);
    board.setInvOpCost(INVOP_TRADE,ports.item,7,25);
    REQUIRE(board.quotedCost()==25);
    context["world_epoch"]="2";
    CHECK_FALSE(source.current(context));
    board.startInvOp(ports.panel());
    if(source.observe(context)){ board.invalidateQuote(); }
    CHECK(board.aiInvOp().invOp==INVOP_TRADE); CHECK(board.show());
    CHECK(source.current(context)); CHECK_FALSE(board.quotedCost());
    CHECK_FALSE(board.submitInvOp(ports.item,7));
    REQUIRE(board.queryInvOp(ports.item,7)==aibridge::ShopQuery::Start::Sent);
    board.setInvOpCost(INVOP_TRADE,ports.item,7,30);
    CHECK(board.quotedCost()==30);
    g_itemDB=nullptr; g_progUseDB=fixture.images.get();
}

TEST_CASE("Inventory quote survives the scripts identical post-price panel replay")
{
    GuiFixture fixture;
    ItemImages images; g_progUseDB=&images; g_itemDB=&images;
    InventoryPorts ports; ports.add();
    InventoryBoard board({.shopAccess=ports.access()});
    board.startInvOp(ports.panel()); board.setShow(true);
    const auto revision=board.invOpRevision();
    REQUIRE(board.queryInvOp(ports.item,7)==aibridge::ShopQuery::Start::Sent);
    CHECK(board.queryInvOp(ports.item,7)==aibridge::ShopQuery::Start::Coalesced);
    CHECK(ports.events.size()==1);
    board.setInvOpCost(INVOP_TRADE,ports.item,7,25);
    board.startInvOp(ports.panel());
    CHECK(board.invOpRevision()==revision); CHECK(board.quotedCost()==25);
    REQUIRE(board.submitInvOp(ports.item,7));
    CHECK(ports.events.size()==2);
    CHECK(ports.events.back()=="COMMIT:"+std::to_string(ports.item)+":7");
    CHECK_FALSE(board.submitInvOp(ports.item,7));
    CHECK(ports.events.size()==2);
    g_itemDB=nullptr; g_progUseDB=fixture.images.get();
}

TEST_CASE("Inventory late quote cannot price a new NPC or a changed original unit")
{
    GuiFixture fixture;
    ItemImages images; g_progUseDB=&images; g_itemDB=&images;
    InventoryPorts ports; ports.add();
    InventoryBoard board({.shopAccess=ports.access()});
    board.startInvOp(ports.panel()); board.setShow(true);
    REQUIRE(board.queryInvOp(ports.item,7)==aibridge::ShopQuery::Start::Sent);
    board.startInvOp(ports.panel(44));
    CHECK(board.queryInvOp(ports.item,7)==aibridge::ShopQuery::Start::Busy);
    board.setInvOpCost(INVOP_TRADE,ports.item,7,25);
    CHECK_FALSE(board.quotedCost());
    REQUIRE(board.queryInvOp(ports.item,7)==aibridge::ShopQuery::Start::Sent);
    board.setInvOpCost(INVOP_TRADE,ports.item,7,25);
    ports.pack.remove({.itemID=ports.item,.seqID=7,.count=1});
    CHECK_FALSE(board.submitInvOp(ports.item,7));
    CHECK(ports.events.size()==2);
    g_itemDB=nullptr; g_progUseDB=fixture.images.get();
}

TEST_CASE("Inventory quote expires and hidden panel cannot commit")
{
    GuiFixture fixture;
    ItemImages images; g_progUseDB=&images; g_itemDB=&images;
    InventoryPorts ports; ports.add();
    InventoryBoard board({.shopAccess=ports.access()});
    board.startInvOp(ports.panel()); board.setShow(true);
    REQUIRE(board.queryInvOp(ports.item,7)==aibridge::ShopQuery::Start::Sent);
    board.setInvOpCost(INVOP_TRADE,ports.item,7,25);
    board.setShow(false);
    CHECK_FALSE(board.submitInvOp(ports.item,7));
    board.setShow(true); ports.now+=30000;
    CHECK_FALSE(board.submitInvOp(ports.item,7));
    CHECK(ports.events.size()==1);
    g_itemDB=nullptr; g_progUseDB=fixture.images.get();
}

