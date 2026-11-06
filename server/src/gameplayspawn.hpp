#pragma once
#include <sol/sol.hpp>
#include "gameplayconfig.hpp"

inline sol::table makeMapSpawnTable(sol::state_view state, const std::vector<SpawnGroup> &groups)
{
    auto result = state.create_table();
    for(const auto &group: groups){
        auto entry = state.create_table();
        entry["name"] = group.monsterName;
        auto locations = state.create_table();
        for(const auto &region: group.loc){
            auto loc = state.create_table();
            loc["x"] = region.x;
            loc["y"] = region.y;
            loc["w"] = region.w;
            loc["h"] = region.h;
            loc["count"] = region.count;
            loc["time"] = region.time;
            if(region.cratio){
                loc["cratio"] = *region.cratio;
            }
            locations.add(loc);
        }
        entry["loc"] = locations;
        result.add(entry);
    }
    return result;
}
