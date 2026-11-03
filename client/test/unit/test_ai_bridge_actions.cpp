#include "aibridge/snapshot.hpp"
#include "aibridge/movement.hpp"
#include "aibridge/actions.hpp"
#include "aibridge/controller.hpp"
#include "pathf.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <iostream>
#include <limits>
#include <set>

using namespace aibridge;
namespace
{
    Json context()
    {
        return {{"instance_id", std::string(32, 'a')}, {"session_id", std::string(32, 'b')},
            {"world_epoch", "1"}, {"control_epoch", "1"}};
    }
    template<typename F> void errorCode(F &&function, std::string_view code)
    {
        try{ function(); FAIL("Expected a controlled protocol error"); }
        catch(const ProtocolError &error){ CHECK(error.code == code); }
    }
    std::vector<Json> inventory(size_t count)
    {
        std::vector<Json> result;
        for(size_t i = 0; i < count; ++i){ result.push_back(inventoryEntry({1, uint32_t(i + 1), 2})); }
        return result;
    }
    MoveObservation observation(GridPoint position = {1, 1}, bool stopped = false)
    {
        return {{1, 100000}, context(), position, position, stopped, true, std::nullopt};
    }
}

TEST_CASE("state preserves unknown values and full width identities")
{
    CharacterSnapshot state;
    state.stamp = {std::numeric_limits<uint64_t>::max(), 1000};
    state.characterUid = state.mapUid = std::numeric_limits<uint64_t>::max();
    state.position = {1, 2}; state.actionEndpoint = {2, 3};
    auto value = stateSnapshot(state);
    CHECK(value.at("character_uid") == "18446744073709551615");
    CHECK(value.at("snapshot_seq") == "18446744073709551615");
    CHECK(value.at("name").is_null()); CHECK(value.at("hp").is_null());
    CHECK(value.at("gold").is_null()); CHECK(value.at("level").is_null());
    CHECK(value.at("unavailable_fields").size() == 10);
    state.hp = 0; state.level = 0; state.gold = std::numeric_limits<uint64_t>::max();
    state.direction = 8; state.name = "known";
    value = stateSnapshot(state);
    CHECK(value.at("hp") == 0); CHECK(value.at("level") == 0);
    CHECK(value.at("gold") == "18446744073709551615");
    CHECK(value.at("direction") == 8);
    state.direction = 0; CHECK_THROWS_AS(stateSnapshot(state), ProtocolError);
    state.direction = 1; state.hp = -1; CHECK_THROWS_AS(stateSnapshot(state), ProtocolError);
}

TEST_CASE("handles expire on visibility and real lifetime changes without resurrection")
{
    ObservationHandles handles("test");
    const TargetIdentity target{100, 1}; const GroundIdentity ground{{2, 3}, 1};
    handles.sync(context(), {target}, {ground});
    const auto oldTarget = handles.targetHandle(target), oldGround = handles.groundHandle(ground);
    auto changedControl = context(); changedControl["control_epoch"] = "2";
    handles.sync(changedControl, {target}, {ground});
    CHECK(handles.resolveTarget(oldTarget, changedControl) == target);
    CHECK(handles.resolveGround(oldGround, changedControl) == ground);
    handles.sync(context(), {}, {});
    errorCode([&]{ handles.resolveTarget(oldTarget, context()); }, "stale_target");
    handles.sync(context(), {target}, {ground});
    CHECK(handles.targetHandle(target) != oldTarget);
    CHECK(handles.groundHandle(ground) != oldGround);
    const auto reappeared = handles.targetHandle(target);
    handles.sync(context(), {{100, 2}}, {{{2, 3}, 2}});
    errorCode([&]{ handles.resolveTarget(reappeared, context()); }, "stale_target");
    errorCode([&]{ handles.resolveGround(oldGround, context()); }, "stale_target");
    for(const auto *field: {"instance_id", "session_id", "world_epoch"}){
        auto next = context(); next[field] = field == std::string("world_epoch") ? "2" : std::string(32, 'c');
        const auto before = handles.targetHandle({100, 2});
        handles.sync(next, {{100, 2}}, {});
        errorCode([&]{ handles.resolveTarget(before, next); }, "stale_target");
        handles.sync(context(), {{100, 2}}, {});
    }
    handles.clear();
    errorCode([&]{ handles.resolveTarget(oldTarget, context()); }, "stale_target");
}

TEST_CASE("maximum identity observation is bounded and stable frames preserve handles")
{
    std::vector<TargetIdentity> targets;
    for(size_t i = 0; i < FrozenPages::maxRows; ++i){ targets.push_back({i + 1, i + 1}); }
    ObservationHandles handles("test");
    handles.sync(context(), targets, {});
    const auto first = handles.targetHandle(targets.front()), last = handles.targetHandle(targets.back());
    const auto start = std::chrono::steady_clock::now();
    for(int i = 0; i < 100; ++i){ handles.sync(context(), targets, {}); }
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    std::cout << "Stable identity sync: rows=" << targets.size() << ", frames=100, total_us=" << micros << '\n';
    CHECK(handles.targetHandle(targets.front()) == first); CHECK(handles.targetHandle(targets.back()) == last);
    targets.push_back({99999, 99999});
    errorCode([&]{ handles.sync(context(), targets, {}); }, "unavailable");
    targets.pop_back(); targets.back() = targets.front();
    errorCode([&]{ handles.sync(context(), targets, {}); }, "unavailable");
    CHECK(handles.resolveTarget(last, context()).uid == FrozenPages::maxRows);
}

