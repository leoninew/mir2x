#pragma once
#include "runtime.hpp"
#include "textboard.hpp"

namespace aibridge
{
    // Shared by the runtime and client_objs GUI tests. Coordinates are renderer-logical.
    class AiControlPanel final
    {
        private:
            std::function<AiBridgeController *()> m_controller;
            std::function<bool()> m_connected;
            ControlGesture m_gesture;
            bool m_closed = false;
            bool m_active = false;
            std::string m_status;
            TextBoard m_statusText;
        public:
            AiControlPanel(std::function<AiBridgeController *()>, std::function<bool()> connected);
            SDL_FRect bounds() const;
            void resetGesture() { m_gesture.reset(); }
            void close(std::string reason);
            void refresh();
            bool processEvent(const SDL_Event &);
            void draw() const;
            const std::string &status() const { return m_status; }
    };
}
