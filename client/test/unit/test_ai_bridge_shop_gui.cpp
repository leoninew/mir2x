#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include "aibridge/runtime.hpp"
#include "aibridge/npcadapter.hpp"
#include "aibridge/shopadapter.hpp"
#include "bgmusicdb.hpp"
#include "cerealf.hpp"
#include "client.hpp"
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
#include "emojidb.hpp"
#include "imeboard.hpp"
#include "mapbindb.hpp"
#include "messagestackboard.hpp"
#include "pngtexoffdb.hpp"
#include "processrun.hpp"
#include "soundeffectdb.hpp"
#ifdef CHECK
#undef CHECK
#endif
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



struct AiCombatTestAccess
{
    static void attach(Client &client, std::unique_ptr<ProcessRun> scene)
    {
        client.m_currentProcess = std::move(scene);
        client.m_aiRuntime = std::make_unique<aibridge::AiBridgeRuntime>(client);
    }
    static void detach(Client &client)
    {
        client.m_aiRuntime.reset();
        client.m_currentProcess.reset();
    }
};

namespace
{
    struct ShopGuiResources
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

        ShopGuiResources()
        {
            const char *overridePath = std::getenv("MIR2X_TEST_RES_DIR");
            std::string resourceArg = "--res-path=" + std::string(overridePath ? overridePath : MIR2X_TEST_DEFAULT_RES_DIR);
            char name[] = "shop_close_test", audio[] = "--disable-audio", profiler[] = "--disable-profiler";
            char bridge[] = "--enable-ai-bridge=0";
            char *argv[] = {name, audio, profiler, bridge, resourceArg.data()};
            args = std::make_unique<ClientArgParser>(argf::parser(5, argv)); g_clientArgParser = args.get();
            logDisableProfiler();
            log = std::make_unique<Log>("mir2x-shop-close-test", "."); g_mir2xLog = log.get();
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
        ~ShopGuiResources()
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

    ShopGuiResources &shopResources()
    {
        static ShopGuiResources fixture;
        return fixture;
    }

    struct ShopAdapterScene
    {
        ShopGuiResources &resources = shopResources();
        ProcessRun *scene = nullptr;
        std::filesystem::path root;
        unsigned requestSequence = 0;
        const uint32_t item = DBCOM_ITEMID(u8"金创药（小）");
        const uint64_t npc = uidf::buildNPCUID(1, 0);

        explicit ShopAdapterScene(bool allowClose = true)
        {
            SMOnlineOK initial{};
            initial.uid = uidf::getPlayerUID(8);
            initial.gender = 1;
            initial.job = JOB_WARRIOR;
            initial.mapUID = uidf::getBaseMapUID(DBCOM_MAPID(u8"道馆_1"), 0);
            initial.action = ActionStand{.direction = DIR_RIGHT, .x = 430, .y = 108};
            auto process = std::make_unique<ProcessRun>(initial);
            scene = process.get();
            AiCombatTestAccess::attach(*resources.client, std::move(process));
            const auto start = cerealf::serialize(SDStartGameScene{
                .uid = initial.uid, .mapUID = initial.mapUID, .x = 430, .y = 108, .name = "shop-close-test"});
            scene->on_SM_STARTGAMESCENE(reinterpret_cast<const uint8_t *>(start.data()), start.size());
            const auto hp = cerealf::serialize(SDHealth{.uid = initial.uid, .hp = 300, .maxHP = 300});
            scene->on_SM_HEALTH(reinterpret_cast<const uint8_t *>(hp.data()), hp.size());
            for(int i = 0; i < 100 && !scene->getMyHero()->aiMotionStopped(); ++i){ scene->getMyHero()->update(100); }
            REQUIRE(scene->getMyHero()->aiMotionStopped());
            const auto inv = cerealf::serialize(SDInventory{});
            scene->on_SM_INVENTORY(reinterpret_cast<const uint8_t *>(inv.data()), inv.size());
            scene->getMyHero()->setGold(1000);
            root = std::filesystem::current_path() / ("shop-adapter-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
            std::filesystem::create_directory(root);
            const auto policy = root / "policy.json";
            aibridge::Json config = {{"allowed_actions", {"game_shop_buy"}}, {"allowed_item_ids", {item}},
                {"max_gold_per_operation", "100"}, {"max_gold_per_session", "1000"}, {"max_item_count_per_operation", 1}};
            if(allowClose){ config["allowed_actions"].push_back("game_shop_close"); }
            { std::ofstream out(policy); out << config.dump(); }
            runtime().start({root / "bridge", policy, "shop-close-test"});
            runtime().observe(scene->aiState());
            controller().onConnected(1);
            REQUIRE(controller().controlState() == aibridge::ControlState::Active);
        }
        ~ShopAdapterScene() { AiCombatTestAccess::detach(*resources.client); }
        aibridge::AiBridgeRuntime &runtime() { return *resources.client->aiRuntime(); }
        aibridge::AiBridgeController &controller() { return *runtime().controller(); }
        PurchaseBoard &board() { return *dynamic_cast<PurchaseBoard *>(scene->getWidget("PurchaseBoard")); }
        std::string open(uint64_t source = 0)
        {
            const auto message = cerealf::serialize(SDNPCSell{.npcUID = source ? source : npc, .itemList = {item}});
            scene->on_SM_NPCSELL(reinterpret_cast<const uint8_t *>(message.data()), message.size());
            REQUIRE(board().show());
            const auto handle = runtime().npc()->panelHandle(aibridge::NpcPanelKind::Shop);
            REQUIRE_FALSE(handle.empty());
            return handle;
        }
        void details(uint64_t source = 0)
        {
            const auto message = cerealf::serialize(SDSellItemList{.npcUID = source ? source : npc,
                .list = {{.item = {.itemID = item, .seqID = 0, .count = 1}, .costList = {{DBCOM_ITEMID(u8"金币（小）"), 80}}}}});
            scene->on_SM_SELLITEMLIST(reinterpret_cast<const uint8_t *>(message.data()), message.size());
        }
        aibridge::Json query(const std::string &method, aibridge::Json params = aibridge::Json::object())
        {
            return controller().handleRequest({1, {{"v", 1}, {"id", "query"}, {"method", method}, {"params", params}}});
        }
        aibridge::Json action(const std::string &method, aibridge::Json params, aibridge::Json context = nullptr, int timeout = 5000)
        {
            const auto suffix = std::to_string(++requestSequence);
            return controller().handleRequest({1, {{"v", 1}, {"id", "action"}, {"method", method}, {"params", params},
                {"context", context.is_null() ? controller().context() : context}, {"timeout_ms", timeout},
                {"request_id", "00000000-0000-4000-8000-" + std::string(12 - suffix.size(), '0') + suffix}}});
        }
        aibridge::Json status(const std::string &id)
        {
            return query("game_get_operation", {{"operation_id", id}}).at("data");
        }
        aibridge::Json close(const std::string &handle)
        {
            const auto accepted = action("game_shop_close", {{"shop_handle", handle}});
            INFO(accepted.dump());
            REQUIRE(accepted.at("ok") == true);
            REQUIRE(accepted.at("data").at("state") == "accepted");
            controller().advance();
            return status(accepted.at("data").at("operation_id"));
        }
        std::string quote()
        {
            const SMAction spawn{.UID = npc, .mapUID = scene->mapUID(),
                .action = ActionStand{.direction = DIR_DOWN, .x = 431, .y = 108}};
            scene->on_SM_ACTION(reinterpret_cast<const uint8_t *>(&spawn), sizeof(spawn));
            REQUIRE(scene->findUID(npc, true));
            const auto handle = open();
            REQUIRE(board().queryDetails(item) == aibridge::ShopQuery::Start::Sent);
            details();
            const auto listed = query("game_shop_list", {{"shop_handle", handle}, {"item_id", item}});
            INFO(listed.dump());
            REQUIRE(listed.at("ok") == true);
            REQUIRE(listed.at("data").at("items").size() == 1);
            return listed.at("data").at("items").at(0).at("quote_handle").get<std::string>();
        }
    };

    void clickShopX(PurchaseBoard &board)
    {
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.x = 259;
        event.motion.y = 185;
        board.processEvent(event, true, {});
        event = {};
        event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        event.button.button = SDL_BUTTON_LEFT;
        event.button.clicks = 1;
        event.button.x = 259;
        event.button.y = 185;
        board.processEvent(event, true, {});
        event.type = SDL_EVENT_MOUSE_BUTTON_UP;
        board.processEvent(event, true, {});
    }
}

TEST_CASE("Shop close native SDL X hides main and details without another query or buy")
{
    ShopGuiResources resources;
    ShopPorts ports;
    PurchaseBoard board(nullptr, nullptr, false, ports.access());
    board.loadSell(33, {ports.item}); board.setShow(true);
    REQUIRE(board.queryDetails(ports.item) == aibridge::ShopQuery::Start::Sent);
    board.setSellItemList(ports.reply());
    REQUIRE_FALSE(board.details().list.empty());
    REQUIRE(board.submitBuy(33, ports.item, 0, 1));
    clickShopX(board);
    CHECK_FALSE(board.show());
    CHECK(board.details().list.empty());
    CHECK(board.detailCache().empty());
    CHECK(board.buyAttempts(33, ports.item, 0) == 1);
    CHECK(ports.queries == 1);
    CHECK(ports.buys == 1);
}

TEST_CASE("Shop close actual adapter closes a shop without dialogue and preserves unrelated panels")
{
    ShopAdapterScene test;
    test.open();
    REQUIRE(test.board().queryDetails(test.item) == aibridge::ShopQuery::Start::Sent);
    test.details();
    REQUIRE_FALSE(test.board().details().list.empty());
    auto inventory = test.scene->getWidget("InventoryBoard");
    inventory->setShow(true);
    const auto interaction = test.query("game_get_interaction");
    REQUIRE(interaction.at("data").at("interaction").at("dialogue").is_null());
    const auto handle = interaction.at("data").at("interaction").at("shop_handle").get<std::string>();
    const auto budget = test.controller().session().at("capabilities").at("policy");
    const auto done = test.close(handle);
    CHECK(done.at("state") == "succeeded");
    CHECK(done.at("completion_scope") == "client_state");
    CHECK(done.at("side_effects_possible") == false);
    CHECK(done.at("result").at("submitted") == false);
    REQUIRE(done.at("evidence").size() == 1);
    CHECK(done.at("evidence").at(0).at("source") == "client");
    CHECK_FALSE(test.board().show());
    CHECK(test.board().details().list.empty());
    CHECK(test.board().detailCache().empty());
    CHECK(inventory->show());
    CHECK(test.query("game_get_interaction").at("data").at("interaction").at("shop_handle").is_null());
    CHECK(test.controller().session().at("capabilities").at("policy") == budget);
    CHECK(test.board().buyAttempts(test.npc, test.item, 0) == 0);
}

TEST_CASE("Shop close actual adapter retains pending query until late reply drains")
{
    ShopAdapterScene test;
    const auto handle = test.open();
    REQUIRE(test.board().queryDetails(test.item) == aibridge::ShopQuery::Start::Sent);
    REQUIRE(test.board().detailQuery().pending());
    REQUIRE(test.close(handle).at("state") == "succeeded");
    REQUIRE(test.board().detailQuery().pending());
    const auto replacement = uidf::buildNPCUID(2, 0);
    test.open(replacement);
    CHECK(test.board().queryDetails(test.item) == aibridge::ShopQuery::Start::Busy);
    test.details();
    CHECK_FALSE(test.board().detailQuery().pending());
    CHECK(test.board().details().list.empty());
    CHECK(test.board().detailCache().empty());
    REQUIRE(test.board().queryDetails(test.item) == aibridge::ShopQuery::Start::Sent);
    test.details(replacement);
    REQUIRE(test.board().detailCache().size() == 1);
    CHECK(test.board().details().npcUID == replacement);
}

TEST_CASE("Shop close actual adapter rejects stale source context and paused control")
{
    ShopAdapterScene test;
    auto handle = test.open();
    std::string expected;
    aibridge::Json context = test.controller().context();
    SECTION("replacement shop") { test.open(uidf::buildNPCUID(2, 0)); expected = "stale_target"; }
    SECTION("wrong epoch") { context["control_epoch"] = "999"; expected = "stale_context"; }
    SECTION("manual pause") { test.controller().manualInput(); context = test.controller().context(); expected = "paused"; }
    SECTION("death") {
        auto state = test.scene->aiState(); state.alive = false;
        test.controller().observe(state); context = test.controller().context(); expected = "paused";
    }
    const auto response = test.action("game_shop_close", {{"shop_handle", handle}}, context);
    INFO(response.dump());
    REQUIRE(response.at("ok") == false);
    CHECK(response.at("error").at("code") == expected);
    CHECK(test.board().show());
}

TEST_CASE("Shop close actual adapter requires policy and revalidates admitted source")
{
    SECTION("no permission") {
        ShopAdapterScene test(false);
        const auto response = test.action("game_shop_close", {{"shop_handle", test.open()}});
        REQUIRE(response.at("ok") == false);
        CHECK(response.at("error").at("code") == "policy_denied");
        CHECK(test.board().show());
    }
    SECTION("replacement after admission") {
        ShopAdapterScene test;
        const auto response = test.action("game_shop_close", {{"shop_handle", test.open()}});
        REQUIRE(response.at("ok") == true);
        test.open(uidf::buildNPCUID(2, 0));
        test.controller().advance();
        const auto done = test.status(response.at("data").at("operation_id"));
        CHECK(done.at("state") == "failed");
        CHECK(done.at("error").at("code") == "stale_target");
        CHECK(done.at("side_effects_possible") == false);
        CHECK(test.board().show());
    }
    SECTION("deadline before begin") {
        ShopAdapterScene test;
        const auto response = test.action("game_shop_close", {{"shop_handle", test.open()}}, nullptr, 100);
        REQUIRE(response.at("ok") == true);
        SDL_Delay(110);
        test.controller().checkDeadlines();
        test.controller().advance();
        const auto done = test.status(response.at("data").at("operation_id"));
        CHECK(done.at("state") == "failed");
        CHECK(done.at("error").at("code") == "timeout");
        CHECK_FALSE(done.at("side_effects_possible").get<bool>());
        CHECK(test.board().show());
    }
    SECTION("manual input after admission") {
        ShopAdapterScene test;
        const auto response = test.action("game_shop_close", {{"shop_handle", test.open()}});
        REQUIRE(response.at("ok") == true);
        test.controller().manualInput();
        test.controller().advance();
        CHECK(test.status(response.at("data").at("operation_id")).at("state") == "cancelled");
        CHECK(test.board().show());
    }
}

TEST_CASE("Shop close actual adapter preserves unknown buy and its late receipt")
{
    ShopAdapterScene test;
    const auto quote = test.quote();
    const auto handle = test.runtime().npc()->panelHandle(aibridge::NpcPanelKind::Shop);
    const auto buy = test.action("game_shop_buy", {{"quote_handle", quote}, {"count", 1}, {"max_total_gold", "80"}});
    INFO(buy.dump());
    REQUIRE(buy.at("ok") == true);
    const auto id = buy.at("data").at("operation_id").get<std::string>();
    test.controller().advance();
    REQUIRE(test.status(id).at("state") == "running");
    REQUIRE(test.board().buyAttempts(test.npc, test.item, 0) == 1);
    const auto busy = test.action("game_shop_close", {{"shop_handle", handle}});
    REQUIRE(busy.at("ok") == false);
    CHECK(busy.at("error").at("code") == "busy");
    CHECK(test.board().show());
    // Inject a lost terminal observation without resending or clearing the real adapter watch.
    test.controller().report(id, {.outcome = aibridge::OperationOutcome::Unknown, .submitted = true});
    REQUIRE(test.status(id).at("state") == "unknown");
    REQUIRE(test.controller().session().at("capabilities").at("policy").at("economic_uncertain") == true);
    REQUIRE(test.close(handle).at("state") == "succeeded");
    CHECK(test.board().buyAttempts(test.npc, test.item, 0) == 1);
    CHECK(test.status(id).at("state") == "unknown");
    CHECK(test.controller().session().at("capabilities").at("policy").at("economic_uncertain") == true);
    const auto rejected = test.action("game_shop_buy", {{"quote_handle", quote}, {"count", 1}, {"max_total_gold", "80"}});
    REQUIRE(rejected.at("ok") == false);
    CHECK(rejected.at("error").at("code") == "economic_uncertain");
    const SMBuySucceed reply{.npcUID = test.npc, .itemID = test.item, .seqID = 0};
    test.scene->on_SM_BUYSUCCEED(reinterpret_cast<const uint8_t *>(&reply), sizeof(reply));
    test.runtime().shop()->observe();
    const auto after = test.status(id);
    CHECK(after.at("state") == "unknown");
    REQUIRE_FALSE(after.at("late_resolution").is_null());
    CHECK(after.at("late_resolution").at("state") == "succeeded");
    REQUIRE(after.at("late_resolution").at("evidence").size() == 1);
    CHECK(after.at("late_resolution").at("evidence").at(0).at("server_code") == "SM_BUYSUCCEED");
    CHECK_FALSE(test.board().show());
    CHECK(test.board().buyAttempts(test.npc, test.item, 0) == 1);
}

TEST_CASE("Shop close actual adapter invalidates old quotes and repeated close handles")
{
    ShopAdapterScene test;
    const auto quote = test.quote();
    const auto handle = test.runtime().npc()->panelHandle(aibridge::NpcPanelKind::Shop);
    REQUIRE(test.close(handle).at("state") == "succeeded");
    const auto buy = test.action("game_shop_buy", {{"quote_handle", quote}, {"count", 1}, {"max_total_gold", "80"}});
    REQUIRE(buy.at("ok") == false);
    CHECK(buy.at("error").at("code") == "stale_target");
    CHECK(test.board().buyAttempts(test.npc, test.item, 0) == 0);
    const auto close = test.action("game_shop_close", {{"shop_handle", handle}});
    REQUIRE(close.at("ok") == false);
    CHECK(close.at("error").at("code") == "stale_target");
    CHECK_FALSE(test.board().show());
}