TEST_CASE("expiring one observation preserves unchanged target and ground handles")
{
    ObservationHandles handles("test");
    const TargetIdentity staying{100, 1}, leaving{200, 2};
    const GroundIdentity stableGround{{1, 1}, 1}, changedGround{{2, 2}, 2};
    handles.sync(context(), {staying, leaving}, {stableGround, changedGround});
    const auto target = handles.targetHandle(staying), expired = handles.targetHandle(leaving);
    const auto ground = handles.groundHandle(stableGround), replaced = handles.groundHandle(changedGround);
    handles.sync(context(), {staying}, {stableGround});
    CHECK(handles.resolveTarget(target, context()) == staying);
    CHECK(handles.resolveGround(ground, context()) == stableGround);
    errorCode([&]{ handles.resolveTarget(expired, context()); }, "stale_target");
    errorCode([&]{ handles.resolveGround(replaced, context()); }, "stale_target");
    handles.sync(context(), {staying, leaving}, {stableGround, {{2, 2}, 3}});
    CHECK(handles.targetHandle(staying) == target); CHECK(handles.groundHandle(stableGround) == ground);
    CHECK(handles.targetHandle(leaving) != expired);
    CHECK(handles.groundHandle({{2, 2}, 3}) != replaced);
}

TEST_CASE("nearby filtering uses visible inputs and ground unknowns stay explicit")
{
    ObservationHandles handles("test");
    handles.sync(context(), {{100, 1}}, {{{3, 3}, 1}});
    NearbyEntry monster; monster.kind = "monster"; monster.position = {2, 2}; monster.target = TargetIdentity{100, 1};
    NearbyEntry ground; ground.kind = "ground"; ground.position = {3, 3}; ground.ground = GroundIdentity{{3, 3}, 1};
    ground.groundItemIds = {1, 2};
    CHECK(selectNearby({monster, ground}, {0, 0}, "all", 2).size() == 1);
    CHECK(selectNearby({monster, ground}, {0, 0}, "ground", 3).size() == 1);
    CHECK(selectNearby({monster, ground}, {0, 0}, "npc", 40).empty());
    auto value = nearbyEntry(monster, handles);
    CHECK(value.at("name").is_null()); CHECK(value.at("hp").is_null()); CHECK(value.at("alive").is_null());
    value = nearbyEntry(ground, handles);
    CHECK(value.at("uid").is_null()); CHECK(value.at("target_handle").is_null());
    CHECK(value.at("items").at(0).at("seq_id").is_null());
    CHECK(value.at("items").at(0).at("count").is_null());
    ground.groundItemIds.resize(100, 1); CHECK(nearbyEntry(ground, handles).at("items").size() == 100);
    ground.groundItemIds.push_back(1);
    errorCode([&]{ nearbyEntry(ground, handles); }, "unavailable");
}

TEST_CASE("frozen pages keep two independent immutable snapshots per list")
{
    FrozenPages pages("test");
    auto items = inventory(201);
    auto first = pages.first(SnapshotList::Inventory, context(), Json::object(), {1, 1000}, items);
    const auto cursor = first.at("next_cursor").get<std::string>();
    CHECK(first.at("items").size() == 100);
    items[100]["count"] = 999;
    pages.first(SnapshotList::Inventory, context(), Json::object(), {2, 2000}, items);
    auto second = pages.next(SnapshotList::Inventory, context(), Json::object(), cursor);
    CHECK(second.at("snapshot_seq") == "1"); CHECK(second.at("observed_at") == "1000");
    CHECK(second.at("items").at(0).at("count") == 2);
    const auto last = pages.next(SnapshotList::Inventory, context(), Json::object(), second.at("next_cursor").get<std::string>());
    CHECK(last.at("items").size() == 1); CHECK(last.at("next_cursor").is_null());
    for(int i = 0; i < 3; ++i){ pages.first(SnapshotList::Nearby, context(), Json::object(), {3, 3000}, {}); }
    CHECK_NOTHROW(pages.next(SnapshotList::Inventory, context(), Json::object(), cursor));
    pages.first(SnapshotList::Inventory, context(), Json::object(), {4, 4000}, items);
    errorCode([&]{ pages.next(SnapshotList::Inventory, context(), Json::object(), cursor); }, "stale_cursor");
}

TEST_CASE("old nearby pages never revive invalidated target handles")
{
    ObservationHandles handles("test"); FrozenPages pages("test");
    handles.sync(context(), {{100, 1}}, {});
    NearbyEntry entry; entry.kind = "monster"; entry.target = TargetIdentity{100, 1};
    const auto row = nearbyEntry(entry, handles);
    auto first = pages.first(SnapshotList::Nearby, context(), Json::object(), {1, 1000}, std::vector<Json>(101, row));
    handles.sync(context(), {}, {});
    const auto second = pages.next(SnapshotList::Nearby, context(), {{"kind", "all"}, {"radius", 15}}, first.at("next_cursor").get<std::string>());
    const auto old = second.at("items").at(0).at("target_handle").get<std::string>();
    errorCode([&]{ handles.resolveTarget(old, context()); }, "stale_target");
    errorCode([&]{ pages.next(SnapshotList::Nearby, context(), {{"radius", 14}}, first.at("next_cursor").get<std::string>()); }, "stale_cursor");
}

TEST_CASE("cursor rejects changed scope forged offsets wrong list and cleared cache")
{
    FrozenPages pages("test");
    const auto first = pages.first(SnapshotList::Inventory, context(), Json::object(), {1, 1000}, inventory(101));
    const auto cursor = first.at("next_cursor").get<std::string>();
    auto changed = context(); changed["world_epoch"] = "2";
    errorCode([&]{ pages.next(SnapshotList::Inventory, changed, Json::object(), cursor); }, "stale_cursor");
    changed = context(); changed["control_epoch"] = "2";
    CHECK_NOTHROW(pages.next(SnapshotList::Inventory, changed, Json::object(), cursor));
    errorCode([&]{ pages.next(SnapshotList::Nearby, context(), Json::object(), cursor); }, "stale_cursor");
    for(const auto suffix: {":0", ":0100", ":101", ":200", ":100x", ":18446744073709551616"}){
        const auto forged = cursor.substr(0, cursor.find(':')) + suffix;
        errorCode([&]{ pages.next(SnapshotList::Inventory, context(), Json::object(), forged); }, "stale_cursor");
    }
    pages.clear();
    auto fresh = pages.first(SnapshotList::Inventory, context(), Json::object(), {1, 1000}, inventory(101));
    CHECK(fresh.at("next_cursor") != cursor);
    errorCode([&]{ pages.next(SnapshotList::Inventory, context(), Json::object(), cursor); }, "stale_cursor");
}

