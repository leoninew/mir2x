#include <chrono>
#include <fstream>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "server.hpp"
#include "peerconfig.hpp"
#include "serverargparser.hpp"
#include "dropitemconfig.hpp"

class ActorPool;
class DBPod;
class MapBinDB;
class ScriptWindow;
class ProfilerWindow;
class MainWindow;
class ServerPasswordWindow;
class ServerConfigureWindow;
class PodMonitorWindow;
class ActorMonitorWindow;

ServerArgParser       *g_serverArgParser = nullptr;
PeerConfig            *g_peerConfig = nullptr;
Log                   *g_mir2xLog = nullptr;
ActorPool             *g_actorPool = nullptr;
DBPod                 *g_dbPod = nullptr;
MapBinDB              *g_mapBinDB = nullptr;
ScriptWindow          *g_scriptWindow = nullptr;
ProfilerWindow        *g_profilerWindow = nullptr;
MainWindow            *g_mainWindow = nullptr;
Server                *g_server = nullptr;
ServerPasswordWindow  *g_serverPasswordWindow = nullptr;
ServerConfigureWindow *g_serverConfigureWindow = nullptr;
PodMonitorWindow      *g_podMonitorWindow = nullptr;
ActorMonitorWindow    *g_actorMonitorWindow = nullptr;

TEST_CASE("gameplay production drops retain repeat, group order, gold fluctuation and startup snapshot")
{
    using Json = nlohmann::json;
    const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
    const auto root = parent / ("mir2x-gameplay-drop-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup
    {
        std::filesystem::path root;
        std::filesystem::path parent;
        ~Cleanup()
        {
            if(std::filesystem::weakly_canonical(root).parent_path() == parent){
                std::filesystem::remove_all(root);
            }
            g_server = nullptr;
            g_peerConfig = nullptr;
            g_serverArgParser = nullptr;
        }
    } cleanup{root, parent};
    std::filesystem::copy(std::filesystem::path(MIR2X_TEST_SOURCE_DIR) / "server/config/gameplay", root, std::filesystem::copy_options::recursive);
    char program[] = "gameplay-test";
    char slave[] = "--slave";
    char master[] = "--master-ip=127.0.0.1";
    char *argv[] = {program, slave, master, nullptr};
    const argf::parser arguments(3, argv);
    ServerArgParser parser(arguments);
    PeerConfig peer;
    Server server;
    g_serverArgParser = &parser;
    g_peerConfig = &peer;
    g_server = &server;

    CHECK_THROWS_AS(server.gameplayConfig(), std::logic_error);
    CHECK_THROWS(server.initializeGameplayConfig(root / "missing"));
    CHECK_THROWS_AS(server.gameplayConfig(), std::logic_error);
    const auto manifest = Json::parse(std::ifstream(root / "manifest.json"));
    const auto monsterID = manifest["drop_monsters"][0].get<uint32_t>();
    const auto path = root / "drops" / (std::to_string(monsterID) + ".json");
    auto doc = Json::parse(std::ifstream(path));
    uint32_t itemID = 0;
    for(uint32_t id = 1; id < DBCOM_ITEMENDID(); ++id){
        const auto &record = DBCOM_ITEMRECORD(id);
        if(record && record.packable() && !record.isGold()){
            itemID = id;
            break;
        }
    }
    REQUIRE(itemID != 0);
    const auto item = [&](uint32_t id, int group, int repeat, int count) -> Json
    {
        return {{"item_id", id}, {"item_name", reinterpret_cast<const char *>(DBCOM_ITEMRECORD(id).name)},
                {"group", group}, {"prob_recip", 1}, {"repeat", repeat}, {"count", count}};
    };
    doc["items"] = Json::array({item(itemID, 0, 2, 3), item(DBCOM_ITEMID(u8"金币（小）"), 0, 1, 100),
                                item(itemID, 1, 1, 7), item(itemID, 1, 1, 9)});
    { std::ofstream out(path); out << doc; }
    server.initializeGameplayConfig(root);
    CHECK_THROWS_AS(server.initializeGameplayConfig(root), std::logic_error);
    std::filesystem::remove(path);
    const auto drops = getMonsterDropItemList(monsterID);
    REQUIRE(drops.size() == 4);
    CHECK(drops[0].itemID == itemID);
    CHECK(drops[0].count == 3);
    CHECK(drops[1].itemID == itemID);
    CHECK(drops[1].count == 3);
    CHECK(DBCOM_ITEMRECORD(drops[2].itemID).isGold());
    CHECK(drops[2].count >= 90);
    CHECK(drops[2].count <= 110);
    CHECK(drops[3].itemID == itemID);
    CHECK(drops[3].count == 7);
}
