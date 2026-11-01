#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <climits>
#include <stdexcept>
#include <utility>
#include "clientargparser.hpp"
#include "fontexdb.hpp"
#include "log.hpp"
#include "sdldevice.hpp"
#include "pngtexdb.hpp"
#include "layoutboard.hpp"
#include "inputstringboard.hpp"
#include "aibridge/tradeadapter.hpp"
#include "aibridge/tradesource.hpp"
#include "gui/directtradeboard/directtradeboard.hpp"
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
    struct FixtureImages: PNGTexDB
    {
        FixtureImages(): PNGTexDB(16 * 1024 * 1024) {}
        ~FixtureImages() override { clear(); }
        std::optional<std::tuple<PNGTexElement, size_t>> loadResource(uint32_t key) override
        {
            // Real widgets read texture dimensions even when tests do not draw them.
            const auto [width,height]=key==0X00002010 ? std::pair{485,330}
                : key==0X07000000 ? std::pair{359,273} : std::pair{24,24};
            auto texture=SDL_CreateTexture(g_sdlDevice->getRenderer(),SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_STATIC,width,height);
            if(!texture){ throw std::runtime_error(SDL_GetError()); }
            return std::tuple{PNGTexElement{texture},size_t(width)*height*4};
        }
    };
    struct GuiFixture
    {
        std::unique_ptr<ClientArgParser> args;
        std::unique_ptr<Log> log;
        std::unique_ptr<SDLDevice> device;
        std::unique_ptr<TestFonts> fonts;
        std::unique_ptr<FixtureImages> images;
        GuiFixture()
        {
            char name[]="trade_gui_test", audio[]="--disable-audio", profiler[]="--disable-profiler";
            char *argv[]={name,audio,profiler};
            const argf::parser parser(3,argv);
            args=std::make_unique<ClientArgParser>(parser); g_clientArgParser=args.get();
            logDisableProfiler();
            log=std::make_unique<Log>("mir2x-trade-gui-test","."); g_mir2xLog=log.get();
            device=std::make_unique<SDLDevice>(); g_sdlDevice=device.get(); device->createMainWindow();
            fonts=std::make_unique<TestFonts>(); g_fontexDB=fonts.get();
            const char *overridePath=std::getenv("MIR2X_TEST_RES_DIR");
            const std::string res=overridePath ? overridePath : MIR2X_TEST_DEFAULT_RES_DIR;
            fonts->load((res+"/font/fontex.zsdb").c_str());
            images=std::make_unique<FixtureImages>(); g_progUseDB=images.get();
        }
        ~GuiFixture()
        {
            images.reset(); fonts.reset(); device.reset(); log.reset(); args.reset();
            g_progUseDB=nullptr; g_fontexDB=nullptr; g_sdlDevice=nullptr; g_mir2xLog=nullptr; g_clientArgParser=nullptr;
        }
    };
}

TEST_CASE("Trade invitation GUI binds peer and exact revision")
{
    GuiFixture fixture;
    InputStringBoard board(DIR_UPLEFT,0,0,false);
    int accepted=0,rejected=0;
    board.waitTradeChoice(UINT64_MAX,u8"<layout><par>trade</par></layout>",[&]{++accepted;},[&]{++rejected;});
    const auto revision=board.revision();
    CHECK(board.tradePeerUid()==UINT64_MAX);
    CHECK_FALSE(board.npcInput());
    CHECK_FALSE(board.respondTradeChoice(revision,1,true));
    CHECK_FALSE(board.respondTradeChoice(revision-1,UINT64_MAX,true));
    CHECK(accepted==0);
    CHECK_FALSE(board.respondTradeChoice(revision,UINT64_MAX,true,[]{ return false; }));
    CHECK(board.show()); CHECK(board.revision()==revision); CHECK(accepted==0);
    REQUIRE(board.respondTradeChoice(revision,UINT64_MAX,true));
    CHECK(accepted==1); CHECK(rejected==0);
    CHECK_FALSE(board.show()); CHECK(board.tradePeerUid()==0);
    CHECK_FALSE(board.respondTradeChoice(revision,UINT64_MAX,true));
}