TEST_CASE("inventory distinguishes unknown initial data from known empty and validates reservations")
{
    FrozenPages pages("test");
    const auto unknown = pages.first(SnapshotList::Inventory, context(), Json::object(), {1, 1000}, {}, {"inventory", "belt", "equipment"});
    const auto empty = pages.first(SnapshotList::Inventory, context(), Json::object(), {2, 2000}, {});
    CHECK(unknown.at("items").empty()); CHECK(unknown.at("unavailable_fields").size() == 3);
    CHECK(empty.at("items").empty()); CHECK(empty.at("unavailable_fields").empty());
    InventoryEntry item{1, 2, 3, 4};
    errorCode([&]{ inventoryEntry(item); }, "unavailable");
    item.reservedCount = 3; CHECK(inventoryEntry(item).at("reserved_count") == 3);
}

TEST_CASE("frozen missing-field metadata separates unknown values from inapplicable nulls")
{
    ObservationHandles handles("test"); FrozenPages pages("test");
    handles.sync(context(), {{100, 1}}, {{{1, 1}, 1}});
    NearbyEntry ground; ground.kind = "ground"; ground.position = {1, 1};
    ground.ground = GroundIdentity{{1, 1}, 1}; ground.groundItemIds = {1};
    NearbyEntry npc; npc.kind = "npc"; npc.target = TargetIdentity{100, 1};
    auto rows = std::vector<Json>(100, nearbyEntry(ground, handles));
    rows.push_back(nearbyEntry(npc, handles));
    const auto first = pages.first(SnapshotList::Nearby, context(), Json::object(), {1, 1000}, rows);
    const auto missing = first.at("unavailable_fields");
    const auto has = [&](std::string field){ return std::find(missing.begin(), missing.end(), Json(field)) != missing.end(); };
    CHECK(has("items[].items[].seq_id")); CHECK(has("items[].items[].count")); CHECK(has("items[].items[].name"));
    CHECK(has("items[].name")); CHECK(has("items[].hp")); CHECK(has("items[].alive"));
    CHECK_FALSE(has("items[].target_handle")); CHECK_FALSE(has("items[].ground_handle")); CHECK_FALSE(has("items[].uid"));
    const auto second = pages.next(SnapshotList::Nearby, context(), Json::object(), first.at("next_cursor").get<std::string>());
    CHECK(second.at("unavailable_fields") == missing);
    const auto bag = pages.first(SnapshotList::Inventory, context(), Json::object(), {2, 2000}, inventory(1));
    CHECK(bag.at("unavailable_fields") == Json::array({"items[].name"}));
}

TEST_CASE("region rejects overflow and edges before reading and preserves dynamic uncertainty")
{
    size_t reads = 0;
    const auto read = [&](int x, int){ ++reads; return TerrainCell{x == 9, x == 9 ? std::optional<bool>(true) : std::nullopt}; };
    errorCode([&]{ mapRegion({1, 1000}, 1, 10, 10, std::numeric_limits<int>::max(), 0, 33, 1, read); }, "out_of_bounds");
    errorCode([&]{ mapRegion({1, 1000}, 1, 10, 10, 9, 9, 2, 1, read); }, "out_of_bounds");
    errorCode([&]{ mapRegion({1, 1000}, 1, 10, 10, -1, 0, 1, 1, read); }, "out_of_bounds");
    CHECK(reads == 0);
    const auto region = mapRegion({1, 1000}, std::numeric_limits<uint64_t>::max(), 10, 10, 8, 9, 2, 1, read);
    CHECK(reads == 2); CHECK(region.at("map_uid") == "18446744073709551615");
    CHECK(region.at("cells").at(0).at("dynamic_blocked").is_null());
    CHECK(region.at("cells").at(1).at("walkable") == true);
    CHECK(region.at("cells").at(1).at("dynamic_blocked") == true);
    CHECK(region.at("unavailable_fields") == Json::array({"dynamic_blocked"}));
}

TEST_CASE("movement requires stopped observed position not queued endpoint")
{
    MoveProgress progress({2, 2}, 0, context(), {1, 1}, 0, 30000);
    auto state = observation(); state.actionEndpoint = {2, 2};
    CHECK(progress.observe(state, 10, true).outcome == OperationOutcome::Running);
    state.position = {2, 2};
    CHECK(progress.observe(state, 20, true).outcome == OperationOutcome::Running);
    state.stopped = true;
    const auto done = progress.observe(state, 30, true);
    CHECK(done.outcome == OperationOutcome::Succeeded);
    CHECK(done.completionScope == "observed_postcondition");
    CHECK(done.result.at("position") == gridPoint({2, 2}));
    CHECK(done.evidence.at(0).at("server_code").is_null());
    CHECK(done.evidence.at(0).at("source") == "client");
    state.position = {10, 10}; CHECK(progress.observe(state, 40, true).outcome == OperationOutcome::Succeeded);
    MoveProgress current({1, 1}, 0, context(), {1, 1}, 0, 30000);
    CHECK(current.observe(observation({1, 1}, true), 0, false).outcome == OperationOutcome::Succeeded);
    MoveProgress tolerance({3, 3}, 2, context(), {1, 1}, 0, 30000);
    CHECK(tolerance.observe(observation({1, 1}, true), 1, false).outcome == OperationOutcome::Succeeded);
}

