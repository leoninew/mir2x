#pragma once

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>

namespace aibridge
{
    struct Policy
    {
        static Policy parse(std::string_view jsonText);
        static Policy load(const std::filesystem::path &absolutePath);

        bool allowsAction(std::string_view action) const;
        bool allowsPeer(std::uint64_t uid) const;
        bool allowsItem(std::uint32_t itemId) const;

        bool allowPvp = false;
        bool allowNpcDialogue = false;
        std::uint64_t maxGoldPerOperation = 0;
        std::uint64_t maxGoldPerSession = 0;
        std::uint32_t maxItemCountPerOperation = 0;

    private:
        bool m_allActions = false;
        bool m_allPeers = false;
        bool m_allItems = false;
        std::set<std::string, std::less<>> m_actions;
        std::set<std::uint64_t> m_peers;
        std::set<std::uint32_t> m_items;
    };
}
