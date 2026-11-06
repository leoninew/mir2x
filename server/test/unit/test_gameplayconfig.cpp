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