TEST_CASE("shared client path solver handles fake terrain edges and dynamic obstruction")
{
    // ClientPathFinder uses this same solver with ProcessRun::oneStepCost.
    std::set<GridPoint> blocked;
    const auto traversable = [&](int x, int y){ return x >= 0 && y >= 0 && x < 5 && y < 5 && !blocked.contains({x, y}); };
    const auto stepCost = [&](int x0, int y0, int, int x1, int y1)->std::optional<double>{
        if(!traversable(x0, y0) || !traversable(x1, y1)){ return std::nullopt; }
        return x0 != x1 && y0 != y1 ? 1.5 : 1.0;
    };
    pathf::AStarPathFinder finder(true, 1, stepCost);
    SECTION("reachable map edge and shared movement postcondition"){
        finder.search(0, 0, 1, 4, 4);
        REQUIRE(finder.hasPath());
        const auto path = finder.getPathNode(); REQUIRE_FALSE(path.empty());
        CHECK(path.front().X == 0); CHECK(path.back().X == 4); CHECK(path.back().Y == 4);
        MoveProgress progress({4, 4}, 0, context(), {0, 0}, 0, 30000);
        for(size_t i = 0; i < path.size(); ++i){
            const auto update = progress.observe(observation({path[i].X, path[i].Y}, i + 1 == path.size()), i * 100, i != 0);
            CHECK(update.outcome == (i + 1 == path.size() ? OperationOutcome::Succeeded : OperationOutcome::Running));
        }
    }
    SECTION("static wall blocks target"){
        for(int y = 0; y < 5; ++y){ blocked.insert({2, y}); }
        finder.search(0, 0, 1, 4, 4); CHECK_FALSE(finder.hasPath());
    }
    SECTION("new dynamic blocker removes the only open crossing"){
        for(int y = 0; y < 4; ++y){ blocked.insert({2, y}); }
        finder.search(0, 0, 1, 4, 4); REQUIRE(finder.hasPath());
        blocked.insert({2, 4});
        // CreatureMovable constructs a fresh ClientPathFinder for each changed scene.
        pathf::AStarPathFinder changedFinder(true, 1, stepCost);
        changedFinder.search(0, 0, 1, 4, 4); CHECK_FALSE(changedFinder.hasPath());
    }
}

TEST_CASE("bounded shared path search distinguishes exhausted work from proven no route")
{
    constexpr size_t searchLimit = maxMoveSearchRounds;
    size_t reads = 0;
    const auto traversable = [](int x, int y){ return x >= 0 && y >= 0 && x < 8192 && y < 8192 && x != 4096; };
    pathf::AStarPathFinder large(true, 1, [&](int x0, int y0, int, int x1, int y1)->std::optional<double>{
        ++reads;
        if(!traversable(x0, y0) || !traversable(x1, y1)){ return std::nullopt; }
        return x0 != x1 && y0 != y1 ? 1.5 : 1.0;
    });
    const auto started = std::chrono::steady_clock::now();
    const auto exhausted = large.search(10, 10, 1, 8000, 8000, searchLimit);
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
    CHECK_FALSE(exhausted.hasPath()); CHECK_FALSE(exhausted.doneSearch());
    CHECK(reads > 0); CHECK(reads <= searchLimit * 32);
    std::cout << "Bounded path probe: iterations=" << searchLimit << ", reads=" << reads << ", elapsed_us=" << micros << '\n';
    pathf::AStarPathFinder sealed(true, 1, [](int, int, int, int, int)->std::optional<double>{ return std::nullopt; });
    const auto impossible = sealed.search(1, 1, 1, 3, 3, searchLimit);
    CHECK_FALSE(impossible.hasPath()); CHECK(impossible.doneSearch());
}

TEST_CASE("movement stall uses real grid progress and handles server correction")
{
    MoveProgress progress({10, 10}, 0, context(), {1, 1}, 0, 30000);
    auto state = observation();
    state.actionEndpoint = {10, 10};
    CHECK(progress.observe(state, 2999, true).outcome == OperationOutcome::Running);
    auto stalled = progress.observe(state, 3000, true);
    REQUIRE(stalled.error); CHECK(stalled.error->code == "stalled");
    MoveProgress advancing({10, 10}, 0, context(), {1, 1}, 0, 30000);
    state.position = {2, 2}; CHECK(advancing.observe(state, 2500, true).outcome == OperationOutcome::Running);
    state.position = {1, 1}; state.lastServerPosition = GridPoint{1, 1};
    auto corrected = advancing.observe(state, 5000, true);
    CHECK(corrected.outcome == OperationOutcome::Running);
    CHECK(corrected.evidence.at(0).at("description").get<std::string>().find("非本操作回执") != std::string::npos);
    CHECK(advancing.observe(state, 7999, true).outcome == OperationOutcome::Running);
    CHECK(advancing.observe(state, 8000, true).error->code == "stalled");
}

TEST_CASE("movement timeout caps continuous progress and invalid context never succeeds")
{
    for(const bool submitted: {false, true}){
        MoveProgress progress({10, 10}, 0, context(), {1, 1}, 0, 30000);
        for(uint64_t t = 2000; t < 30000; t += 2000){
            CHECK(progress.observe(observation({int(t / 2000), 1}), t, submitted).outcome == OperationOutcome::Running);
        }
        auto timeout = progress.observe(observation({10, 10}, true), 30000, submitted);
        CHECK(timeout.outcome == (submitted ? OperationOutcome::Unknown : OperationOutcome::Failed));
        CHECK(timeout.error->code == "timeout");
        for(const bool death: {false, true}){
            MoveProgress cancelled({1, 1}, 0, context(), {1, 1}, 0, 30000);
            auto state = observation({1, 1}, true);
            if(death){ state.alive = false; } else{ state.context["world_epoch"] = "2"; }
            CHECK(cancelled.observe(state, 1, submitted).outcome == (submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled));
        }
    }
    errorCode([&]{ MoveProgress invalid({1, 1}, 0, context(), {1, 1}, 0, 30001); }, "invalid_argument");
    CHECK_FALSE(withinTolerance({0, 0}, {std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}, 2));
}

TEST_CASE("movement never forgets actual submission when later samples omit it")
{
    MoveProgress progress({2, 2}, 0, context(), {1, 1}, 0, 30000);
    CHECK(progress.observe(observation(), 1, true).submitted);
    auto changed = observation(); changed.context["control_epoch"] = "2";
    const auto terminal = progress.observe(changed, 2, false);
    CHECK(terminal.submitted); CHECK(terminal.outcome == OperationOutcome::Unknown);
}

