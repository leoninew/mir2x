#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <set>
#include "aibridge/equipment.hpp"
#include "aibridge/actions.hpp"
#include "clientargparser.hpp"
#include "log.hpp"
#include "myhero.hpp"
#include "pngtexdb.hpp"
#include "sdldevice.hpp"
#include "soundeffectdb.hpp"

class FontexDB;
class EmojiDB;
class PNGTexOffDB;
class MapBinDB;
class BGMusicDB;
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

using namespace aibridge;
namespace
{
    struct ItemImages: PNGTexDB
    {
        ItemImages(): PNGTexDB(16 * 1024 * 1024) {}
        ~ItemImages() override { clear(); }
    };
    struct SilentSounds: SoundEffectDB
    {
        SilentSounds(): SoundEffectDB(1024) {}
        std::optional<std::tuple<SoundEffectElement, size_t>> loadResource(uint32_t) override { return {}; }
    };
    struct GuiFixture
    {
        std::unique_ptr<ClientArgParser> args;
        std::unique_ptr<Log> log;
        std::unique_ptr<SDLDevice> device;
        std::unique_ptr<ItemImages> images;
        SilentSounds sounds;
        GuiFixture()
        {
            char name[] = "equipment_gui_test", audio[] = "--disable-audio", profiler[] = "--disable-profiler";
            char *argv[] = {name, audio, profiler};
            const argf::parser parser(3, argv);
            args = std::make_unique<ClientArgParser>(parser); g_clientArgParser = args.get();
            logDisableProfiler();
            log = std::make_unique<Log>("mir2x-equipment-gui-test", "."); g_mir2xLog = log.get();
            device = std::make_unique<SDLDevice>(); g_sdlDevice = device.get(); device->createMainWindow();
            images = std::make_unique<ItemImages>(); g_itemDB = images.get();
            const char *overridePath = std::getenv("MIR2X_TEST_RES_DIR");
            const std::string res = overridePath ? overridePath : MIR2X_TEST_DEFAULT_RES_DIR;
            images->load((res + "/texture/item.zsdb").c_str());
            g_seffDB = &sounds;
        }
        ~GuiFixture()
        {
            images.reset(); device.reset(); log.reset(); args.reset();
            g_itemDB = nullptr; g_sdlDevice = nullptr; g_mir2xLog = nullptr; g_clientArgParser = nullptr; g_seffDB = nullptr;
        }
    };
    template<typename Function> void errorCode(Function &&function, std::string_view code)
    {
        try{ function(); FAIL("Expected equipment protocol error"); }
        catch(const ProtocolError &error){ CHECK(error.code == code); }
    }
    struct EquipmentPorts
    {
        InvPack inventory;
        SDWear wear;
        EquipmentRequests requests;
        bool male = true, inventoryKnown = true, wearKnown = true, busy = false, networkAccepts = true;
        std::set<std::pair<uint32_t, uint32_t>> reserved;
        unsigned networkAttempts = 0, sends = 0;
        EquipmentSelection lastSent;
        EquipmentAccess access()
        {
            return {.inventory = &inventory, .wear = &wear, .inventoryKnown = inventoryKnown, .wearKnown = wearKnown,
                .busy = busy, .equipmentPending = requests.pending(),
                .reserved = [this](uint32_t itemId, uint32_t seqId){ return reserved.contains({itemId, seqId}); },
                .canWear = [this](uint32_t itemId, int slot){ return MyHero::canWearForGender(itemId, slot, male); }};
        }
        bool fakeNetworkSend(const EquipmentSelection &selection, EquipmentOrigin origin)
        {
            return requests.submit(selection, origin, [this, selection]
            {
                ++networkAttempts;
                if(!networkAccepts){ return false; }
                ++sends; lastSent = selection;
                return true;
            });
        }
        bool submit(const EquipmentSelection &selection, EquipmentOrigin origin = EquipmentOrigin::Ai,
            const std::function<bool()> &guard = []{ return true; })
        {
            return submitInventoryWear(access(), selection, origin, guard,
                [this, origin](const EquipmentSelection &current){ return fakeNetworkSend(current, origin); });
        }
        SDItem add(uint32_t itemId, uint32_t seqId = 7)
        {
            SDItem item{.itemID = itemId, .seqID = seqId, .count = 1};
            inventory.add(item, false);
            return item;
        }
    };
    Json context()
    {
        return {{"instance_id", std::string(32, 'a')}, {"session_id", std::string(32, 'b')},
            {"world_epoch", "1"}, {"control_epoch", "1"}};
    }
}