TEST_CASE("Trade rejection uses the original callback exactly once")
{
    GuiFixture fixture;
    InputStringBoard board(DIR_UPLEFT,0,0,false);
    int rejected=0;
    board.waitTradeChoice(99,u8"<layout><par>trade</par></layout>",[]{},[&]{++rejected;});
    const auto revision=board.revision();
    REQUIRE(board.respondTradeChoice(revision,99,false));
    CHECK(rejected==1);
    CHECK_FALSE(board.respondTradeChoice(revision,99,false));
    CHECK(rejected==1);
}

TEST_CASE("Generic and NPC input replacement invalidate trade invitation")
{
    GuiFixture fixture;
    InputStringBoard board(DIR_UPLEFT,0,0,false);
    int oldCalls=0;
    board.waitTradeChoice(99,u8"<layout><par>trade</par></layout>",[&]{++oldCalls;},[]{});
    auto revision=board.revision();
    board.waitChoice(u8"<layout><par>generic</par></layout>",false,[]{});
    CHECK(board.tradePeerUid()==0);
    CHECK_FALSE(board.respondTradeChoice(revision,99,true));
    board.waitTradeChoice(99,u8"<layout><par>trade</par></layout>",[&]{++oldCalls;},[]{});
    revision=board.revision();
    board.waitInput({.layoutString=u8"<layout><par>NPC</par></layout>",.onAccept=[](auto){},.npcUid=7});
    CHECK_FALSE(board.respondTradeChoice(revision,99,false));
    REQUIRE(board.npcInput()); CHECK(oldCalls==0);
}

TEST_CASE("Trade response clears old state before reentrant callback")
{
    GuiFixture fixture;
    InputStringBoard board(DIR_UPLEFT,0,0,false);
    int freshCalls=0;
    board.waitTradeChoice(99,u8"<layout><par>old</par></layout>",[&]{
        board.waitTradeChoice(100,u8"<layout><par>new</par></layout>",[&]{++freshCalls;},[]{});
    },[]{});
    const auto oldRevision=board.revision();
    REQUIRE(board.respondTradeChoice(oldRevision,99,true));
    CHECK(board.show()); CHECK(board.tradePeerUid()==100);
    CHECK_FALSE(board.respondTradeChoice(oldRevision,99,true));
    REQUIRE(board.respondTradeChoice(board.revision(),100,true));
    CHECK(freshCalls==1);
}

TEST_CASE("Hidden invitation cannot be answered through the bridge")
{
    GuiFixture fixture;
    InputStringBoard board(DIR_UPLEFT,0,0,false);
    int calls=0;
    board.waitTradeChoice(99,u8"<layout><par>trade</par></layout>",[&]{++calls;},[]{});
    const auto revision=board.revision();
    board.setShow(false);
    CHECK(board.tradePeerUid()==0);
    CHECK_FALSE(board.respondTradeChoice(revision,99,true));
    CHECK(calls==0);
}

TEST_CASE("Actual trade DTO preserves uint64 peer identity and pending versus ack")
{
    aibridge::TradePanelSnapshot panel{.handle="trade-real",.peerUid=UINT64_MAX};
    panel.local.revision=UINT64_MAX;
    panel.local.gold=INT_MAX;
    panel.local.pending=true;
    panel.local.items.push_back({1,2,3,{}});
    const auto value=panel.json();
    CHECK(value.at("peer_uid")=="18446744073709551615");
    CHECK(value.at("local").at("revision")=="18446744073709551615");
    CHECK(value.at("local").at("gold")=="2147483647");
    CHECK(value.at("local").at("pending")==true);
    CHECK(value.at("local").at("items")[0].at("name").is_null());
    CHECK(value.at("local").at("locked")==false);
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
    struct TradePorts
    {
        InvPack inventory;
        uint64_t gold=1000;
        unsigned commits=0,cancels=0;
        std::vector<SDDirectTradeOffer> sent;
        DirectTradeBoard::Access access()
        {
            return {
                .inventory=[this]{ return &inventory; },
                .gold=[this]() -> std::optional<uint64_t>{ return gold; },
                .offer=[this](uint64_t,uint32_t value,bool locked,const std::vector<SDItem> &items){
                    sent.push_back({.uid=1,.gold=value,.locked=locked,.itemList=items}); return true;
                },
                .commit=[this](uint64_t){ ++commits; },
                .cancel=[this](uint64_t){ ++cancels; },
                .localName=[]{ return std::string("fixture"); },
                .itemTooLarge=[]{ FAIL("unexpected oversized test item"); },
            };
        }
    };
}

