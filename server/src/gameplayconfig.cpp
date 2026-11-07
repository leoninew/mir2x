#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "dbcomid.hpp"
#include "gameplayconfig.hpp"

namespace
{
    using Json = nlohmann::json;

    std::string displayPath(const std::filesystem::path &path)
    {
        const auto utf8 = path.u8string();
        return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
    }

    [[noreturn]] void invalid(const std::string &field, const std::string &reason)
    {
        throw std::runtime_error(field + ": " + reason);
    }

    Json read(const std::filesystem::path &path)
    {
        std::ifstream stream(path, std::ios::binary);
        if(!stream){
            invalid(displayPath(path), "unable to open gameplay configuration");
        }
        std::vector<std::set<std::string>> keys;
        try{
            return Json::parse(stream, [&keys](int, Json::parse_event_t event, Json &value)
            {
                if(event == Json::parse_event_t::object_start){
                    keys.emplace_back();
                }
                else if(event == Json::parse_event_t::key){
                    if(!keys.back().insert(value.get<std::string>()).second){
                        invalid(value.get<std::string>(), "duplicate object key");
                    }
                }
                else if(event == Json::parse_event_t::object_end){
                    keys.pop_back();
                }
                return true;
            });
        }
        catch(const std::exception &e){
            invalid(displayPath(path), e.what());
        }
    }

    void object(const Json &value, const std::string &field, std::initializer_list<const char *> required, std::initializer_list<const char *> optional = {})
    {
        if(!value.is_object()){
            invalid(field, "expected object");
        }
        std::set<std::string> allowed;
        for(const auto key: required){
            allowed.insert(key);
            if(!value.contains(key)){
                invalid(field + "." + key, "required field missing");
            }
        }
        for(const auto key: optional){
            allowed.insert(key);
        }
        for(const auto &[key, unused]: value.items()){
            (void)unused;
            if(!allowed.contains(key)){
                invalid(field + "." + key, "unknown field");
            }
        }
    }

    const Json &array(const Json &value, const std::string &field)
    {
        if(!value.is_array()){
            invalid(field, "expected array");
        }
        return value;
    }

    int64_t integer(const Json &value, const std::string &field, int64_t low, int64_t high)
    {
        if(!value.is_number_integer()){
            invalid(field, "expected integer");
        }
        if(value.is_number_unsigned() && value.get<uint64_t>() > static_cast<uint64_t>(high)){
            invalid(field, "integer out of range");
        }
        const auto result = value.get<int64_t>();
        if(result < low || result > high){
            invalid(field, "integer out of range");
        }
        return result;
    }

    int number(const Json &value, const std::string &field, int low = 0)
    {
        return static_cast<int>(integer(value, field, low, std::numeric_limits<int>::max()));
    }

    uint32_t identity(const Json &value, const std::string &field)
    {
        return static_cast<uint32_t>(integer(value, field, 1, std::numeric_limits<uint32_t>::max()));
    }

    void label(const Json &value, const char8_t *expected, const std::string &field)
    {
        if(!expected || !value.is_string() || value.get<std::string>() != reinterpret_cast<const char *>(expected)){
            invalid(field, "identity/name does not match compiled catalog");
        }
    }

    void version(const Json &doc, const std::string &field)
    {
        integer(doc.at("schema_version"), field + ".schema_version", 1, 1);
    }

    template<typename Record> void checkCatalog(const Json &values, uint32_t endID, Record record, const std::string &field)
    {
        std::set<uint32_t> seen;
        for(const auto &row: array(values, field)){
            object(row, field, {"id", "name"});
            const auto id = identity(row.at("id"), field + ".id");
            if(!record(id) || !seen.insert(id).second){
                invalid(field, "unknown or duplicate identity");
            }
            label(row.at("name"), record(id).name, field + ".name");
        }
        for(uint32_t id = 1; id < endID; ++id){
            if(record(id) && !seen.contains(id)){
                invalid(field, "catalog is incomplete");
            }
        }
    }
}

