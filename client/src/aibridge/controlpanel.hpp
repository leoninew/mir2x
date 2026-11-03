#pragma once
#include "runtime.hpp"
#include "textboard.hpp"
#include "textbutton.hpp"

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
            std::string m_status;
            std::string m_role = "角色：未进入游戏";
            std::string m_reason;
            uint64_t m_noticeUntil = 0;
            TextBoard m_statusText;
            TextBoard m_roleText;
            TextBoard m_reasonText;
            TextButton m_enable;
            TextButton m_disable;
        public:
            AiControlPanel(std::function<AiBridgeController *()>, std::function<bool()> connected);
            SDL_FRect bounds() const;
            SDL_FRect enableBounds() const;
            SDL_FRect disableBounds() const;
            void setRole(std::string text) { m_role = std::move(text); }
            void resetGesture() { m_gesture.reset(); }
            void close(std::string reason);
            void refresh();
            bool processEvent(const SDL_Event &);
            void draw() const;
            const std::string &status() const { return m_status; }
    };
}