TEST_CASE("Actual trade board gates lock and commit on canonical offer acknowledgements")
{
    GuiFixture fixture;
    TradePorts ports;
    DirectTradeBoard board({.access=ports.access()});
    board.beginTrade(99,"peer");
    REQUIRE(board.replaceOffer(100,{}));
    CHECK(board.offerPending()); CHECK(board.localRevision()==0);
    CHECK_FALSE(board.lockOffer()); CHECK_FALSE(board.replaceOffer(101,{}));
    auto wrong=ports.sent.back(); wrong.gold=101;
    board.applyLocalOfferAck(wrong);
    CHECK(board.offerPending()); CHECK(board.localRevision()==0);
    board.applyLocalOfferAck(ports.sent.back());
    CHECK_FALSE(board.offerPending()); CHECK(board.localRevision()==1);
    REQUIRE(board.lockOffer());
    CHECK_FALSE(board.confirmTrade());
    board.applyLocalOfferAck(ports.sent.back());
    CHECK(board.localLocked()); CHECK(board.localRevision()==2);
    CHECK_FALSE(board.confirmTrade());
    board.setPeerOffer({.uid=99,.locked=true});
    CHECK(board.peerRevision()==1);
    REQUIRE(board.confirmTrade()); CHECK(ports.commits==1);
    board.rejectLocalOffer();
    const auto revision=board.localRevision();
    board.applyLocalOfferAck(ports.sent.back());
    CHECK(board.localRevision()==revision);
    CHECK_FALSE(board.confirmTrade()); CHECK(ports.commits==1);
    CHECK(board.localConfirmed()); CHECK(board.peerUID()==99);
}

TEST_CASE("Actual trade board keeps reserved items while cancel or commit is unconfirmed")
{
    GuiFixture fixture;
    ItemImages images; g_itemDB=&images;
    struct Reset { ~Reset(){ g_itemDB=nullptr; } } reset;
    TradePorts ports;
    const auto itemId=DBCOM_ITEMID(u8"金创药（小）");
    ports.inventory.add(SDItem{.itemID=itemId,.seqID=77,.count=10},false);
    DirectTradeBoard board({.access=ports.access()});
    board.beginTrade(99,"peer");
    REQUIRE(board.replaceOffer(100,{{.itemID=itemId,.seqID=77,.count=6}}));
    REQUIRE(ports.inventory.getPackBinList().size()==1);
    CHECK(ports.inventory.getPackBinList()[0].item.count==4);
    CHECK(board.localItemList()[0].count==6);
    board.applyLocalOfferAck(ports.sent.back());
    REQUIRE(board.lockOffer()); board.applyLocalOfferAck(ports.sent.back());
    board.setPeerOffer({.uid=99,.locked=true});
    REQUIRE(board.confirmTrade());
    REQUIRE(board.requestClose());
    CHECK_FALSE(board.requestClose()); CHECK(ports.cancels==1);
    CHECK(board.peerUID()==99); CHECK(board.localItemList()[0].count==6);
    CHECK(ports.inventory.getPackBinList()[0].item.count==4);
    // A server completion wins the race with cancellation; never return the old six items.
    board.completeTrade();
    CHECK(board.localItemList().empty()); CHECK(board.peerUID()==0);
    CHECK(ports.inventory.getPackBinList()[0].item.count==4);
}