TEST_CASE("oversized nearby observation stays a controlled query failure")
{
    ControllerOptions options; options.instanceId = std::string(32, 'a'); options.build = "actions-test";
    options.autoEnable = false;
    options.monotonicMs = []{ return 1; }; options.unixMs = []{ return 1000; };
    options.newSessionId = []{ return std::string(32, 'b'); };
    AiBridgeController controller(options); controller.onConnected(1); controller.observe({"run", 100, 200, true});
    ObservationHandles handles("test"); handles.sync(controller.context(), {}, {{{1, 1}, 1}});
    NearbyEntry entry; entry.kind = "ground"; entry.position = {1, 1}; entry.ground = GroundIdentity{{1, 1}, 1};
    entry.groundItemIds.resize(101, 1);
    controller.registerQuery("game_list_nearby", [&](const Json &, const Json &){ return nearbyEntry(entry, handles); });
    const auto response = controller.handleRequest({1, {{"v", 1}, {"id", "nearby"}, {"method", "game_list_nearby"}, {"params", Json::object()}}});
    REQUIRE(response.at("ok") == false); CHECK(response.at("error").at("code") == "unavailable");
    CHECK(controller.pauseReason() != "controller_error");
    CHECK(controller.controlState() == ControlState::ReadOnly);
}

TEST_CASE("controller drives shared movement helper and cancels before unbudgeted deadlines")
{
    uint64_t now = 0; int begins = 0, ticks = 0, cancels = 0;
    ControllerOptions options; options.instanceId = std::string(32, 'a'); options.build = "actions-test";
    options.policy = Policy::parse(R"({"allowed_actions":["game_move_to"]})");
    options.monotonicMs = [&]{ return now; }; options.unixMs = [&]{ return 100000 + now; };
    options.newSessionId = []{ return std::string(32, 'b'); };
    AiBridgeController controller(options); controller.onConnected(1); controller.observe({"run", 100, 200, true});
    REQUIRE(controller.enableByUser());
    std::optional<MoveProgress> progress;
    auto state = observation(); state.context = controller.context();
    bool sent = false;
    ActionAdapter adapter;
    adapter.validate = [](const Json &, const Json &){ return ValidationResult{}; };
    adapter.begin = [&](const OperationView &view){
        ++begins; progress.emplace(GridPoint{2, 2}, 0, view.context, state.position, now, view.deadlineMs - now);
        return progress->observe(state, now, sent);
    };
    adapter.tick = [&](const OperationView &)->std::optional<OperationUpdate>{ ++ticks; return progress->observe(state, now, sent); };
    adapter.cancel = [&](const OperationView &){ ++cancels; OperationUpdate update; update.outcome = OperationOutcome::Cancelled; update.submitted = sent; return update; };
    controller.registerAction("game_move_to", adapter);
    Json request = {{"v", 1}, {"id", "move"}, {"method", "game_move_to"}, {"params", {{"x", 2}, {"y", 2}}},
        {"request_id", "00000000-0000-4000-8000-000000000001"}, {"context", controller.context()}, {"timeout_ms", 30000}};
    const auto accepted = controller.handleRequest({1, request}); REQUIRE(accepted.at("ok") == true);
    const auto operationId = accepted.at("data").at("operation_id");
    const auto status = [&]{
        return controller.handleRequest({1, {{"v", 1}, {"id", "status"}, {"method", "game_get_operation"}, {"params", {{"operation_id", operationId}}}}}).at("data");
    };
    SECTION("manual takeover while accepted never begins"){
        controller.manualInput(); controller.advance(); CHECK(begins == 0); CHECK(status().at("state") == "cancelled");
    }
    SECTION("arrival records real helper evidence"){
        controller.advance(); CHECK(begins == 1);
        sent = true; state.position = {2, 2}; state.actionEndpoint = {2, 2}; state.stopped = true;
        controller.advance(); const auto result = status(); CHECK(result.at("state") == "succeeded");
        CHECK(result.at("completion_scope") == "observed_postcondition");
        CHECK(result.at("result").at("position") == gridPoint({2, 2}));
    }
    SECTION("busy cannot replace active movement and exhausted work budget still expires"){
        controller.advance(); sent = true; controller.advance();
        request["id"] = "other"; request["request_id"] = "00000000-0000-4000-8000-000000000002";
        CHECK(controller.handleRequest({1, request}).at("error").at("code") == "busy");
        const auto before = ticks; now = 30000; controller.checkDeadlines();
        CHECK(ticks == before); CHECK(cancels == 1); CHECK(status().at("state") == "unknown");
    }
}

namespace
{
    ActionObservation actionObservation()
    {
        ActionObservation result; result.stamp = {1, 100000}; result.context = context(); return result;
    }
    const std::vector<ActionKind> actionKinds = {ActionKind::PickUp, ActionKind::UseItem, ActionKind::AttackOnce,
        ActionKind::AttackUntilDead, ActionKind::Cast, ActionKind::Toggle};
    void completeObservation(ActionKind kind, ActionObservation &state)
    {
        state.submitted = true;
        switch(kind){
            case ActionKind::PickUp: state.correlatedReceipt = true; break;
            case ActionKind::UseItem: state.itemChanges = {{10, 11, 5, 3}}; break;
            case ActionKind::AttackOnce:
            case ActionKind::Cast: state.actionStarted = state.actionFinished = true; break;
            case ActionKind::AttackUntilDead: state.targetDead = true; break;
            case ActionKind::Toggle: state.toggleEnabled = false; break;
            case ActionKind::EquipWear: state.equipment = EquipmentObservation{10,11,2,1}; break;
        }
    }
}

TEST_CASE("typed action evidence distinguishes submission from actual completion")
{
    for(const auto kind: actionKinds){
        CAPTURE(int(kind));
        ActionProgress progress(kind, context(), 100, 1000, 10, 11, 2);
        auto state = actionObservation(); state.submitted = true;
        CHECK(progress.observe(state, 101).outcome == OperationOutcome::Running);
        completeObservation(kind, state);
        auto result = progress.observe(state, 102);
        REQUIRE(result.outcome == OperationOutcome::Succeeded);
        REQUIRE(result.completionScope);
        CHECK(*result.completionScope == (kind == ActionKind::PickUp ? "server_ack" :
            (kind == ActionKind::UseItem || kind == ActionKind::AttackUntilDead) ? "observed_postcondition" : "client_state"));
        REQUIRE_FALSE(result.evidence.empty());
        validateDefinition(result.result, "actionResult");
        for(const auto &entry: result.evidence){ validateDefinition(entry, "evidence"); }
        state.context["world_epoch"] = "2";
        CHECK(progress.observe(state, 2000).outcome == OperationOutcome::Succeeded);
    }
}

