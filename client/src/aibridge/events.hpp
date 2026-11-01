#pragma once
#include "protocol.hpp"
#include <deque>
#include <optional>

namespace aibridge
{
    class EventRing
    {
        public:
            static constexpr size_t capacity = 1024;
            void append(uint64_t observedAt, std::string_view kind, const Json &context,
                std::optional<std::string_view> operationId, std::string_view message);
            Json read(uint64_t after, size_t limit) const;
            uint64_t sequence() const noexcept { return m_sequence; }
        private:
            uint64_t m_sequence = 0;
            std::deque<Json> m_events;
    };
}
