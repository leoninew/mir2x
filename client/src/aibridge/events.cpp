#include "events.hpp"
#include <limits>

namespace aibridge
{
    void EventRing::append(uint64_t time, std::string_view kind, const Json &context,
        std::optional<std::string_view> operationId, std::string_view message)
    {
        if(m_sequence == std::numeric_limits<uint64_t>::max()){
            throw ProtocolError("unavailable", "事件序号已耗尽");
        }
        Json event = {{"event_seq", std::to_string(m_sequence + 1)}, {"observed_at", std::to_string(time)},
            {"kind", kind}, {"context", context}, {"operation_id", operationId ? Json(*operationId) : Json(nullptr)},
            {"message", message}};
        validateDefinition(event, "event");
        ++m_sequence;
        if(m_events.size() == capacity){ m_events.pop_front(); }
        m_events.push_back(std::move(event));
    }

    Json EventRing::read(uint64_t after, size_t limit) const
    {
        if(after > m_sequence || limit == 0 || limit > 100){
            throw ProtocolError("invalid_argument", "事件游标或数量无效");
        }
        const auto first = m_sequence - m_events.size() + 1;
        const bool gap = !m_events.empty() && after < first - 1;
        Json result = Json::array();
        uint64_t next = after;
        for(size_t i = 0; i < m_events.size() && result.size() < limit; ++i){
            const auto sequence = first + i;
            if(sequence > after){ result.push_back(m_events[i]); next = sequence; }
        }
        Json response = {{"events", std::move(result)}, {"gap", gap}, {"next_seq", std::to_string(next)}};
        validateDefinition(response, "events");
        return response;
    }
}
