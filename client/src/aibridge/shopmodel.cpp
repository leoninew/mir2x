#include "shopmodel.hpp"
#include "protocol.hpp"
#include <limits>

namespace aibridge
{
    bool ShopQuery::timedOut(uint64_t now) const noexcept
    {
        return m_pending ? (now < m_started || now - m_started >= 10000) : m_expired;
    }
    ShopQuery::Start ShopQuery::begin(ShopQueryKey key, uint64_t now)
    {
        // A timed-out request still owns its response slot until its unnumbered reply drains.
        if(m_pending){ return !m_abandoned && !timedOut(now) && m_key == key ? Start::Coalesced : Start::Busy; }
        if(m_serial == std::numeric_limits<uint64_t>::max()){ throw ProtocolError("unavailable", "Shop query identity exhausted"); }
        ++m_serial;
        m_key = key; m_started = now;
        m_pending = true; m_abandoned = false; m_accepted = false; m_expired = false;
        return Start::Sent;
    }
    ShopQuery::Reply ShopQuery::receive(uint32_t itemId, uint32_t seqId, int mode, uint64_t now)
    {
        if(!m_pending || itemId != m_key.itemId || seqId != m_key.seqId || mode != m_key.mode){ return Reply::Ignored; }
        m_expired = timedOut(now);
        m_pending = false;
        m_accepted = !m_abandoned && !m_expired;
        return m_accepted ? Reply::Accepted : Reply::Drained;
    }
    bool shopPostcondition(const ShopQuote &quote, uint32_t count,
        const ShopObservation &before, const ShopObservation &after)
    {
        if(!count || !quote.unitCount){ return false; }
        auto expected = before.items;
        uint64_t gold = before.gold;
        if(quote.selling){
            if(before.exactCount < quote.unitCount || after.exactCount != before.exactCount - quote.unitCount
                || gold > UINT64_MAX - quote.gold || expected[quote.itemId] < quote.unitCount){ return false; }
            gold += quote.gold;
            expected[quote.itemId] -= quote.unitCount;
        }
        else{
            // The current server charges costList once per buy request, including packable goods.
            if(gold < quote.gold){ return false; }
            if(quote.seqId && (before.exactCount>UINT64_MAX-count || after.exactCount!=before.exactCount+count)){ return false; }
            gold -= quote.gold;
            for(const auto &material: quote.materials){
                if(expected[material.itemId] < material.count){ return false; }
                expected[material.itemId] -= material.count;
            }
            if(expected[quote.itemId] > UINT64_MAX - count){ return false; }
            expected[quote.itemId] += count;
        }
        std::erase_if(expected, [](const auto &p){ return p.second == 0; });
        auto observed = after.items;
        std::erase_if(observed, [](const auto &p){ return p.second == 0; });
        return gold == after.gold && expected == observed;
    }
}
