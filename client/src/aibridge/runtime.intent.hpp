#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace aibridge
{
    // Motion callbacks hold a stamp, not an authorization that can be renewed later.
    class IntentOwnership
    {
        private:
            uint64_t m_generation = 0;
            bool m_owned = false;
            bool m_used = false;
            void next()
            {
                if(m_generation == std::numeric_limits<uint64_t>::max()){
                    throw std::overflow_error("AI intention generation exhausted");
                }
                ++m_generation;
            }
        public:
            struct Stamp { uint64_t generation; bool permitted; };
            void begin()
            {
                next();
                m_used = true;
                m_owned = true;
            }
            bool cancel()
            {
                if(!m_owned){ return false; }
                next();
                m_owned = false;
                return true;
            }
            Stamp stamp(bool parsingLocalAction, bool matchingHumanIntent = false) const noexcept
            {
                return {m_generation, !m_used || parsingLocalAction || (!m_owned && matchingHumanIntent)};
            }
            bool permits(Stamp stamp) const noexcept
            {
                return stamp.permitted && stamp.generation == m_generation;
            }
            bool owned() const noexcept { return m_owned; }
    };
}
