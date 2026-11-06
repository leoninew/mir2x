#pragma once
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "dropitemconfigtype.hpp"

struct SpawnRegion
{
    int x;
    int y;
    int w;
    int h;
    int count;
    int time;
    std::optional<int> cratio;
};

struct SpawnGroup
{
    uint32_t monsterID;
    std::string monsterName;
    std::vector<SpawnRegion> loc;
};

class GameplayConfig final
{
    private:
        std::unordered_map<uint32_t, int> m_exp;
        std::unordered_map<uint32_t, std::map<int, std::vector<DropItemConfig>>> m_drops;
        std::unordered_map<uint32_t, std::vector<SpawnGroup>> m_spawns;

    public:
        static GameplayConfig load(const std::filesystem::path &);
        int monsterExp(uint32_t) const;
        const std::map<int, std::vector<DropItemConfig>> &monsterDropTable(uint32_t) const;
        const std::vector<SpawnGroup> &mapSpawns(uint32_t) const;
};
