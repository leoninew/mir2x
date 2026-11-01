#include "snapshot.hpp"
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <limits>
#include <random>
#include <set>
#include <sstream>

namespace aibridge
{
    namespace
    {
        template<typename T> Json nullable(const std::optional<T> &value)
        {
            return value ? Json(*value) : Json(nullptr);
        }
        Json scope(const Json &context)
        {
            validateDefinition(context, "context");
            auto result = context;
            result.erase("control_epoch");
            return result;
        }
        std::string nonce(std::string value)
        {
            if(value.empty()){
                std::random_device source;
                std::ostringstream text;
                text << std::hex << std::setfill('0');
                for(int i = 0; i < 4; ++i){ text << std::setw(8) << static_cast<uint32_t>(source()); }
                value = text.str();
            }
            if(value.size() > 32 || std::any_of(value.begin(), value.end(), [](char ch){
                return !(ch >= '0' && ch <= '9') && !(ch >= 'a' && ch <= 'z');
            })){ throw std::invalid_argument("Invalid snapshot nonce"); }
            return value;
        }
        uint64_t next(uint64_t &sequence)
        {
            if(sequence == std::numeric_limits<uint64_t>::max()){ throw ProtocolError("unavailable", "Snapshot identity exhausted"); }
            return ++sequence;
        }
        std::string_view listName(SnapshotList kind)
        {
            switch(kind){
                case SnapshotList::Nearby: return "nearby";
                case SnapshotList::Inventory: return "inventory";
            }
            throw std::invalid_argument("Invalid snapshot list");
        }
        Json filters(SnapshotList kind, const Json &input)
        {
            if(!input.is_object()){ throw ProtocolError("invalid_argument", "Invalid list filters"); }
            const std::string method = kind == SnapshotList::Nearby ? "game_list_nearby" : "game_get_inventory";
            validateSchema(input, contract().at("$defs").at(method + "Request").at("properties").at("params"));
            if(input.contains("cursor")){ throw ProtocolError("invalid_argument", "Filters must exclude cursor"); }
            if(kind == SnapshotList::Nearby){ return {{"kind", input.value("kind", "all")}, {"radius", input.value("radius", 15)}}; }
            return input;
        }
        bool validKind(std::string_view kind)
        {
            return kind == "player" || kind == "monster" || kind == "npc" || kind == "ground";
        }
        int64_t distance(GridPoint a, GridPoint b)
        {
            return std::max(std::abs(int64_t(a.x) - b.x), std::abs(int64_t(a.y) - b.y));
        }
    }

    Json gridPoint(GridPoint value)
    {
        Json result = {{"x", value.x}, {"y", value.y}};
        validateDefinition(result, "position");
        return result;
    }
    bool withinTolerance(GridPoint a, GridPoint b, int tolerance)
    {
        if(tolerance < 0 || tolerance > 2){ throw ProtocolError("invalid_argument", "Invalid movement tolerance"); }
        return distance(a, b) <= tolerance;
    }
    Json snapshotMetadata(SnapshotStamp stamp, std::string_view coverage, const std::vector<std::string> &unavailable)
    {
        return {{"snapshot_seq", std::to_string(stamp.sequence)}, {"observed_at", std::to_string(stamp.observedAt)},
            {"source", "client"}, {"coverage", coverage}, {"unavailable_fields", unavailable}};
    }
    Json stateSnapshot(const CharacterSnapshot &s)
    {
        auto result = snapshotMetadata(s.stamp, "client_known");
        result.update({{"character_uid", std::to_string(s.characterUid)}, {"map_uid", std::to_string(s.mapUid)},
            {"name", nullable(s.name)}, {"map_name", nullable(s.mapName)},
            {"position", gridPoint(s.position)}, {"action_endpoint", gridPoint(s.actionEndpoint)},
            {"direction", nullable(s.direction)}, {"hp", nullable(s.hp)}, {"max_hp", nullable(s.maxHp)},
            {"mp", nullable(s.mp)}, {"max_mp", nullable(s.maxMp)}, {"level", nullable(s.level)},
            {"gold", s.gold ? Json(std::to_string(*s.gold)) : Json(nullptr)}, {"action", nullable(s.action)},
            {"known_states", s.knownStates}});
        for(const auto *name: {"name", "map_name", "direction", "hp", "max_hp", "mp", "max_mp", "level", "gold", "action"}){
            if(result.at(name).is_null()){ result.at("unavailable_fields").push_back(name); }
        }
        validateDefinition(result, "state");
        return result;
    }