TEST_CASE("Actual trade replacement validates all identities before mutating reservations")
{
    GuiFixture fixture;
    ItemImages images; g_itemDB=&images;
    struct Reset { ~Reset(){ g_itemDB=nullptr; } } reset;
    TradePorts ports;
    const auto itemId=DBCOM_ITEMID(u8"金创药（小）");
    ports.inventory.add(SDItem{.itemID=itemId,.seqID=77,.count=10},false);
    DirectTradeBoard board({.access=ports.access()});
    board.beginTrade(99,"peer");
    CHECK_FALSE(board.replaceOffer(10,{{.itemID=itemId,.seqID=77,.count=6},{.itemID=itemId,.seqID=77,.count=1}}));
    CHECK_FALSE(board.replaceOffer(10,{{.itemID=itemId,.seqID=77,.count=11}}));
    CHECK(ports.sent.empty()); CHECK(ports.inventory.getPackBinList()[0].item.count==10);
    REQUIRE(board.replaceOffer(10,{{.itemID=itemId,.seqID=77,.count=6}}));
    board.applyLocalOfferAck(ports.sent.back());
    REQUIRE(board.replaceOffer(20,{{.itemID=itemId,.seqID=77,.count=3}}));
    CHECK(ports.inventory.getPackBinList()[0].item.count==7);
    CHECK(board.localItemList()[0].count==3);
    REQUIRE(board.requestClose());
    CHECK(ports.inventory.getPackBinList()[0].item.count==7);
    board.closeTrade(false);
    CHECK(ports.inventory.getPackBinList()[0].item.count==10);
    CHECK(board.localItemList().empty());
}

TEST_CASE("Trade source authority is invalidated by world session and RUN exit")
{
    using namespace aibridge;
    Json context={{"instance_id",std::string(32,'a')},{"session_id",std::string(32,'b')},
        {"world_epoch","1"},{"control_epoch","1"}};
    TradeSources sources;
    sources.sync(context);
    CHECK_FALSE(sources.trade(context,99,7));
    sources.started(context,99,7); sources.invited(context,99,11);
    REQUIRE(sources.trade(context,99,7)); REQUIRE(sources.invitation(context,99,11));
    const TradeSourceStamp pending{economyScope(context),99,7};
    auto changed=context; changed["control_epoch"]="2";
    sources.sync(changed);
    CHECK(sources.trade(changed,99,7));
    CHECK(TradeSources::matches(pending,changed,99,7));
    CHECK_FALSE(TradeSources::matches(pending,changed,99,8));
    CHECK_FALSE(TradeSources::matches(pending,changed,100,7));
    changed["world_epoch"]="2"; sources.sync(changed);
    CHECK_FALSE(sources.trade(changed,99,7)); CHECK_FALSE(sources.invitation(changed,99,11));
    CHECK_FALSE(TradeSources::matches(pending,changed,99,7));
    sources.sync(changed); sources.sync(changed);
    CHECK_FALSE(sources.trade(changed,99,7));
    sources.started(changed,99,8); sources.invited(changed,99,12);
    changed["session_id"]=std::string(32,'c'); sources.sync(changed);
    CHECK_FALSE(sources.trade(changed,99,8)); CHECK_FALSE(sources.invitation(changed,99,12));
    sources.started(changed,99,9);
    sources.sync(nullptr); sources.sync(changed);
    CHECK_FALSE(sources.trade(changed,99,9));
}