TEST_CASE("Equipment shared inventory path selects ordinary weapon dress and paired slots")
{
    GuiFixture fixture;
    EquipmentPorts ports;
    for(const auto &[itemId, slot]: {std::pair{DBCOM_ITEMID(u8"木剑"), WLG_WEAPON},
        std::pair{DBCOM_ITEMID(u8"布衣（男）"), WLG_DRESS}, std::pair{DBCOM_ITEMID(u8"古铜戒指"), WLG_RING0},
        std::pair{DBCOM_ITEMID(u8"铁手镯"), WLG_ARMRING0}}){
        CAPTURE(itemId, slot);
        const auto item = ports.add(itemId);
        const auto selection = prepareInventoryWear(ports.access(), itemId, item.seqID, 1);
        CHECK(selection.slot == slot);
        CHECK(ports.inventory.getPackBinList().size() >= 1);
        CHECK_FALSE(ports.wear.getWLItem(slot));
    }
    for(const auto &[itemId, first, second]: {std::tuple{DBCOM_ITEMID(u8"古铜戒指"), WLG_RING0, WLG_RING1},
        std::tuple{DBCOM_ITEMID(u8"铁手镯"), WLG_ARMRING0, WLG_ARMRING1}}){
        const auto selection = prepareInventoryWear(ports.access(), itemId, 7, 1);
        ports.wear.setWLItem(first, {.itemID = itemId, .seqID = 8, .count = 1});
        errorCode([&]{ ports.submit(selection); }, "stale_target");
        auto next = prepareInventoryWear(ports.access(), itemId, 7, 1);
        CHECK(next.slot == second);
        ports.wear.setWLItem(second, {.itemID = itemId, .seqID = 9, .count = 1});
        CHECK(prepareInventoryWear(ports.access(), itemId, 7, 1).slot == second);
    }
    CHECK(ports.sends == 0);
}

TEST_CASE("Equipment shared inventory validation rejects unknown reserved unavailable and ineligible items")
{
    GuiFixture fixture;
    EquipmentPorts ports;
    const auto itemId = DBCOM_ITEMID(u8"布衣（男）");
    ports.add(itemId);
    for(const uint32_t count: {0U, 2U}){ errorCode([&]{ prepareInventoryWear(ports.access(), itemId, 7, count); }, "invalid_argument"); }
    errorCode([&]{ prepareInventoryWear(ports.access(), itemId, 0, 1); }, "invalid_argument");
    errorCode([&]{ prepareInventoryWear(ports.access(), itemId, 8, 1); }, "invalid_argument");
    errorCode([&]{ prepareInventoryWear(ports.access(), 0, 7, 1); }, "invalid_argument");
    errorCode([&]{ prepareInventoryWear(ports.access(), DBCOM_ITEMID(u8"金创药（小）"), 7, 1); }, "unsupported_item_type");
    SECTION("unknown inventory"){ ports.inventoryKnown = false; errorCode([&]{ prepareInventoryWear(ports.access(), itemId, 7, 1); }, "unavailable"); }
    SECTION("unknown equipment"){ ports.wearKnown = false; errorCode([&]{ prepareInventoryWear(ports.access(), itemId, 7, 1); }, "unavailable"); }
    SECTION("modal busy"){ ports.busy = true; errorCode([&]{ prepareInventoryWear(ports.access(), itemId, 7, 1); }, "busy"); }
    SECTION("temporary trade or auction reservation"){
        ports.reserved.emplace(itemId, 7);
        errorCode([&]{ prepareInventoryWear(ports.access(), itemId, 7, 1); }, "busy");
    }
    SECTION("grabbed instance"){
        ports.inventory.setGrabbedItem({.itemID = itemId, .seqID = 7, .count = 1});
        errorCode([&]{ prepareInventoryWear(ports.access(), itemId, 7, 1); }, "busy");
    }
    SECTION("real canWear sex denial"){
        ports.male = false;
        CHECK_FALSE(MyHero::canWearForGender(itemId, WLG_DRESS, ports.male));
        errorCode([&]{ prepareInventoryWear(ports.access(), itemId, 7, 1); }, "invalid_argument");
    }
    CHECK(ports.sends == 0);
}

