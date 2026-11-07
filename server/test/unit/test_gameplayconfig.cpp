#include <chrono>
#include <fstream>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "gameplayconfig.hpp"

namespace
{
    using Json = nlohmann::json;
    const std::filesystem::path source = MIR2X_TEST_SOURCE_DIR;

    Json read(const std::filesystem::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        return Json::parse(in);
    }

    struct Fixture
    {
        std::filesystem::path root = std::filesystem::temp_directory_path() / ("mir2x-gameplay-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Fixture()
        {
            std::filesystem::copy(source / "server/config/gameplay", root, std::filesystem::copy_options::recursive);
        }
        ~Fixture()
        {
            if(std::filesystem::weakly_canonical(root).parent_path() == std::filesystem::canonical(std::filesystem::temp_directory_path())){
                std::filesystem::remove_all(root);
            }
        }
    };
}

TEST_CASE("gameplay schema contract rejects invalid input before publishing a snapshot")
{
    const auto contract = read(source / "server/test/data/gameplay/contract/expected-results.json");
    for(const auto &entry: contract.at("cases")){
        DYNAMIC_SECTION(entry.at("name").get<std::string>()){
            Fixture fixture;
            if(entry.contains("file")){
                std::ofstream out(fixture.root / entry.at("file").get<std::string>(), std::ios::binary);
                out << (entry.contains("raw") ? entry.at("raw").get<std::string>() : entry.at("data").dump());
            }
            if(entry.at("valid").get<bool>()){
                REQUIRE_NOTHROW(GameplayConfig::load(fixture.root));
            }
            else{
                REQUIRE_THROWS(GameplayConfig::load(fixture.root));
            }
        }
    }
}

TEST_CASE("gameplay values and ordered expanded drops match the frozen production baseline")
{
    const auto config = GameplayConfig::load(source / "server/config/gameplay");
    const auto baseline = read(source / "tools/gameplayconfig/migration/baseline.json");
    for(const auto &monster: baseline.at("monster_experience")){
        CHECK(config.monsterExp(monster.at("monster_id").get<uint32_t>()) == monster.at("exp").get<int>());
    }
    Json expanded = Json::array();
    for(const auto &monster: baseline.at("catalog").at("monsters")){
        const auto id = monster.at("id").get<uint32_t>();
        for(const auto &[group, items]: config.monsterDropTable(id)){
            for(const auto &item: items){
                expanded.push_back({id, group, item.itemID, item.probRecip, item.count});
            }
        }
    }
    CHECK(expanded == baseline.at("expanded_production_drops"));
    CHECK_THROWS(config.monsterExp(0));
    CHECK_THROWS(config.monsterDropTable(0));
    CHECK_THROWS(config.mapSpawns(0));
}

TEST_CASE("gameplay snapshot is stable after editing disk and a new load uses new base experience")
{
    Fixture fixture;
    auto values = read(fixture.root / "monsters.json");
    const auto id = values["monsters"][0]["monster_id"].get<uint32_t>();
    const auto first = GameplayConfig::load(fixture.root);
    const auto previous = first.monsterExp(id);
    values["monsters"][0]["exp"] = previous + 1;
    { std::ofstream out(fixture.root / "monsters.json"); out << values; }
    const auto second = GameplayConfig::load(fixture.root);
    CHECK(first.monsterExp(id) == previous);
    CHECK(second.monsterExp(id) == previous + 1);
    std::filesystem::remove(fixture.root / "manifest.json");
    CHECK_THROWS(GameplayConfig::load(fixture.root));
    CHECK(first.monsterExp(id) == previous);
}

TEST_CASE("independent multipliers support old bundles decimals zero and immutable snapshots")
{
    Fixture fixture;
    std::filesystem::remove(fixture.root / "multipliers.json");
    const auto original = GameplayConfig::load(fixture.root);
    CHECK(original.multipliers().experience == 1.0);
    CHECK(original.multipliers().gold == 1.0);
    CHECK(original.multipliers().drop == 1.0);
    {
        std::ofstream out(fixture.root / "multipliers.json");
        out << R"({"schema_version":1,"experience":1.5,"gold":0,"drop":0.25})";
    }
    const auto changed = GameplayConfig::load(fixture.root);
    CHECK(original.multipliers().experience == 1.0);
    CHECK(changed.multipliers().scaleExperience(3) == 5);
    CHECK(changed.multipliers().gold == 0.0);
    CHECK(changed.multipliers().drop == 0.25);
    CHECK(GameplayMultipliers{.experience = 0.0}.scaleExperience(100) == 0);
    CHECK(GameplayMultipliers{.experience = 0.1}.scaleExperience(1) == 0);
    CHECK(GameplayMultipliers{.experience = 1e308}.scaleExperience(100) == std::numeric_limits<int>::max());
    for(const auto &value: {Json(-1), Json(true), Json("2"), Json(nullptr)}){
        {
            std::ofstream out(fixture.root / "multipliers.json");
            out << Json{{"schema_version", 1}, {"experience", value}, {"gold", 1}, {"drop", 1}};
        }
        CHECK_THROWS(GameplayConfig::load(fixture.root));
    }
}
