#pragma once
#include "operation.hpp"
#include "snapshot.hpp"
#include <array>

namespace aibridge
{
    enum class NpcPanelKind { Dialogue, Input, Shop, InvOp };
    struct NpcOption
    {
        int paragraph = 0;
        int leaf = 0;
        std::string text, path, event;
        std::optional<std::string> args;
        bool autoClose = false;
        auto operator<=>(const NpcOption &) const = default;
    };
    struct NpcDialogue
    {
        uint64_t revision = 0, npcUid = 0;
        std::string npcHandle, text;
        std::vector<NpcOption> options;
        auto operator<=>(const NpcDialogue &) const = default;
    };
    struct NpcInput
    {
        uint64_t revision = 0, npcUid = 0;
        std::string prompt;
        uint32_t maxBytes = 199;
        bool sensitive = true;
        auto operator<=>(const NpcInput &) const = default;
    };
    struct NpcPanelIdentity
    {
        uint64_t revision = 0, npcUid = 0;
        auto operator<=>(const NpcPanelIdentity &) const = default;
    };
    class NpcModel
    {
        public:
            void sync(const Json &context, std::optional<NpcDialogue>, std::optional<NpcInput>,
                std::optional<NpcPanelIdentity> shop = {}, std::optional<NpcPanelIdentity> invop = {});
            void clear();
            uint64_t revision() const noexcept { return m_revision; }
            Json snapshot(SnapshotStamp, bool auctionOpen = false) const;
            const NpcDialogue &dialogue(std::string_view id, const Json &context) const;
            NpcOption option(std::string_view id, std::string_view handle, const Json &context) const;
            const NpcInput &input(std::string_view handle, const Json &context) const;
            NpcPanelIdentity panel(NpcPanelKind, std::string_view handle, const Json &context) const;
            std::string handle(NpcPanelKind) const;
        private:
            Json m_scope = nullptr;
            uint64_t m_revision = 0;
            std::optional<NpcDialogue> m_dialogue;
            std::optional<NpcInput> m_input;
            std::optional<NpcPanelIdentity> m_shop, m_invop;
            std::array<std::string, 4> m_handles;
            std::string issue(NpcPanelKind);
            void checkContext(const Json &) const;
    };
    bool npcEventFits(const NpcOption &) noexcept;
    bool npcInputFits(std::string_view text, size_t maxBytes = 199) noexcept;
    OperationUpdate npcObserved(SnapshotStamp, bool submitted, bool localClose,
        std::string_view description, Json result = Json::object());
    OperationUpdate observeNpcReply(SnapshotStamp, const std::function<Json()> &readPanel);
}