TEST_CASE("Actual trade send boundary rejects expired prepared offers and lost authorization")
{
    using namespace aibridge;
    GuiFixture fixture;
    ItemImages images; g_itemDB=&images;
    struct Reset { ~Reset(){ g_itemDB=nullptr; } } reset;
    TradePorts ports;
    const auto itemId=DBCOM_ITEMID(u8"金创药（小）");
    ports.inventory.add(SDItem{.itemID=itemId,.seqID=77,.count=10},false);
    DirectTradeBoard board({.access=ports.access()});
    board.beginTrade(99,"peer");
    REQUIRE(board.replaceOffer(100,{{.itemID=itemId,.seqID=77,.count=2}}));
    board.applyLocalOfferAck(ports.sent.back());
    const auto revision=board.localRevision();
    const Json context={{"instance_id",std::string(32,'a')},{"session_id",std::string(32,'b')},
        {"world_epoch","1"},{"control_epoch","1"}};
    const OperationView view{.operationId="trade-send",.context=context,.deadlineMs=1000};
    unsigned checks=0;
    CHECK_FALSE(board.replaceOffer(200,{{.itemID=itemId,.seqID=77,.count=6}},[&]{
        return !validateEconomicSend(view,context,true,++checks==1 ? 999 : 1000);
    }));
    CHECK(checks==2); CHECK(ports.sent.size()==1);
    REQUIRE(board.localItemList().size()==1); CHECK(board.localItemList()[0].count==2);
    REQUIRE(ports.inventory.getPackBinList().size()==1); CHECK(ports.inventory.getPackBinList()[0].item.count==8);
    CHECK(board.localGold()==100); CHECK(board.localRevision()==revision);
    CHECK_FALSE(board.offerPending()); CHECK(board.localOfferAcknowledged());

    auto current=context;
    uint64_t now=1000;
    bool active=true;
    const auto canSend=[&]{ return !validateEconomicSend(view,current,active,now); };
    CHECK_FALSE(board.lockOffer(canSend)); CHECK_FALSE(board.offerPending());
    CHECK(ports.sent.size()==1);
    now=999;
    REQUIRE(board.lockOffer(canSend)); board.applyLocalOfferAck(ports.sent.back());
    board.setPeerOffer({.uid=99,.locked=true});
    active=false;
    CHECK_FALSE(board.confirmTrade(canSend)); CHECK_FALSE(board.localConfirmed()); CHECK(ports.commits==0);
    active=true; current["control_epoch"]="2";
    CHECK_FALSE(board.confirmTrade(canSend)); CHECK(ports.commits==0);
    current=context;
    REQUIRE(board.confirmTrade(canSend)); CHECK(ports.commits==1);
    now=1000;
    CHECK_FALSE(board.requestClose(canSend)); CHECK_FALSE(board.closePending()); CHECK(ports.cancels==0);
    now=999;
    REQUIRE(board.requestClose(canSend)); CHECK(ports.cancels==1);
}

TEST_CASE("Real trade and invitation GUI cannot authorize retained panels in a new world")
{
    GuiFixture fixture;
    TradePorts ports;
    DirectTradeBoard board({.access=ports.access()});
    InputStringBoard input(DIR_UPLEFT,0,0,false);
    aibridge::Json context={{"instance_id",std::string(32,'a')},{"session_id",std::string(32,'b')},
        {"world_epoch","1"},{"control_epoch","1"}};
    aibridge::TradeSources sources;
    board.beginTrade(99,"peer");
    input.waitTradeChoice(99,u8"<layout><par>trade</par></layout>",[]{},[]{});
    sources.started(context,board.peerUID(),board.generation());
    sources.invited(context,input.tradePeerUid(),input.revision());
    REQUIRE(sources.trade(context,board.peerUID(),board.generation()));
    REQUIRE(sources.invitation(context,input.tradePeerUid(),input.revision()));
    context["world_epoch"]="2";
    sources.sync(context);
    REQUIRE(board.peerUID()==99); REQUIRE(input.tradePeerUid()==99);
    for(int i=0;i<3;++i){
        sources.sync(context);
        CHECK_FALSE(sources.trade(context,board.peerUID(),board.generation()));
        CHECK_FALSE(sources.invitation(context,input.tradePeerUid(),input.revision()));
    }
    // Only another real source notification can qualify new GUI generations.
    board.beginTrade(99,"peer");
    sources.started(context,board.peerUID(),board.generation());
    CHECK(sources.trade(context,board.peerUID(),board.generation()));
    CHECK_FALSE(sources.invitation(context,input.tradePeerUid(),input.revision()));
    input.waitTradeChoice(99,u8"<layout><par>new trade</par></layout>",[]{},[]{});
    sources.invited(context,input.tradePeerUid(),input.revision());
    CHECK(sources.invitation(context,input.tradePeerUid(),input.revision()));
}
