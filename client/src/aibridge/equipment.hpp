#pragma once
#include <algorithm>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include "dbcomid.hpp"
#include "invpack.hpp"
#include "protocol.hpp"

namespace aibridge
{
    enum class EquipmentOrigin { Manual, Ai };
    struct EquipmentSelection
    {
        uint32_t itemId = 0, seqId = 0;
        int slot = -1;
        bool operator==(const EquipmentSelection &) const = default;
    };

    inline std::optional<int> inventoryWearSlot(uint32_t itemId, const SDWear &wear)
    {
        const auto &record = DBCOM_ITEMRECORD(itemId);
        if(!record){ return {}; }
        if(record.wearable(WLG_RING0)){ return wear.getWLItem(WLG_RING0) ? WLG_RING1 : WLG_RING0; }
        if(record.wearable(WLG_ARMRING0)){ return wear.getWLItem(WLG_ARMRING0) ? WLG_ARMRING1 : WLG_ARMRING0; }
        for(const int slot: {WLG_HELMET, WLG_WEAPON, WLG_DRESS, WLG_SHOES, WLG_NECKLACE}){
            if(record.wearable(slot)){ return slot; }
        }
        return {};
    }

    struct EquipmentAccess
    {
        const InvPack *inventory = nullptr;
        const SDWear *wear = nullptr;
        bool inventoryKnown = false, wearKnown = false, busy = false, equipmentPending = false;
        std::function<bool(uint32_t, uint32_t)> reserved;
        std::function<bool(uint32_t, int)> canWear;
    };

    inline EquipmentSelection prepareInventoryWear(const EquipmentAccess &access, uint32_t itemId, uint32_t seqId, uint32_t count,
        EquipmentOrigin origin = EquipmentOrigin::Manual)
    {
        if(!itemId || !seqId || count != 1){ throw ProtocolError("invalid_argument", "Equipment requires one exact item instance"); }
        if(!access.inventory || !access.wear || !access.inventoryKnown || !access.wearKnown || !access.canWear || !access.reserved){
            throw ProtocolError("unavailable", "Inventory or equipment state is not known");
        }
        if(access.busy || (origin == EquipmentOrigin::Ai && access.equipmentPending)){
            throw ProtocolError("busy", "An equipment request or modal interaction is active");
        }
        const auto slot = inventoryWearSlot(itemId, *access.wear);
        if(!slot){ throw ProtocolError("unsupported_item_type", "Item has no ordinary inventory wear path"); }
        const auto matches = [=](const SDItem &item){ return item.itemID == itemId && item.seqID == seqId; };
        if(matches(access.inventory->getGrabbedItem()) || access.reserved(itemId, seqId)){
            throw ProtocolError("busy", "Item identity is reserved by a GUI interaction");
        }
        const auto &bins = access.inventory->getPackBinList();
        if(bins.size() > 8192){ throw ProtocolError("unavailable", "Inventory scan limit exceeded"); }
        size_t matchesFound = 0;
        for(const auto &bin: bins){
            if(matches(bin.item)){
                if(!bin.item || bin.item.count != 1){ throw ProtocolError("invalid_argument", "Invalid equipment inventory instance"); }
                ++matchesFound;
            }
        }
        if(matchesFound != 1){ throw ProtocolError("invalid_argument", "Equipment instance is not available in inventory"); }
        for(int current = WLG_BEGIN; current < WLG_END; ++current){
            if(matches(access.wear->getWLItem(current))){ throw ProtocolError("invalid_argument", "Item is already equipped"); }
        }
        if(!access.canWear(itemId, *slot)){ throw ProtocolError("invalid_argument", "Character cannot wear this equipment"); }
        return {itemId, seqId, *slot};
    }

    inline bool submitInventoryWear(const EquipmentAccess &access, const EquipmentSelection &selection,
        EquipmentOrigin origin, const std::function<bool()> &guard, const std::function<bool(const EquipmentSelection &)> &send)
    {
        if(prepareInventoryWear(access, selection.itemId, selection.seqId, 1, origin) != selection){
            throw ProtocolError("stale_target", "Ordinary equipment slot selection changed");
        }
        if(!guard || !guard() || !send){ return false; }
        return send(selection);
    }

    class EquipmentRequests
    {
        private:
            struct Attempt
            {
                EquipmentSelection selection;
                EquipmentOrigin origin;
                uint64_t sequence;
            };
            uint64_t m_sequence = 0, m_aiSequence = 0, m_observed = 0, m_rejected = 0;
            std::optional<EquipmentSelection> m_aiSelection, m_lastObserved;
            std::deque<Attempt> m_pending;
            bool m_aiReceiptEligible = false, m_overflow = false;
        public:
            bool pending() const noexcept { return m_overflow || !m_pending.empty(); }
            uint64_t sequence() const noexcept { return m_sequence; }
            bool submit(const EquipmentSelection &selection, EquipmentOrigin origin, const std::function<bool()> &send)
            {
                if(origin == EquipmentOrigin::Ai && (pending() || m_sequence == std::numeric_limits<uint64_t>::max())){ return false; }
                if(!send || !send()){ return false; }
                if(origin == EquipmentOrigin::Manual){ m_aiReceiptEligible = false; }
                if(m_sequence == std::numeric_limits<uint64_t>::max()){
                    m_overflow = true;
                    return true;
                }
                ++m_sequence;
                if(origin == EquipmentOrigin::Ai){
                    m_aiSequence = m_sequence;
                    m_aiSelection = selection;
                    m_aiReceiptEligible = true;
                }
                if(m_pending.size() == 128){
                    m_overflow = true;
                    m_aiReceiptEligible = false;
                }
                else{ m_pending.push_back({selection, origin, m_sequence}); }
                return true;
            }
            void observe(int slot, const SDItem &item)
            {
                if(!item || item.count != 1){ return; }
                const EquipmentSelection selection{item.itemID, item.seqID, slot};
                const auto found = std::ranges::find_if(m_pending, [&](const auto &attempt){ return attempt.selection == selection; });
                if(found == m_pending.end()){ return; }
                if(found->origin == EquipmentOrigin::Ai && found->sequence == m_aiSequence && m_aiReceiptEligible){
                    m_observed = found->sequence;
                    m_lastObserved = selection;
                }
                m_pending.erase(found);
            }
            void reject(uint32_t itemId, uint32_t seqId)
            {
                const auto found = std::ranges::find_if(m_pending, [=](const auto &attempt)
                {
                    return attempt.selection.itemId == itemId && attempt.selection.seqId == seqId;
                });
                if(found == m_pending.end()){ return; }
                if(found->origin == EquipmentOrigin::Ai && found->sequence == m_aiSequence && m_aiReceiptEligible){
                    m_rejected = found->sequence;
                }
                m_pending.erase(found);
            }
            bool submitted(uint64_t sequence, const EquipmentSelection &selection) const
            {
                return sequence && sequence == m_aiSequence && m_aiSelection == selection;
            }
            bool rejected(uint64_t sequence) const noexcept { return m_aiReceiptEligible && sequence && m_rejected == sequence; }
            bool observed(uint64_t sequence, const EquipmentSelection &selection, const SDWear &wear) const
            {
                if(!m_aiReceiptEligible || !sequence || sequence != m_observed || m_lastObserved != selection){ return false; }
                const auto &item = wear.getWLItem(selection.slot);
                return item && item.count == 1 && item.itemID == selection.itemId && item.seqID == selection.seqId;
            }
    };
}
