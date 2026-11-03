#pragma once
#include <memory>
#include <string>
#include <SDL3/SDL.h>
#include "controller.hpp"

class Client;
struct ClientMsgBuf;

namespace aibridge
{
    // Input gate for explicit grants; startup uses a separate one-shot controller gate.
    class ControlGesture
    {
        private:
            int m_button = 0;
            SDL_WindowID m_window = 0;
            std::string m_context;
            static bool contains(SDL_FRect rect, float x, float y) noexcept
            {
                // Widget pixels occupy a half-open rectangle; SDL's float helper includes the far edge.
                return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
            }
        public:
            enum class Result { Pass, Consumed, Enable, Pause };
            static bool manual(const SDL_Event &e)
            {
                return e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_WHEEL
                    || (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)
                    || e.type == SDL_EVENT_TEXT_INPUT || e.type == SDL_EVENT_TEXT_EDITING;
            }
            static bool gameWindowEvent(const SDL_Event &e, SDL_WindowID window)
            {
                if(!window){ return false; }
                switch(e.type){
                    case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP: return e.button.windowID == window;
                    case SDL_EVENT_MOUSE_MOTION: return e.motion.windowID == window;
                    case SDL_EVENT_MOUSE_WHEEL: return e.wheel.windowID == window;
                    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: return e.key.windowID == window;
                    case SDL_EVENT_TEXT_INPUT: return e.text.windowID == window;
                    case SDL_EVENT_TEXT_EDITING: return e.edit.windowID == window;
                    case SDL_EVENT_WINDOW_FOCUS_LOST: case SDL_EVENT_WINDOW_FOCUS_GAINED: return e.window.windowID == window;
                    default: return false;
                }
            }
            void reset() { m_button = 0; m_context.clear(); }
            int pendingButton() const noexcept { return m_button; }
            Result route(const SDL_Event &e, SDL_FRect enable, SDL_FRect pause, const std::string &context)
            {
                if(e.type == SDL_EVENT_WINDOW_FOCUS_LOST){ reset(); }
                if(e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT){
                    m_button = contains(enable, e.button.x, e.button.y) ? 1 : contains(pause, e.button.x, e.button.y) ? 2 : 0;
                    m_window = e.button.windowID;
                    m_context = context;
                    if(m_button){ return Result::Consumed; }
                }
                else if(e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT && m_button){
                    const auto rect = m_button == 1 ? enable : pause;
                    const auto result = m_button == 1 ? Result::Enable : Result::Pause;
                    const bool matched = m_window == e.button.windowID && m_context == context && contains(rect, e.button.x, e.button.y);
                    reset();
                    return matched ? result : Result::Consumed;
                }
                if(manual(e)){ reset(); }
                return Result::Pass;
            }
    };

    class AiGameAdapter;
    class AiNpcAdapter;
    class AiTradeAdapter;
    class AiShopAdapter;
    class AiAuctionAdapter;
    enum class NpcPanelKind;
    enum class TradeEvent;
    class AiBridgeRuntime final
    {
        public:
            explicit AiBridgeRuntime(Client &);
            ~AiBridgeRuntime();
            void start(const AiBridgeTransport::Options &);
            void shutdown() noexcept;
            void observe(const RuntimeState &);
            void poll();
            bool processEvent(const SDL_Event &); // Already scaled exactly once.
            void drawPanel() const;
            AiBridgeController *controller() noexcept;
            AiGameAdapter *game() noexcept;
            AiNpcAdapter *npc() noexcept;
            AiTradeAdapter *trade() noexcept;
            AiShopAdapter *shop() noexcept;
            AiAuctionAdapter *auction() noexcept;
            const Policy *policy() const noexcept;
            void npcServerPanel(NpcPanelKind, uint64_t) noexcept;
            void tradeServerEvent(TradeEvent, uint64_t peerUid, uint32_t error = 0, uint64_t sourceGeneration = 0) noexcept;
            void shopBuyResult(uint64_t npcUid, uint32_t itemId, uint32_t seqId, int error) noexcept;
            // Future domain adapters claim before changing game intentions and release on completion.
            bool claimIntent(std::string_view operationId, std::string_view kind);
            bool setIntentFocus(std::string_view operationId, int focusType, uint64_t uid);
            void releaseIntent(std::string_view operationId);
            // Decoder must only interpret the receipt; it must not capture a Process or send more actions.
            using ReceiptDecoder = std::function<OperationUpdate(uint8_t, const uint8_t *, size_t)>;
            uint64_t sendScoped(const OperationView &, const ClientMsgBuf &, ReceiptDecoder);
        private:
            bool closeOnFault() noexcept;
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };
}