TEST_CASE("Equipment send facts require current guarded validation and a real send")
{
    GuiFixture fixture;
    EquipmentPorts ports;
    const auto item = ports.add(DBCOM_ITEMID(u8"木剑"));
    const auto selection = prepareInventoryWear(ports.access(), item.itemID, item.seqID, 1);
    CHECK_FALSE(ports.submit(selection, EquipmentOrigin::Ai, []{ return false; }));
    CHECK(ports.networkAttempts == 0); CHECK(ports.requests.sequence() == 0);
    ports.networkAccepts = false;
    CHECK_FALSE(ports.submit(selection));
    CHECK(ports.networkAttempts == 1); CHECK(ports.requests.sequence() == 0); CHECK_FALSE(ports.requests.pending());
    ports.networkAccepts = true;
    ports.inventory.remove(item.itemID, item.seqID, 1);
    errorCode([&]{ ports.submit(selection); }, "invalid_argument");
    CHECK(ports.networkAttempts == 1);
    ports.inventory.add(item, false);
    REQUIRE(ports.submit(selection));
    CHECK(ports.sends == 1); CHECK(ports.requests.submitted(1, selection)); CHECK(ports.requests.pending());
    CHECK(ports.inventory.getPackBinList().at(0).item.seqID == item.seqID);
    CHECK_FALSE(ports.wear.getWLItem(selection.slot));
    errorCode([&]{ ports.submit(selection); }, "busy");
    CHECK(ports.sends == 1);
}

TEST_CASE("Equipment observation rejects backpack removal wrong identity slot and stale generations")
{
    GuiFixture fixture;
    EquipmentPorts ports;
    const auto item = ports.add(DBCOM_ITEMID(u8"木剑"));
    const auto selection = prepareInventoryWear(ports.access(), item.itemID, item.seqID, 1);
    REQUIRE(ports.submit(selection));
    ports.inventory.remove(item.itemID, item.seqID, 1);
    CHECK_FALSE(ports.requests.observed(1, selection, ports.wear));
    auto wrong = item; ++wrong.seqID;
    ports.requests.observe(selection.slot, wrong);
    ports.requests.observe(WLG_DRESS, item);
    CHECK(ports.requests.pending());
    ports.wear.setWLItem(selection.slot, item);
    CHECK_FALSE(ports.requests.observed(1, selection, ports.wear));
    ports.requests.observe(selection.slot, item);
    REQUIRE(ports.requests.observed(1, selection, ports.wear));
    CHECK_FALSE(ports.requests.observed(2, selection, ports.wear));
    ports.wear.setWLItem(selection.slot, wrong);
    CHECK_FALSE(ports.requests.observed(1, selection, ports.wear));

    const auto nextItem = ports.add(item.itemID, 9);
    const auto next = prepareInventoryWear(ports.access(), nextItem.itemID, nextItem.seqID, 1);
    REQUIRE(ports.submit(next));
    ports.requests.observe(selection.slot, item);
    CHECK(ports.requests.pending()); CHECK_FALSE(ports.requests.observed(2, next, ports.wear));
    ports.wear.setWLItem(next.slot, nextItem);
    ports.requests.observe(next.slot, nextItem);
    CHECK(ports.requests.observed(2, next, ports.wear));
    CHECK_FALSE(ports.requests.observed(1, next, ports.wear));
}