    ObservationHandles::ObservationHandles(std::string value): m_nonce(nonce(std::move(value))) {}
    void ObservationHandles::clear()
    {
        m_scope = nullptr; m_targets.clear(); m_ground.clear(); m_lastTargets.clear(); m_lastGround.clear();
    }
    void ObservationHandles::sync(const Json &context, const std::vector<TargetIdentity> &targets,
        const std::vector<GroundIdentity> &ground)
    {
        if(context.is_null()){ clear(); return; }
        if(targets.size() + ground.size() > FrozenPages::maxRows){ throw ProtocolError("unavailable", "Visible observation limit exceeded"); }
        const auto currentScope = scope(context);
        const bool sameScope = m_scope == currentScope;
        // Stable frames need no map allocation; a visibility/lifetime change still takes the full validation path.
        if(sameScope && targets == m_lastTargets && ground == m_lastGround){ return; }
        std::map<TargetIdentity, std::string> nextTargets;
        std::map<GroundIdentity, std::string> nextGround;
        std::set<uint64_t> uids;
        std::set<GridPoint> positions;
        for(const auto identity: targets){
            if(!identity.uid || !identity.incarnation || !uids.insert(identity.uid).second){
                throw ProtocolError("unavailable", "Invalid object observation identity");
            }
            const auto old = m_targets.find(identity);
            nextTargets.emplace(identity, sameScope && old != m_targets.end() ? old->second : "t-" + m_nonce + "-" + std::to_string(next(m_sequence)));
        }
        for(const auto identity: ground){
            gridPoint(identity.position);
            if(!identity.revision || !positions.insert(identity.position).second){ throw ProtocolError("unavailable", "Invalid ground observation identity"); }
            const auto old = m_ground.find(identity);
            nextGround.emplace(identity, sameScope && old != m_ground.end() ? old->second : "g-" + m_nonce + "-" + std::to_string(next(m_sequence)));
        }
        m_scope = currentScope;
        m_targets.swap(nextTargets);
        m_ground.swap(nextGround);
        m_lastTargets = targets;
        m_lastGround = ground;
    }
    std::string ObservationHandles::targetHandle(TargetIdentity identity) const
    {
        const auto it = m_targets.find(identity);
        if(it == m_targets.end()){ throw ProtocolError("stale_target", "Target is no longer visible"); }
        return it->second;
    }
    std::string ObservationHandles::groundHandle(GroundIdentity identity) const
    {
        const auto it = m_ground.find(identity);
        if(it == m_ground.end()){ throw ProtocolError("stale_target", "Ground observation changed"); }
        return it->second;
    }
    TargetIdentity ObservationHandles::resolveTarget(std::string_view handle, const Json &context) const
    {
        if(!context.is_null() && m_scope == scope(context)){
            for(const auto &[identity, value]: m_targets){ if(value == handle){ return identity; } }
        }
        throw ProtocolError("stale_target", "Target handle expired");
    }
    GroundIdentity ObservationHandles::resolveGround(std::string_view handle, const Json &context) const
    {
        if(!context.is_null() && m_scope == scope(context)){
            for(const auto &[identity, value]: m_ground){ if(value == handle){ return identity; } }
        }
        throw ProtocolError("stale_target", "Ground handle expired");
    }
    Json nearbyEntry(const NearbyEntry &entry, const ObservationHandles &handles)
    {
        if(!validKind(entry.kind) || (entry.kind == "ground" ? (!entry.ground || entry.target) : (!entry.target || entry.ground))){
            throw ProtocolError("unavailable", "Invalid nearby observation");
        }
        Json items = Json::array();
        if(entry.groundItemIds.size() > 100){ throw ProtocolError("unavailable", "Ground item observation exceeds contract limit"); }
        for(const auto item: entry.groundItemIds){
            items.push_back({{"item_id", item}, {"seq_id", nullptr}, {"count", nullptr}, {"name", nullptr}, {"location", "ground"}});
        }
        Json result = {{"target_handle", entry.target ? Json(handles.targetHandle(*entry.target)) : Json(nullptr)},
            {"ground_handle", entry.ground ? Json(handles.groundHandle(*entry.ground)) : Json(nullptr)},
            {"kind", entry.kind}, {"uid", entry.target ? Json(std::to_string(entry.target->uid)) : Json(nullptr)},
            {"name", nullable(entry.name)}, {"position", gridPoint(entry.position)},
            {"hp", nullable(entry.hp)}, {"max_hp", nullable(entry.maxHp)}, {"alive", nullable(entry.alive)}, {"items", std::move(items)}};
        validateDefinition(result, "nearbyObject");
        return result;
    }
    std::vector<NearbyEntry> selectNearby(const std::vector<NearbyEntry> &visible, GridPoint origin, std::string_view kind, int radius)
    {
        if((kind != "all" && !validKind(kind)) || radius < 0 || radius > 40){ throw ProtocolError("invalid_argument", "Invalid nearby filter"); }
        std::vector<NearbyEntry> result;
        for(const auto &entry: visible){
            if((kind == "all" || kind == entry.kind) && distance(origin, entry.position) <= radius){ result.push_back(entry); }
        }
        return result;
    }
    Json inventoryEntry(const InventoryEntry &entry)
    {
        if(entry.reservedCount > entry.count){ throw ProtocolError("unavailable", "Invalid item reservation observation"); }
        Json result = {{"item_id", entry.itemId}, {"seq_id", entry.seqId}, {"count", entry.count}, {"name", nullable(entry.name)},
            {"location", entry.location}, {"slot", nullable(entry.slot)}, {"reserved_count", entry.reservedCount}};
        validateDefinition(result, "inventoryItem");
        return result;
    }

