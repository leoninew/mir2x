#include <catch2/catch_test_macros.hpp>
#include <sol/sol.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    void runLuaSuite(const char *path, const std::vector<std::string> &arguments)
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::coroutine,
                           sol::lib::string, sol::lib::math, sol::lib::table,
                           sol::lib::io, sol::lib::os, sol::lib::debug, sol::lib::utf8);
#ifdef _WIN32
        // Lua's narrow fopen uses the Windows code page; fixture names are UTF-8.
        lua["_readTestScriptUtf8"] = [](const std::string &path) -> sol::optional<std::string>
        {
            std::ifstream input(std::filesystem::u8path(path), std::ios::binary);
            if(!input){
                return sol::nullopt;
            }
            return std::string(std::istreambuf_iterator<char>(input), {});
        };
        lua.script(R"lua(
            function loadfile(path, mode, env)
                local code = _readTestScriptUtf8(path)
                if not code then return nil, 'unable to open ' .. path end
                if env == nil then return load(code, '@' .. path, mode) end
                return load(code, '@' .. path, mode, env)
            end
            function dofile(path)
                local chunk, message = loadfile(path)
                if not chunk then error(message, 2) end
                return chunk()
            end
        )lua");
#endif
        const std::string root = MIR2X_TEST_SOURCE_DIR;
        auto arg = lua.create_table();
        for(size_t i = 0; i < arguments.size(); ++i){
            arg[i + 1] = arguments[i];
        }
        lua["arg"] = arg;
        const auto result = lua.safe_script_file(
            root + "/" + path,
            sol::script_pass_on_error);
        if(!result.valid()){
            const sol::error error = result;
            FAIL(error.what());
        }
        REQUIRE(result.valid());
    }
}

TEST_CASE("Introductory and side quests", "[questscripts]")
{
    runLuaSuite("server/test/unit/test_questscripts.lua", {MIR2X_TEST_SOURCE_DIR});
}

TEST_CASE("Warrior and wizard quests", "[magicquests]")
{
    runLuaSuite("server/test/unit/test_magicquests.lua", {MIR2X_TEST_SOURCE_DIR});
}

TEST_CASE("Taoist quests", "[taoistquests]")
{
    runLuaSuite("server/test/unit/test_taoistquests.lua", {MIR2X_TEST_SOURCE_DIR});
}

TEST_CASE("Story quests", "[worldquests]")
{
    runLuaSuite("server/test/unit/test_worldquests.lua", {MIR2X_TEST_SOURCE_DIR});
}

TEST_CASE("Merchant contracts and NPC entry paths", "[merchants]")
{
    std::vector<std::string> scripts;
    for(const auto &entry: std::filesystem::directory_iterator(
            std::filesystem::path(MIR2X_TEST_SOURCE_DIR) / "server/script/npc")){
        if(entry.is_regular_file() && entry.path().extension() == ".lua"){
            const auto utf8 = entry.path().u8string();
            scripts.emplace_back(reinterpret_cast<const char *>(utf8.data()), utf8.size());
        }
    }
    std::ranges::sort(scripts);
    REQUIRE_FALSE(scripts.empty());
    runLuaSuite("server/test/merchant.lua", scripts);
}