TEST_CASE("pickup changes are observations not correlated pickup receipts")
{
    ActionProgress progress(ActionKind::PickUp, context(), 0, 1000);
    auto state = actionObservation(); state.submitted = true;
    state.itemChanges = {{10, 11, 0, 5}}; state.goldBefore = 100; state.goldAfter = 200;
    auto result = progress.observe(state, 500);
    CHECK(result.outcome == OperationOutcome::Running);
    CHECK(result.result.at("item_changes").at(0).at("after") == 5);
    CHECK(result.result.at("gold_before") == "100"); CHECK(result.result.at("gold_after") == "200");
    CHECK(result.evidence.size() == 2);
    result = progress.observe(state, 1000);
    CHECK(result.outcome == OperationOutcome::Unknown);
    CHECK_FALSE(result.completionScope);
    state.correlatedReceipt = true;
    CHECK(progress.observe(state, 1001).outcome == OperationOutcome::Unknown);
}

TEST_CASE("use item requires exact instance decrease and never treats rewards as consumption")
{
    auto state = actionObservation(); state.submitted = true;
    for(const auto delta: std::vector<ItemChange>{{10,12,5,3},{12,11,5,3},{10,11,3,5},{10,11,5,4},{10,11,5,0}}){
        ActionProgress progress(ActionKind::UseItem, context(), 0, 1000, 10, 11, 2);
        state.itemChanges = {delta}; state.correlatedReceipt = true;
        CHECK(progress.observe(state, 1).outcome == OperationOutcome::Running);
        CHECK(progress.observe(state, 1000).outcome == OperationOutcome::Unknown);
    }
    ActionProgress exact(ActionKind::UseItem, context(), 0, 1000, 10, 11, 2);
    state.itemChanges = {{10,11,5,3}}; state.submitted = false;
    CHECK(exact.observe(state, 1).outcome == OperationOutcome::Running);
    state.submitted = true;
    CHECK(exact.observe(state, 2).outcome == OperationOutcome::Succeeded);
}

TEST_CASE("equipment action needs an exact slot identity and its own send generation")
{
    ActionProgress progress(ActionKind::EquipWear, context(), 0, 1000, 10, 11, 1, 2, 5);
    auto state = actionObservation(); state.submitted = true;
    state.itemChanges = {{10,11,1,0}}; state.correlatedReceipt = true;
    CHECK(progress.observe(state, 1).outcome == OperationOutcome::Running);
    state.itemChanges.clear();
    for(const auto equipment: {EquipmentObservation{10,12,2,5}, EquipmentObservation{12,11,2,5},
        EquipmentObservation{10,11,3,5}, EquipmentObservation{10,11,2,4}, EquipmentObservation{10,11,2,0}}){
        state.equipment = equipment;
        CHECK(progress.observe(state, 2).outcome == OperationOutcome::Running);
    }
    state.equipment = EquipmentObservation{10,11,2,5};
    ActionProgress unsent(ActionKind::EquipWear, context(), 0, 1000, 10, 11, 1, 2, 5);
    state.submitted = false;
    CHECK(unsent.observe(state, 3).outcome == OperationOutcome::Running);
    state.submitted = true;
    const auto result = progress.observe(state, 4);
    REQUIRE(result.outcome == OperationOutcome::Succeeded);
    CHECK(result.completionScope == "observed_postcondition");
    REQUIRE(result.evidence.size() == 1);
    CHECK(result.evidence.at(0).at("kind") == "action");
    CHECK(result.evidence.at(0).at("source") == "client");
    CHECK_FALSE(result.result.contains("item_changes"));
    validateDefinition(result.result, "actionResult");
    validateDefinition(result.evidence.at(0), "evidence");
}

TEST_CASE("equipment action invalidation keeps submitted uncertainty and terminal late state")
{
    for(const bool submitted: {false,true}){
        for(const auto invalidation: {"session_id","world_epoch","control_epoch","death","timeout","cancel"}){
            CAPTURE(submitted, invalidation);
            ActionProgress progress(ActionKind::EquipWear, context(), 0, 1000, 10, 11, 1, 2, 5);
            auto state = actionObservation(); state.submitted = submitted;
            state.equipment = EquipmentObservation{10,11,2,5};
            OperationUpdate result;
            if(invalidation == std::string("cancel")){ result = progress.cancel(submitted); }
            else{
                if(invalidation == std::string("death")){ state.alive = false; }
                else if(invalidation != std::string("timeout")){ state.context[invalidation] = "changed"; }
                result = progress.observe(state, invalidation == std::string("timeout") ? 1000 : 1);
            }
            CHECK(result.submitted == submitted);
            CHECK(result.outcome == (submitted ? OperationOutcome::Unknown :
                invalidation == std::string("timeout") ? OperationOutcome::Failed : OperationOutcome::Cancelled));
            CHECK_FALSE(result.completionScope);
            state.context = context(); state.alive = true; state.submitted = true;
            CHECK(progress.observe(state, 2).outcome == result.outcome);
        }
    }
    errorCode([&]{ ActionProgress invalid(ActionKind::EquipWear, context(), 0, 1000, 10, 11, 2, 2, 5); }, "invalid_argument");
    errorCode([&]{ ActionProgress invalid(ActionKind::EquipWear, context(), 0, 1000, 10, 11, 1, 2, 0); }, "invalid_argument");
}