    FrozenPages::FrozenPages(std::string value): m_nonce(nonce(std::move(value))) {}
    void FrozenPages::clear(){ m_pages.clear(); }
    Json FrozenPages::first(SnapshotList kind, const Json &context, const Json &inputFilters, SnapshotStamp stamp,
        std::vector<Json> items, std::vector<std::string> unavailable)
    {
        if(items.size() > maxRows){ throw ProtocolError("unavailable", "Snapshot row limit exceeded"); }
        const auto definition = kind == SnapshotList::Nearby ? "nearbyObject" : "inventoryItem";
        std::set<std::string> missing(unavailable.begin(), unavailable.end());
        for(const auto &item: items){
            validateDefinition(item, definition);
            if(kind == SnapshotList::Inventory){
                if(item.at("name").is_null()){ missing.insert("items[].name"); }
            }
            else if(item.at("kind") == "ground"){
                for(const auto &groundItem: item.at("items")){
                    for(const auto *field: {"seq_id", "count", "name"}){
                        if(groundItem.at(field).is_null()){ missing.insert(std::string("items[].items[].") + field); }
                    }
                }
            }
            else{
                for(const auto *field: {"name", "hp", "max_hp", "alive"}){
                    if(item.at(field).is_null()){ missing.insert(std::string("items[].") + field); }
                }
            }
        }
        // Freeze missing-field metadata for the whole snapshot, just like its rows.
        unavailable.assign(missing.begin(), missing.end());
        PageSet set{"p-" + m_nonce + "-" + std::to_string(aibridge::next(m_sequence)), scope(context),
            filters(kind, inputFilters), stamp, std::move(items), std::move(unavailable)};
        auto result = page(kind, set, 0);
        auto &sets = m_pages[kind];
        sets.push_back(std::move(set));
        if(sets.size() > 2){ sets.pop_front(); }
        return result;
    }
    Json FrozenPages::page(SnapshotList kind, const PageSet &set, size_t offset) const
    {
        auto result = snapshotMetadata(set.stamp, "client_known", set.unavailable);
        result["items"] = Json::array();
        const auto end = std::min(set.items.size(), offset + pageSize);
        for(auto i = offset; i < end; ++i){ result["items"].push_back(set.items[i]); }
        result["next_cursor"] = end < set.items.size() ? Json(set.id + ":" + std::to_string(end)) : Json(nullptr);
        validateDefinition(result, listName(kind));
        return result;
    }
    Json FrozenPages::next(SnapshotList kind, const Json &context, const Json &inputFilters, std::string_view cursor) const
    {
        const auto colon = cursor.find(':');
        const auto sets = m_pages.find(kind);
        if(cursor.size() <= 128 && colon != std::string_view::npos && sets != m_pages.end()){
            const auto text = cursor.substr(colon + 1);
            size_t offset = 0;
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), offset);
            if(!text.empty() && text.front() != '0' && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
                && offset % pageSize == 0){
                for(const auto &set: sets->second){
                    if(set.id == cursor.substr(0, colon) && !context.is_null() && set.scope == scope(context)
                        && set.filters == filters(kind, inputFilters) && offset < set.items.size()){
                        return page(kind, set, offset);
                    }
                }
            }
        }
        throw ProtocolError("stale_cursor", "Frozen snapshot cursor expired or changed");
    }

    Json mapRegion(SnapshotStamp stamp, uint64_t mapUid, int mapWidth, int mapHeight,
        int x, int y, int width, int height, const std::function<TerrainCell(int, int)> &readCell)
    {
        if(width < 1 || width > 33 || height < 1 || height > 33 || !readCell){ throw ProtocolError("invalid_argument", "Invalid map region"); }
        if(x < 0 || y < 0 || mapWidth <= 0 || mapHeight <= 0 || int64_t(x) + width > mapWidth || int64_t(y) + height > mapHeight){
            throw ProtocolError("out_of_bounds", "Region is outside the current map");
        }
        Json cells = Json::array();
        bool unknownDynamic = false;
        for(int row = 0; row < height; ++row){
            for(int col = 0; col < width; ++col){
                const auto cell = readCell(x + col, y + row);
                unknownDynamic |= !cell.dynamicBlocked.has_value();
                cells.push_back({{"x", x + col}, {"y", y + row}, {"walkable", cell.walkable}, {"dynamic_blocked", nullable(cell.dynamicBlocked)}});
            }
        }
        auto result = snapshotMetadata(stamp, "current_map", unknownDynamic ? std::vector<std::string>{"dynamic_blocked"} : std::vector<std::string>{});
        result.update({{"map_uid", std::to_string(mapUid)}, {"x", x}, {"y", y}, {"width", width}, {"height", height}, {"cells", std::move(cells)}});
        validateDefinition(result, "mapRegion");
        return result;
    }
}