TEST_CASE("Equipment cancellation preserves pending send uncertainty until its late real state")
{
    GuiFixture fixture;
    EquipmentPorts ports;
    const auto item = ports.add(DBCOM_ITEMID(u8"古铜戒指"));
    const auto selection = prepareInventoryWear(ports.access(), item.itemID, item.seqID, 1);
    REQUIRE(ports.submit(selection));
    ActionProgress progress(ActionKind::EquipWear, context(), 0, 1000, item.itemID, item.seqID, 1, selection.slot, 1);
    CHECK(progress.cancel(true).outcome == OperationOutcome::Unknown);
    errorCode([&]{ ports.submit(selection); }, "busy");
    ports.requests.reject(item.itemID, item.seqID + 1);
    CHECK(ports.requests.pending()); CHECK_FALSE(ports.requests.rejected(1));
    ports.wear.setWLItem(selection.slot, item);
    ports.requests.observe(selection.slot, item);
    REQUIRE(ports.requests.observed(1, selection, ports.wear));
    ActionObservation state; state.context = context(); state.submitted = true;
    state.equipment = EquipmentObservation{item.itemID, item.seqID, selection.slot, 1};
    CHECK(progress.observe(state, 10).outcome == OperationOutcome::Unknown);
    errorCode([&]{ prepareInventoryWear(ports.access(), item.itemID, item.seqID, 1); }, "invalid_argument");
    CHECK(ports.sends == 1);
}

TEST_CASE("Equipment explicit server error resolves only the matching pending request")
{
    GuiFixture fixture;
    EquipmentPorts ports;
    const auto item = ports.add(DBCOM_ITEMID(u8"木剑"));
    const auto selection = prepareInventoryWear(ports.access(), item.itemID, item.seqID, 1);
    REQUIRE(ports.submit(selection));
    ports.requests.reject(item.itemID + 1, item.seqID);
    CHECK_FALSE(ports.requests.rejected(1)); CHECK(ports.requests.pending());
    ports.requests.reject(item.itemID, item.seqID);
    CHECK(ports.requests.rejected(1)); CHECK_FALSE(ports.requests.pending());
    CHECK_FALSE(ports.requests.observed(1, selection, ports.wear));
}

TEST_CASE("Manual equipment stays available for the same or another slot after an unknown AI send")
{
    GuiFixture fixture;
    for(const bool sameSlot: {true, false}){
        CAPTURE(sameSlot);
        EquipmentPorts ports;
        const auto aiItem = ports.add(DBCOM_ITEMID(u8"木剑"));
        const auto aiSelection = prepareInventoryWear(ports.access(), aiItem.itemID, aiItem.seqID, 1, EquipmentOrigin::Ai);
        REQUIRE(ports.submit(aiSelection));
        ActionProgress progress(ActionKind::EquipWear, context(), 0, 1000, aiItem.itemID, aiItem.seqID, 1, aiSelection.slot, 1);
        CHECK(progress.cancel(true).outcome == OperationOutcome::Unknown);

        const auto manualItem = ports.add(sameSlot ? aiItem.itemID : DBCOM_ITEMID(u8"布衣（男）"), 8);
        const auto manualSelection = prepareInventoryWear(ports.access(), manualItem.itemID, manualItem.seqID, 1);
        CHECK((manualSelection.slot == aiSelection.slot) == sameSlot);
        REQUIRE(ports.submit(manualSelection, EquipmentOrigin::Manual));
        REQUIRE(ports.fakeNetworkSend(manualSelection, EquipmentOrigin::Manual));
        CHECK(ports.sends == 3);
        CHECK(ports.requests.submitted(1, aiSelection));
        errorCode([&]{ prepareInventoryWear(ports.access(), aiItem.itemID, aiItem.seqID, 1, EquipmentOrigin::Ai); }, "busy");

        ports.wear.setWLItem(aiSelection.slot, aiItem);
        ports.requests.observe(aiSelection.slot, aiItem);
        CHECK_FALSE(ports.requests.observed(1, aiSelection, ports.wear));
        ports.wear.setWLItem(manualSelection.slot, manualItem);
        ports.requests.observe(manualSelection.slot, manualItem);
        CHECK(ports.requests.pending());
        ports.requests.reject(manualItem.itemID, manualItem.seqID);
        CHECK_FALSE(ports.requests.pending()); CHECK_FALSE(ports.requests.rejected(1));
        CHECK(ports.requests.submitted(1, aiSelection));

        const auto nextItem = ports.add(aiItem.itemID, 9);
        const auto nextSelection = prepareInventoryWear(ports.access(), nextItem.itemID, nextItem.seqID, 1, EquipmentOrigin::Ai);
        REQUIRE(ports.submit(nextSelection));
        CHECK(ports.requests.submitted(4, nextSelection));
        ports.requests.observe(manualSelection.slot, manualItem);
        CHECK_FALSE(ports.requests.observed(4, nextSelection, ports.wear));
        ports.wear.setWLItem(nextSelection.slot, nextItem);
        ports.requests.observe(nextSelection.slot, nextItem);
        CHECK(ports.requests.observed(4, nextSelection, ports.wear));
    }
}