TEST_CASE("combat completion requires own animation and until dead never equates disappearance with death")
{
    for(const auto kind: {ActionKind::AttackOnce, ActionKind::Cast}){
        ActionProgress progress(kind, context(), 0, 30000);
        auto state = actionObservation(); state.submitted = true; state.actionFinished = true;
        CHECK(progress.observe(state, 1).outcome == OperationOutcome::Running);
        state.actionFinished = false; state.actionStarted = true;
        CHECK(progress.observe(state, 2).outcome == OperationOutcome::Running);
        state.actionStarted = false; state.actionFinished = true;
        CHECK(progress.observe(state, 3).outcome == OperationOutcome::Succeeded);
    }
    ActionProgress lost(ActionKind::AttackUntilDead, context(), 0, 30000);
    auto state = actionObservation(); state.submitted = true; state.targetVisible = false;
    const auto result = lost.observe(state, 1);
    CHECK(result.outcome == OperationOutcome::Failed); REQUIRE(result.error); CHECK(result.error->code == "target_lost");
    state.targetDead = true; CHECK(lost.observe(state, 2).outcome == OperationOutcome::Failed);
    ActionProgress witnessed(ActionKind::AttackUntilDead, context(), 0, 30000);
    CHECK(witnessed.observe(state, 1).outcome == OperationOutcome::Succeeded);
}

TEST_CASE("all action kinds reject stale context death timeout and explicit errors before success")
{
    for(const auto kind: actionKinds){
        for(const bool submitted: {false,true}){
            CAPTURE(int(kind), submitted);
            for(const auto field: {"session_id", "world_epoch", "control_epoch", "death"}){
                ActionProgress progress(kind, context(), 0, 1000, 10, 11, 2);
                auto state = actionObservation(); completeObservation(kind, state); state.submitted = submitted;
                if(field == std::string("death")){ state.alive = false; }
                else { state.context[field] = "different"; }
                CHECK(progress.observe(state, 1).outcome == (submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled));
            }
            ActionProgress timeout(kind, context(), 0, 1000, 10, 11, 2);
            auto state = actionObservation(); completeObservation(kind, state); state.submitted = submitted;
            CHECK(timeout.observe(state, 1000).outcome == (submitted ? OperationOutcome::Unknown : OperationOutcome::Failed));
            ActionProgress rejected(kind, context(), 0, 1000, 10, 11, 2);
            state.rejection = OperationError{"server_rejected", "明确拒绝", Json::object()};
            CHECK(rejected.observe(state, 1).outcome == OperationOutcome::Failed);
            ActionProgress cancelled(kind, context(), 0, 1000, 10, 11, 2);
            CHECK(cancelled.cancel(submitted).outcome == (submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled));
        }
    }
}

TEST_CASE("action evidence preserves sticky submission and bounds observation payloads")
{
    ActionProgress progress(ActionKind::UseItem, context(), 0, 30000, 10, 11);
    auto state = actionObservation(); state.submitted = true;
    CHECK(progress.observe(state, 1).submitted);
    state.submitted = false; state.alive = false;
    const auto stopped = progress.observe(state, 2); CHECK(stopped.submitted); CHECK(stopped.outcome == OperationOutcome::Unknown);
    ActionProgress bounded(ActionKind::PickUp, context(), 0, 1000);
    state = actionObservation(); state.itemChanges.resize(101);
    errorCode([&]{ bounded.observe(state, 1); }, "unavailable");
    state.itemChanges = {{1,2,1,2},{1,2,2,3}};
    errorCode([&]{ bounded.observe(state, 1); }, "unavailable");
    errorCode([&]{ ActionProgress invalid(ActionKind::Cast, context(), 0, 30001); }, "invalid_argument");
    errorCode([&]{ ActionProgress invalid(ActionKind::UseItem, context(), 0, 1, 1, 1, 0); }, "invalid_argument");
}

TEST_CASE("controller and real action helper retain dedup takeover and late resolution semantics")
{
    for(const auto kind: actionKinds){
        CAPTURE(int(kind));
        const auto method = kind == ActionKind::PickUp ? "game_pick_up" : kind == ActionKind::UseItem ? "game_use_item" :
            (kind == ActionKind::AttackOnce || kind == ActionKind::AttackUntilDead) ? "game_attack" : "game_cast";
        Json params = kind == ActionKind::PickUp ? Json{{"ground_handle","ground"}} : kind == ActionKind::UseItem ?
            Json{{"item_id",10},{"seq_id",11},{"count",2}} : (kind == ActionKind::AttackOnce || kind == ActionKind::AttackUntilDead) ?
            Json{{"target_handle","target"},{"mode", kind == ActionKind::AttackOnce ? "once" : "until_dead"}} : Json{{"magic_id",1}};
        uint64_t now = 0; int sends = 0, cancels = 0;
        ControllerOptions options; options.instanceId = std::string(32, 'a'); options.build = "actions-test";
        options.policy = Policy::parse(R"({"allowed_actions":"*","allowed_item_ids":"*","max_item_count_per_operation":100})");
        options.monotonicMs = [&]{ return now; }; options.unixMs = [&]{ return 100000 + now; };
        options.newSessionId = []{ return std::string(32,'b'); };
        AiBridgeController controller(options); controller.onConnected(1); controller.observe({"run",100,200,true});
        REQUIRE(controller.enableByUser());
        auto state = actionObservation(); state.context = controller.context();
        std::optional<ActionProgress> progress;
        ActionAdapter adapter;
        adapter.validate = [](const Json &, const Json &){ return ValidationResult{}; };
        adapter.begin = [&](const OperationView &view){
            ++sends; progress.emplace(kind, view.context, now, view.deadlineMs-now,10,11,2);
            state.submitted = true; return progress->observe(state,now);
        };
        adapter.tick = [&](const OperationView &)->std::optional<OperationUpdate>{ return progress->observe(state,now); };
        adapter.cancel = [&](const OperationView &view){ ++cancels; return progress->cancel(view.submitted); };
        controller.registerAction(method,adapter);
        Json request = {{"v",1},{"id","action"},{"method",method},{"params",params},
            {"request_id","00000000-0000-4000-8000-000000000001"},{"context",controller.context()},{"timeout_ms",1000}};
        auto accepted = controller.handleRequest({1,request}); REQUIRE(accepted.at("ok") == true);
        const auto id = accepted.at("data").at("operation_id").get<std::string>();
        controller.advance(); CHECK(sends == 1);
        request["id"] = "retry";
        CHECK(controller.handleRequest({1,request}).at("data").at("operation_id") == id);
        controller.advance(); CHECK(sends == 1);
        const auto status = [&]{ return controller.handleRequest({1,{{"v",1},{"id","status"},{"method","game_get_operation"},
            {"params",{{"operation_id",id}}}}}).at("data"); };
        now = 1000; controller.checkDeadlines(); CHECK(cancels == 1); CHECK(status().at("state") == "unknown");
        controller.manualInput();
        // A new independently correlated late observation goes to the original operation, never reopens its helper.
        ActionProgress receipt(kind,state.context,0,30000,10,11,2); completeObservation(kind,state);
        controller.report(id,receipt.observe(state,1001));
        auto after = status(); CHECK(after.at("state") == "unknown"); REQUIRE_FALSE(after.at("late_resolution").is_null());
        CHECK(after.at("late_resolution").at("state") == "succeeded"); CHECK(sends == 1);
        CHECK(controller.controlState() == ControlState::Paused);
    }
}

