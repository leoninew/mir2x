#pragma once
#include "protocol.hpp"
#include <compare>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aibridge
{
    struct GridPoint
    {
        int x = 0;
        int y = 0;
        auto operator<=>(const GridPoint &) const = default;
    };
    struct SnapshotStamp { uint64_t sequence = 0; uint64_t observedAt = 0; };
    struct CharacterSnapshot
    {
        SnapshotStamp stamp;
        uint64_t characterUid = 0;
        uint64_t mapUid = 0;
        GridPoint position;
        GridPoint actionEndpoint;
        std::optional<std::string> name, mapName, action;
        std::optional<int> direction, hp, maxHp, mp, maxMp, level;
        std::optional<uint64_t> gold;
        std::vector<std::string> knownStates;
    };
    Json stateSnapshot(const CharacterSnapshot &);
    Json snapshotMetadata(SnapshotStamp, std::string_view coverage, const std::vector<std::string> &unavailable = {});
    Json gridPoint(GridPoint);
    bool withinTolerance(GridPoint, GridPoint, int tolerance);

    struct TargetIdentity
    {
        uint64_t uid = 0;
        uint64_t incarnation = 0; // Monotonic object identity, never a reusable address.
        auto operator<=>(const TargetIdentity &) const = default;
    };
    struct GroundIdentity
    {
        GridPoint position;
        uint64_t revision = 0; // Changes on every authoritative update, including same-item replacement.
        auto operator<=>(const GroundIdentity &) const = default;
    };
    class ObservationHandles
    {
        public:
            explicit ObservationHandles(std::string nonce = {});
            // Call after visibility/identity changes, even if no nearby query is being served.
            void sync(const Json &context, const std::vector<TargetIdentity> &visibleTargets,
                const std::vector<GroundIdentity> &visibleGround);
            void clear();
            std::string targetHandle(TargetIdentity) const;
            std::string groundHandle(GroundIdentity) const;
            TargetIdentity resolveTarget(std::string_view handle, const Json &context) const;
            GroundIdentity resolveGround(std::string_view handle, const Json &context) const;
        private:
            std::string m_nonce;
            uint64_t m_sequence = 0;
            Json m_scope = nullptr;
            std::map<TargetIdentity, std::string> m_targets;
            std::map<GroundIdentity, std::string> m_ground;
            std::vector<TargetIdentity> m_lastTargets;
            std::vector<GroundIdentity> m_lastGround;
    };

    struct NearbyEntry
    {
        std::string kind; // player / monster / npc / ground
        GridPoint position;
        std::optional<TargetIdentity> target;
        std::optional<GroundIdentity> ground;
        std::optional<std::string> name;
        std::optional<int> hp, maxHp;
        std::optional<bool> alive;
        std::vector<uint32_t> groundItemIds;
    };
    Json nearbyEntry(const NearbyEntry &, const ObservationHandles &);
    // Input has already passed the real client visibility check. Radius uses grid (Chebyshev) distance.
    std::vector<NearbyEntry> selectNearby(const std::vector<NearbyEntry> &visible,
        GridPoint origin, std::string_view kind, int radius);
    struct InventoryEntry
    {
        uint32_t itemId = 0, seqId = 0, count = 0, reservedCount = 0;
        std::optional<std::string> name;
        std::string location = "inventory";
        std::optional<uint16_t> slot;
    };
    Json inventoryEntry(const InventoryEntry &);

    enum class SnapshotList { Nearby, Inventory };
    class FrozenPages
    {
        public:
            static constexpr size_t pageSize = 100;
            static constexpr size_t maxRows = 8192;
            explicit FrozenPages(std::string nonce = {});
            // Filters exclude cursor; nearby filters are normalized kind/radius.
            Json first(SnapshotList, const Json &context, const Json &filters, SnapshotStamp,
                std::vector<Json> items, std::vector<std::string> unavailable = {});
            Json next(SnapshotList, const Json &context, const Json &filters, std::string_view cursor) const;
            void clear(); // Does not reuse issued cursor identities.
        private:
            struct PageSet
            {
                std::string id;
                Json scope;
                Json filters;
                SnapshotStamp stamp;
                std::vector<Json> items;
                std::vector<std::string> unavailable;
            };
            std::string m_nonce;
            uint64_t m_sequence = 0;
            std::map<SnapshotList, std::deque<PageSet>> m_pages;
            Json page(SnapshotList, const PageSet &, size_t offset) const;
    };

    struct TerrainCell { bool walkable = false; std::optional<bool> dynamicBlocked; };
    Json mapRegion(SnapshotStamp, uint64_t mapUid, int mapWidth, int mapHeight,
        int x, int y, int width, int height, const std::function<TerrainCell(int, int)> &readCell);
}