TEST_CASE("Manual identical equipment overlap cannot qualify the old AI receipt or error")
{
    GuiFixture fixture;
    for(const bool reject: {false, true}){
        CAPTURE(reject);
        EquipmentPorts ports;
        const auto item = ports.add(DBCOM_ITEMID(u8"木剑"));
        const auto selection = prepareInventoryWear(ports.access(), item.itemID, item.seqID, 1, EquipmentOrigin::Ai);
        REQUIRE(ports.submit(selection));
        REQUIRE(ports.submit(selection, EquipmentOrigin::Manual));
        CHECK(ports.sends == 2); CHECK(ports.requests.submitted(1, selection));
        if(reject){ ports.requests.reject(item.itemID, item.seqID); }
        else{
            ports.wear.setWLItem(selection.slot, item);
            ports.requests.observe(selection.slot, item);
        }
        CHECK_FALSE(ports.requests.observed(1, selection, ports.wear));
        CHECK_FALSE(ports.requests.rejected(1)); CHECK(ports.requests.pending());
        if(reject){ ports.requests.reject(item.itemID, item.seqID); }
        else{ ports.requests.observe(selection.slot, item); }
        CHECK_FALSE(ports.requests.pending());
        CHECK_FALSE(ports.requests.observed(1, selection, ports.wear));
        CHECK_FALSE(ports.requests.rejected(1));

        ActionProgress progress(ActionKind::EquipWear, context(), 0, 1000, item.itemID, item.seqID, 1, selection.slot, 1);
        ActionObservation state; state.context = context(); state.submitted = ports.requests.submitted(1, selection);
        CHECK(progress.observe(state, 10).outcome == OperationOutcome::Running);
        CHECK(progress.observe(state, 1000).outcome == OperationOutcome::Unknown);
    }
}

TEST_CASE("Only an actual manual equipment send revokes unpolled AI state evidence")
{
    GuiFixture fixture;
    for(const bool manualSends: {false, true}){
        CAPTURE(manualSends);
        EquipmentPorts ports;
        const auto item = ports.add(DBCOM_ITEMID(u8"木剑"));
        const auto selection = prepareInventoryWear(ports.access(), item.itemID, item.seqID, 1, EquipmentOrigin::Ai);
        REQUIRE(ports.submit(selection));
        ports.wear.setWLItem(selection.slot, item);
        ports.requests.observe(selection.slot, item);
        REQUIRE(ports.requests.observed(1, selection, ports.wear));
        const auto manualItem = ports.add(DBCOM_ITEMID(u8"布衣（男）"), 8);
        const auto manualSelection = prepareInventoryWear(ports.access(), manualItem.itemID, manualItem.seqID, 1);
        ports.networkAccepts = manualSends;
        CHECK(ports.submit(manualSelection, EquipmentOrigin::Manual) == manualSends);
        CHECK(ports.requests.observed(1, selection, ports.wear) == !manualSends);
        CHECK(ports.requests.submitted(1, selection));
    }
}

TEST_CASE("A full equipment receipt ledger continues accepting ordinary manual equipment")
{
    EquipmentRequests requests;
    unsigned sends = 0;
    const EquipmentSelection selection{DBCOM_ITEMID(u8"木剑"), 7, WLG_WEAPON};
    for(unsigned index = 0; index < 130; ++index){
        REQUIRE(requests.submit(selection, EquipmentOrigin::Manual, [&]{ ++sends; return true; }));
    }
    CHECK(sends == 130); CHECK(requests.pending());
    CHECK_FALSE(requests.submit(selection, EquipmentOrigin::Ai, [&]{ ++sends; return true; }));
    CHECK(sends == 130);
    REQUIRE(requests.submit(selection, EquipmentOrigin::Manual, [&]{ ++sends; return true; }));
    CHECK(sends == 131);
}
