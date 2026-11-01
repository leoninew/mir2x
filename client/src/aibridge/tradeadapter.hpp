#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "protocol.hpp"

class Client;
namespace aibridge
{
    class AiBridgeRuntime;
    enum class TradeEvent { Invitation, Started, LocalOffer, PeerOffer, Completed, Closed, Error };
    struct TradeItemSnapshot
    {
        uint32_t itemId = 0, seqId = 0, count = 0;
        std::optional<std::string> name;
    };
    struct TradeOfferSnapshot
    {
        uint64_t revision = 0;
        uint32_t gold = 0;
        std::vector<TradeItemSnapshot> items;
        bool locked = false, confirmed = false, pending = false;
    };
    struct TradePanelSnapshot
    {
        std::string handle;
        uint64_t peerUid = 0;
        TradeOfferSnapshot local, peer;
        Json json() const;
    };
    struct TradeInvitationSnapshot
    {
        std::string handle;
        uint64_t peerUid = 0;
        Json json() const;
    };
    struct TradeInteractionSnapshot
    {
        uint64_t revision = 0;
        std::optional<TradePanelSnapshot> trade;
        std::optional<TradeInvitationSnapshot> invitation;
    };
    class AiTradeAdapter final
    {
        public:
            AiTradeAdapter(Client &, AiBridgeRuntime &);
            ~AiTradeAdapter();
            void registerTools();
            void observe();
            void serverEvent(TradeEvent, uint64_t peerUid, uint32_t error = 0, uint64_t sourceGeneration = 0);
            TradeInteractionSnapshot interaction();
        private:
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };
}
