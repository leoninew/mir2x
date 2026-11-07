#include <chrono>
#include <fstream>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <nlohmann/json.hpp>
#include "server.hpp"
#include "peerconfig.hpp"
#include "serverargparser.hpp"
#include "dropitemconfig.hpp"
#include "dbpod.hpp"
#include "mainwindow.hpp"
#include "serverpasswordwindow.hpp"

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

TEST_CASE("server launch rejects missing gameplay configuration before runtime startup")
{
    REQUIRE(g_dbPod == nullptr);
    REQUIRE(g_mapBinDB == nullptr);
    REQUIRE(g_actorPool == nullptr);
    Server server;
    CHECK_THROWS_WITH(server.launch(), "gameplay configuration not initialized");
}

TEST_CASE("manual launch keeps the validated gameplay snapshot when database startup fails")
{
    const auto previousDirectory = std::filesystem::current_path();
    const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
    const auto root = parent / ("mir2x-manual-launch-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "mir2x.db3");
    struct Cleanup
    {
        std::filesystem::path previousDirectory;
        std::filesystem::path root;
        std::filesystem::path parent;
        ~Cleanup()
        {
            std::filesystem::current_path(previousDirectory);
            delete g_server;
            g_server = nullptr;
            g_dbPod = nullptr;
            g_mainWindow = nullptr;
            g_serverPasswordWindow = nullptr;
            g_serverArgParser = nullptr;
            if(std::filesystem::weakly_canonical(root).parent_path() == parent){
                std::filesystem::remove_all(root);
            }
        }
    } cleanup{previousDirectory, root, parent};
    char program[] = "manual-launch-test";
    char *argv[] = {program, nullptr};
    const argf::parser arguments(1, argv);
    ServerArgParser parser(arguments);
    g_serverArgParser = &parser;
    DBPod database;
    g_dbPod = &database;
    g_server = new Server();
    g_server->initializeGameplayConfig(std::filesystem::path(MIR2X_TEST_SOURCE_DIR) / "server/config/gameplay");
    const auto &before = g_server->gameplayConfig();
    const auto mapID = DBCOM_MAPID(u8"边境城市_01");
    const auto expectedGroups = before.mapSpawns(mapID).size();
    REQUIRE(expectedGroups > 0);
    ServerPasswordWindow password;
    MainWindow window;
    g_mainWindow = &window;
    g_serverPasswordWindow = &password;
    std::unique_ptr<Fl_Window> passwordWidgets(password.m_window);
    std::unique_ptr<Fl_Window> mainWidgets(window.m_window);
    auto menu = dynamic_cast<Fl_Menu_Bar *>(window.m_window->child(0));
    REQUIRE(menu != nullptr);
    window.m_launchMenu->do_callback(menu);
    password.hideAll();
    std::filesystem::current_path(root);
    // Exercise the production callback without starting a database, map or
    // network listener. A directory cannot be opened as the SQLite database.
    CHECK_THROWS(password.triggerCallback());
    REQUIRE_NOTHROW(g_server->gameplayConfig());
    CHECK(&g_server->gameplayConfig() == &before);
    CHECK(g_server->gameplayConfig().mapSpawns(mapID).size() == expectedGroups);
}

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
    double goldMultiplier = 1.0;
    double dropMultiplier = 1.0;
    int expectedGold = 100;
    SECTION("default multipliers retain baseline") {}
    SECTION("fractional multipliers compose with peer rates before rounding")
    {
        peer.setConfig({.dropRate = 4.0, .goldRate = 3.0});
        goldMultiplier = 0.5;
        dropMultiplier = 0.5;
        expectedGold = 150;
        for(auto &entry: doc["items"]){
            entry["prob_recip"] = 2;
        }
    }
    SECTION("zero drop multiplier disables all drops") { dropMultiplier = 0.0; }
    SECTION("zero gold multiplier disables gold only") { goldMultiplier = 0.0; }
    {
        std::ofstream out(root / "multipliers.json");
        out << Json{{"schema_version", 1}, {"experience", 1}, {"gold", goldMultiplier}, {"drop", dropMultiplier}};
    }
    { std::ofstream out(path); out << doc; }
    server.initializeGameplayConfig(root);
    CHECK_THROWS_AS(server.initializeGameplayConfig(root), std::logic_error);
    std::filesystem::remove(path);
    const auto drops = getMonsterDropItemList(monsterID);
    if(dropMultiplier == 0.0){
        REQUIRE(drops.empty());
        return;
    }
    REQUIRE(drops.size() == (goldMultiplier == 0.0 ? 3 : 4));
    CHECK(drops[0].itemID == itemID);
    CHECK(drops[0].count == 3);
    CHECK(drops[1].itemID == itemID);
    CHECK(drops[1].count == 3);
    if(goldMultiplier != 0.0){
        CHECK(DBCOM_ITEMRECORD(drops[2].itemID).isGold());
        CHECK(drops[2].count >= expectedGold * 0.9);
        CHECK(drops[2].count <= expectedGold * 1.1);
    }
    CHECK(drops.back().itemID == itemID);
    CHECK(drops.back().count == 7);
}
