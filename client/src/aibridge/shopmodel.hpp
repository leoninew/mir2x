#pragma once
#include <cstdint>
#include <compare>
#include <optional>
#include <map>
#include "economy.hpp"

namespace aibridge
{
    struct ShopQueryKey
    {
        uint64_t npcUid = 0, panelRevision = 0;
        uint32_t itemId = 0, seqId = 0;
        int mode = 0;
        auto operator<=>(const ShopQueryKey &) const = default;
    };
    class ShopQuery
    {
        public:
            enum class Start { Sent, Coalesced, Busy };
            enum class Reply { Accepted, Drained, Ignored };
            Start begin(ShopQueryKey, uint64_t now);
            Reply receive(uint32_t itemId, uint32_t seqId, int mode, uint64_t now);
            void abandon() noexcept { m_abandoned = true; m_accepted = false; }
            bool pending() const noexcept { return m_pending; }
            bool timedOut(uint64_t now) const noexcept;
            uint64_t serial() const noexcept { return m_serial; }
            const ShopQueryKey &key() const noexcept { return m_key; }
            bool accepted() const noexcept { return m_accepted; }
        private:
            ShopQueryKey m_key;
            uint64_t m_serial = 0, m_started = 0;
            bool m_pending = false, m_abandoned = false, m_accepted = false, m_expired = false;
    };
    struct ShopObservation
    {
        uint64_t gold = 0, exactCount = 0;
        std::map<uint32_t, uint64_t> items;
    };
    bool shopPostcondition(const ShopQuote &, uint32_t requestedCount,
        const ShopObservation &before, const ShopObservation &after);
    class ShopBuyReceipt
    {
        private:
            bool m_firstAttempt = false;
            std::optional<int> m_result;
        public:
            void arm(uint64_t attemptsBefore) { m_firstAttempt=attemptsBefore==0; m_result.reset(); }
            bool receive(uint64_t currentAttempts, int result)
            {
                if(!m_firstAttempt || currentAttempts!=1 || m_result){ return false; }
                m_result=result; return true;
            }
            const auto &result() const noexcept { return m_result; }
    };
    class ShopPanelSource
    {
        private:
            Json m_scope = nullptr;
        public:
            // Called only for a real panel message, before issuing queries from that panel.
            bool observe(const Json &context)
            {
                const auto next=economyScope(context);
                const bool changed=next!=m_scope;
                m_scope=next; return changed;
            }
            bool current(const Json &context) const
            {
                return !context.is_null() && m_scope==economyScope(context);
            }
    };
}
