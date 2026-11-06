#include <iostream>
#include <charconv>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#include <nlohmann/json.hpp>
#include "dbcomid.hpp"
#include "gameplayconfig.hpp"

namespace
{
    using Json = nlohmann::json;

    uint32_t identity(std::string_view text)
    {
        uint32_t value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if(error != std::errc{} || end != text.data() + text.size() || !value){
            throw std::invalid_argument("Identity must be a positive uint32 integer");
        }
        return value;
    }

    template<typename Record> Json identities(uint32_t endID, Record record)
    {
        auto result = Json::array();
        for(uint32_t id = 1; id < endID; ++id){
            if(const auto &row = record(id); row){
                result.push_back({{"id", id}, {"name", reinterpret_cast<const char *>(row.name)}});
            }
        }
        return result;
    }
}

int main(int argc, char *argv[])
{
    try{
#ifdef _WIN32
        // The CRT's narrow argv follows the system code page, not UTF-8.
        int wideCount = 0;
        const auto wideArgs = CommandLineToArgvW(GetCommandLineW(), &wideCount);
        if(!wideArgs){
            throw std::runtime_error("Unable to read Unicode command line");
        }
        const std::unique_ptr<void, decltype(&LocalFree)> owner(wideArgs, LocalFree);
        if(wideCount != argc){
            throw std::runtime_error("Unexpected Unicode argument count");
        }
        const auto configPath = [&]() { return std::filesystem::path(wideArgs[2]); };
#else
        const auto configPath = [&]() { return std::filesystem::u8path(argv[2]); };
#endif
        if(argc == 2 && std::string_view(argv[1]) == "catalog"){
            std::cout << Json
            {
                {"schema_version", 1},
                {"monsters", identities(DBCOM_MONSTERENDID(), [](uint32_t id) -> const auto & { return DBCOM_MONSTERRECORD(id); })},
                {"items", identities(DBCOM_ITEMENDID(), [](uint32_t id) -> const auto & { return DBCOM_ITEMRECORD(id); })},
                {"maps", identities(DBCOM_MAPENDID(), [](uint32_t id) -> const auto & { return DBCOM_MAPRECORD(id); })},
            }.dump(2) << '\n';
            return 0;
        }
        if(argc == 3 && std::string_view(argv[1]) == "validate"){
            const auto config = GameplayConfig::load(configPath());
            (void)config;
            std::cout << "Gameplay configuration valid\n";
            return 0;
        }
        if(argc == 5 && std::string_view(argv[1]) == "inspect"){
            const auto config = GameplayConfig::load(configPath());
            const auto monsterID = identity(argv[3]);
            const auto mapID = identity(argv[4]);
            Json result{{"exp", config.monsterExp(monsterID)}, {"drops", Json::array()}, {"spawns", Json::array()}};
            for(const auto &[group, items]: config.monsterDropTable(monsterID)){
                for(const auto &item: items){
                    result["drops"].push_back({{"group", group}, {"item_id", item.itemID}, {"prob_recip", item.probRecip}, {"count", item.count}});
                }
            }
            for(const auto &group: config.mapSpawns(mapID)){
                Json locations = Json::array();
                for(const auto &loc: group.loc){
                    Json row{{"x", loc.x}, {"y", loc.y}, {"w", loc.w}, {"h", loc.h}, {"count", loc.count}, {"time", loc.time}};
                    if(loc.cratio){
                        row["cratio"] = *loc.cratio;
                    }
                    locations.push_back(std::move(row));
                }
                result["spawns"].push_back({{"monster_id", group.monsterID}, {"monster_name", group.monsterName}, {"loc", std::move(locations)}});
            }
            std::cout << result.dump() << '\n';
            return 0;
        }
        std::cerr << "Usage: gameplay-config catalog | validate <gameplay-dir> | inspect <gameplay-dir> <monster-id> <map-id>\n";
        return 2;
    }
    catch(const std::exception &error){
        std::cerr << error.what() << '\n';
        return 1;
    }
}