TEST_CASE("queued combat pickup and consumption cannot execute after human takeover")
{
    for(const auto method: {"game_attack","game_cast","game_pick_up","game_use_item"}){
        CAPTURE(method);
        uint64_t now=0; int begins=0;
        ControllerOptions options; options.instanceId=std::string(32,'a'); options.build="actions-test";
        options.policy=Policy::parse(R"({"allowed_actions":"*","allowed_item_ids":"*","max_item_count_per_operation":100})");
        options.monotonicMs=[&]{return now;}; options.unixMs=[] {return 100000;}; options.newSessionId=[] {return std::string(32,'b');};
        AiBridgeController controller(options); controller.onConnected(1); controller.observe({"run",100,200,true});
        REQUIRE(controller.enableByUser());
        ActionAdapter adapter;
        adapter.validate=[](const Json &,const Json &){return ValidationResult{};};
        adapter.begin=[&](const OperationView &){++begins; return OperationUpdate{.submitted=true};};
        adapter.tick=[](const OperationView &)->std::optional<OperationUpdate>{return {};};
        adapter.cancel=[](const OperationView &v){return OperationUpdate{.outcome=OperationOutcome::Cancelled,.submitted=v.submitted};};
        controller.registerAction(method,adapter);
        Json params=method==std::string("game_attack") ? Json{{"target_handle","target"},{"mode","once"}} :
            method==std::string("game_cast") ? Json{{"magic_id",1}} : method==std::string("game_pick_up") ? Json{{"ground_handle","ground"}} :
            Json{{"item_id",10},{"seq_id",11},{"count",1}};
        Json request={{"v",1},{"id","first"},{"method",method},{"params",params},{"context",controller.context()},
            {"request_id","00000000-0000-4000-8000-000000000001"},{"timeout_ms",1000}};
        const auto accepted=controller.handleRequest({1,request}); REQUIRE(accepted.at("ok")==true);
        controller.manualInput(); controller.advance(); CHECK(begins==0);
        const auto status=controller.handleRequest({1,{{"v",1},{"id","status"},{"method","game_get_operation"},
            {"params",{{"operation_id",accepted.at("data").at("operation_id")}}}}}).at("data");
        CHECK(status.at("state")=="cancelled"); CHECK(controller.controlState()==ControlState::Paused);
        request["id"]="after"; request["request_id"]="00000000-0000-4000-8000-000000000002";
        CHECK(controller.handleRequest({1,request}).at("ok")==false); CHECK(begins==0);
    }
}

TEST_CASE("shared combat policy checks all affected players even without an explicit primary target")
{
    const auto onlyFirst=Policy::parse(R"({"allow_pvp":true,"allowed_peer_uids":["18446744073709551615"]})");
    const uint64_t first=std::numeric_limits<uint64_t>::max();
    CHECK_FALSE(validateCombatPeers(onlyFirst,{}));
    CHECK_FALSE(validateCombatPeers(onlyFirst,{first}));
    REQUIRE(validateCombatPeers(onlyFirst,{first,100}));
    CHECK(validateCombatPeers(onlyFirst,{first,100})->code=="policy_denied");
    const auto explicitAll=Policy::parse(R"({"allow_pvp":true,"allowed_peer_uids":"*"})");
    CHECK_FALSE(validateCombatPeers(explicitAll,{first,100}));
    const auto noPvp=Policy::parse(R"({"allow_pvp":false,"allowed_peer_uids":"*"})");
    CHECK(validateCombatPeers(noPvp,{first})->code=="policy_denied");
    const auto noTargets=Policy::parse(R"({"allow_pvp":true,"allowed_peer_uids":[]})");
    CHECK(validateCombatPeers(noTargets,{first})->code=="policy_denied");
    CHECK_FALSE(validateCombatPeers(explicitAll,std::vector<uint64_t>(8192,first)));
    CHECK(validateCombatPeers(explicitAll,std::vector<uint64_t>(8193,first))->code=="unavailable");
}

TEST_CASE("shared cast target validation prevents coordinate spells from silently using self or stale focus")
{
    CHECK_FALSE(validateCastTarget("target",false,true,false));
    CHECK(validateCastTarget("target",false,false,true)->code=="invalid_argument");
    CHECK(validateCastTarget("target",true,false,true)->code=="invalid_argument");
    CHECK(validateCastTarget("target",false,false,false)->code=="invalid_argument");
    CHECK(validateCastTarget("target",false,true,false,true)->code=="invalid_argument");
    CHECK_FALSE(validateCastTarget("target",true,true,false,true));
    CHECK_FALSE(validateCastTarget("target",true,false,false));
    CHECK_FALSE(validateCastTarget("ground",false,false,true));
    CHECK(validateCastTarget("ground",false,true,true)->code=="invalid_argument");
    CHECK(validateCastTarget("ground",false,false,false)->code=="invalid_argument");
    for(const auto type:{"self","toggle"}){
        CHECK_FALSE(validateCastTarget(type,false,false,false));
        CHECK(validateCastTarget(type,false,true,false)->code=="invalid_argument");
        CHECK(validateCastTarget(type,false,false,true)->code=="invalid_argument");
    }
    CHECK(validateCastTarget("unsupported",false,false,false)->code=="unsupported_skill");
}