GameplayConfig GameplayConfig::load(const std::filesystem::path &root)
{
    GameplayConfig result;
    if(std::filesystem::exists(root / "multipliers.json")){
        const auto doc = read(root / "multipliers.json");
        object(doc, "multipliers.json", {"schema_version", "experience", "gold", "drop"});
        version(doc, "multipliers.json");
        const auto multiplier = [&doc](const char *key)
        {
            const auto &value = doc.at(key);
            if(!value.is_number() || !std::isfinite(value.get<double>()) || value.get<double>() < 0.0){
                invalid(std::string("multipliers.json.") + key, "expected finite nonnegative number");
            }
            return value.get<double>();
        };
        result.m_multipliers = {multiplier("experience"), multiplier("gold"), multiplier("drop")};
    }
    const auto catalog = read(root / "catalog.json");
    object(catalog, "catalog", {"schema_version", "monsters", "items", "maps"});
    version(catalog, "catalog");
    checkCatalog(catalog.at("monsters"), DBCOM_MONSTERENDID(), [](uint32_t id) -> const auto & { return DBCOM_MONSTERRECORD(id); }, "catalog.monsters");
    checkCatalog(catalog.at("items"), DBCOM_ITEMENDID(), [](uint32_t id) -> const auto & { return DBCOM_ITEMRECORD(id); }, "catalog.items");
    checkCatalog(catalog.at("maps"), DBCOM_MAPENDID(), [](uint32_t id) -> const auto & { return DBCOM_MAPRECORD(id); }, "catalog.maps");

    const auto monsters = read(root / "monsters.json");
    object(monsters, "monsters.json", {"schema_version", "monsters"});
    version(monsters, "monsters.json");
    for(const auto &row: array(monsters.at("monsters"), "monsters.json.monsters")){
        object(row, "monsters[]", {"monster_id", "monster_name", "exp"});
        const auto id = identity(row.at("monster_id"), "monsters[].monster_id");
        label(row.at("monster_name"), DBCOM_MONSTERRECORD(id).name, "monsters[].monster_name");
        if(!result.m_exp.emplace(id, number(row.at("exp"), "monsters[].exp")).second){
            invalid("monsters[]", "duplicate identity");
        }
    }
    for(uint32_t id = 1; id < DBCOM_MONSTERENDID(); ++id){
        if(DBCOM_MONSTERRECORD(id) && !result.m_exp.contains(id)){
            invalid("monsters.json", "missing monster experience");
        }
    }

    const auto manifest = read(root / "manifest.json");
    object(manifest, "manifest.json", {"schema_version", "drop_monsters", "spawn_maps"});
    version(manifest, "manifest.json");
    std::set<uint32_t> dropIDs;
    for(const auto &value: array(manifest.at("drop_monsters"), "manifest.drop_monsters")){
        const auto id = identity(value, "manifest.drop_monsters[]");
        if(!DBCOM_MONSTERRECORD(id) || !dropIDs.insert(id).second){
            invalid("manifest.drop_monsters", "unknown or duplicate identity");
        }
        const auto path = root / "drops" / (std::to_string(id) + ".json");
        const auto doc = read(path);
        const auto field = displayPath(path);
        object(doc, field, {"schema_version", "monster_id", "monster_name", "items"});
        version(doc, field);
        if(identity(doc.at("monster_id"), field + ".monster_id") != id){
            invalid(field, "file identity differs from manifest");
        }
        label(doc.at("monster_name"), DBCOM_MONSTERRECORD(id).name, field + ".monster_name");
        auto &groups = result.m_drops[id];
        for(const auto &row: array(doc.at("items"), field + ".items")){
            object(row, field + ".items[]", {"item_id", "item_name", "group", "prob_recip", "repeat", "count"});
            const auto itemID = identity(row.at("item_id"), field + ".items[].item_id");
            label(row.at("item_name"), DBCOM_ITEMRECORD(itemID).name, field + ".items[].item_name");
            const auto group = number(row.at("group"), field + ".items[].group");
            const auto repeat = number(row.at("repeat"), field + ".items[].repeat", 1);
            const DropItemConfig item
            {
                .itemID = itemID,
                .probRecip = number(row.at("prob_recip"), field + ".items[].prob_recip", 1),
                .count = number(row.at("count"), field + ".items[].count", 1),
            };
            groups[group].insert(groups[group].end(), static_cast<size_t>(repeat), item);
        }
    }

    std::set<uint32_t> mapIDs;
    for(const auto &value: array(manifest.at("spawn_maps"), "manifest.spawn_maps")){
        const auto id = identity(value, "manifest.spawn_maps[]");
        if(!DBCOM_MAPRECORD(id) || !mapIDs.insert(id).second){
            invalid("manifest.spawn_maps", "unknown or duplicate identity");
        }
        const auto path = root / "spawns" / (std::to_string(id) + ".json");
        const auto doc = read(path);
        const auto field = displayPath(path);
        object(doc, field, {"schema_version", "map_id", "map_name", "groups"});
        version(doc, field);
        if(identity(doc.at("map_id"), field + ".map_id") != id){
            invalid(field, "file identity differs from manifest");
        }
        label(doc.at("map_name"), DBCOM_MAPRECORD(id).name, field + ".map_name");
        for(const auto &group: array(doc.at("groups"), field + ".groups")){
            object(group, field + ".groups[]", {"monster_id", "monster_name", "loc"});
            const auto monsterID = identity(group.at("monster_id"), field + ".groups[].monster_id");
            label(group.at("monster_name"), DBCOM_MONSTERRECORD(monsterID).name, field + ".groups[].monster_name");
            SpawnGroup entry{monsterID, group.at("monster_name").get<std::string>(), {}};
            for(const auto &row: array(group.at("loc"), field + ".groups[].loc")){
                object(row, field + ".loc[]", {"x", "y", "w", "h", "count", "time"}, {"cratio"});
                entry.loc.push_back(SpawnRegion
                {
                    .x = number(row.at("x"), field + ".x", std::numeric_limits<int>::min()),
                    .y = number(row.at("y"), field + ".y", std::numeric_limits<int>::min()),
                    .w = number(row.at("w"), field + ".w", 1),
                    .h = number(row.at("h"), field + ".h", 1),
                    .count = number(row.at("count"), field + ".count"),
                    .time = number(row.at("time"), field + ".time", 1),
                    .cratio = row.contains("cratio") ? std::optional<int>(number(row.at("cratio"), field + ".cratio", std::numeric_limits<int>::min())) : std::nullopt,
                });
            }
            result.m_spawns[id].push_back(std::move(entry));
        }
    }
    return result;
}

int GameplayConfig::monsterExp(uint32_t id) const
{
    if(const auto p = m_exp.find(id); p != m_exp.end()){
        return p->second;
    }
    invalid("monsterExp", "unknown monster identity");
}

const std::map<int, std::vector<DropItemConfig>> &GameplayConfig::monsterDropTable(uint32_t id) const
{
    if(!DBCOM_MONSTERRECORD(id)){
        invalid("monsterDropTable", "unknown monster identity");
    }
    if(const auto p = m_drops.find(id); p != m_drops.end()){
        return p->second;
    }
    static const std::map<int, std::vector<DropItemConfig>> empty;
    return empty;
}

const std::vector<SpawnGroup> &GameplayConfig::mapSpawns(uint32_t id) const
{
    if(!DBCOM_MAPRECORD(id)){
        invalid("mapSpawns", "unknown map identity");
    }
    if(const auto p = m_spawns.find(id); p != m_spawns.end()){
        return p->second;
    }
    static const std::vector<SpawnGroup> empty;
    return empty;
}
