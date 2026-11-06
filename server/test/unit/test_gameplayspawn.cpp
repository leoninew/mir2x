#include <fstream>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "gameplayspawn.hpp"

TEST_CASE("map spawn tables keep all configured groups and regions and isolate runtime mutations")
{
    const std::filesystem::path root = MIR2X_TEST_SOURCE_DIR;
    const auto config = GameplayConfig::load(root / "server/config/gameplay");
    std::ifstream in(root / "server/config/gameplay/manifest.json");
    const auto manifest = nlohmann::json::parse(in);
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::math, sol::lib::table, sol::lib::string);
    for(const auto &id: manifest.at("spawn_maps")){
        const auto &groups = config.mapSpawns(id.get<uint32_t>());
        auto first = makeMapSpawnTable(lua, groups);
        auto second = makeMapSpawnTable(lua, groups);
        REQUIRE(first.size() == groups.size());
        for(size_t i = 0; i < groups.size(); ++i){
            sol::table entry = first[i + 1];
            CHECK(entry["name"].get<std::string>() == groups[i].monsterName);
            sol::table regions = entry["loc"];
            REQUIRE(regions.size() == groups[i].loc.size());
            for(size_t j = 0; j < groups[i].loc.size(); ++j){
                sol::table loc = regions[j + 1];
                const auto &expected = groups[i].loc[j];
                CHECK(loc["x"].get<int>() == expected.x);
                CHECK(loc["y"].get<int>() == expected.y);
                CHECK(loc["w"].get<int>() == expected.w);
                CHECK(loc["h"].get<int>() == expected.h);
                CHECK(loc["count"].get<int>() == expected.count);
                CHECK(loc["time"].get<int>() == expected.time);
                CHECK(loc["cratio"].get<sol::optional<int>>().has_value() == expected.cratio.has_value());
                loc["uidList"] = lua.create_table_with(1, 42);
                loc["count"] = -1;
                sol::table other = second[i + 1]["loc"][j + 1];
                CHECK_FALSE(other["uidList"].valid());
                CHECK(other["count"].get<int>() == expected.count);
            }
        }
    }
}

TEST_CASE("the original monGener consumes fresh tables without changing count zero or spawn timing")
{
    const std::filesystem::path root = MIR2X_TEST_SOURCE_DIR;
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::math, sol::lib::table, sol::lib::string);
    int now = 1;
    int sent = 0;
    lua.set_function("assertType", [](sol::object, const std::string &) {});
    lua.set_function("getMonsterID", [](const std::string &) { return 1; });
    lua.set_function("countGLoc", [](int, int, int, int) { return 64; });
    lua.set_function("getMapName", [] { return "isolated"; });
    lua.set_function("getTime", [&now] { return now; });
    lua.set_function("uidAlive", [](uint64_t) { return true; });
    lua.set_function("randGLoc", [](int x, int y, int, int) { return std::make_tuple(x, y); });
    lua.set_function("addMonster", [&sent](const std::string &, int, int, bool) { return ++sent; });
    lua.set_function("addLog", [](sol::variadic_args) {});
    lua.set_function("fatalPrintf", [](sol::variadic_args) { throw std::runtime_error("unexpected invalid region"); });
    sol::table addmon = lua.script_file((root / "server/script/map/addmonster.lua").string());
    const std::vector<SpawnGroup> groups{{1, "sample", {{0, 0, 8, 8, 0, 10, 99}}}};
    auto first = makeMapSpawnTable(lua, groups);
    auto second = makeMapSpawnTable(lua, groups);
    lua["addmon"] = addmon;
    lua["first"] = first;
    lua["second"] = second;
    lua.script("firstCo = addmon.monGener(first); secondCo = addmon.monGener(second); assert(coroutine.resume(firstCo))");
    CHECK(sent == 1); // Existing count=0 behavior still produces at least one monster.
    CHECK(groups[0].loc[0].count == 0);
    CHECK(first[1]["loc"][1]["uidList"].get<sol::table>().size() == 1);
    CHECK(second[1]["loc"][1]["uidList"].get<sol::table>().size() == 0);
    lua.script("assert(coroutine.resume(secondCo)); assert(coroutine.resume(firstCo))");
    CHECK(sent == 2);
    now = 10002;
    lua.script("assert(coroutine.resume(firstCo))");
    CHECK(sent == 2);
    CHECK(first[1]["loc"][1]["cratio"].get<int>() == 99);
}
