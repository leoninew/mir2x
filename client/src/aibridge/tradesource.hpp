#pragma once
#include "economy.hpp"
#include <optional>

namespace aibridge
{
    struct TradeSourceStamp
    {
        Json scope;
        uint64_t peer = 0, generation = 0;
    };
    // GUI presence is observation, never authority to mint handles in a new world.
    class TradeSources
    {
        private:
            Json m_scope = nullptr;
            std::optional<TradeSourceStamp> m_trade, m_invitation;
        public:
            bool sync(const Json &context)
            {
                const auto next=context.is_null() ? Json(nullptr) : economyScope(context);
                if(next==m_scope){ return false; }
                m_scope=next; m_trade.reset(); m_invitation.reset(); return true;
            }
            void clear() { m_scope=nullptr; m_trade.reset(); m_invitation.reset(); }
            void started(const Json &context,uint64_t peer,uint64_t generation)
            {
                sync(context);
                if(!m_scope.is_null() && peer && generation){ m_trade=TradeSourceStamp{m_scope,peer,generation}; }
            }
            void invited(const Json &context,uint64_t peer,uint64_t revision)
            {
                sync(context);
                if(!m_scope.is_null() && peer && revision){ m_invitation=TradeSourceStamp{m_scope,peer,revision}; }
            }
            void clearTrade() { m_trade.reset(); }
            void clearInvitation() { m_invitation.reset(); }
            static bool matches(const TradeSourceStamp &stamp,const Json &context,uint64_t peer,uint64_t generation)
            {
                return !context.is_null() && stamp.scope==economyScope(context)
                    && stamp.peer==peer && peer && stamp.generation==generation && generation;
            }
            bool trade(const Json &context,uint64_t peer,uint64_t generation) const
            {
                return m_trade && matches(*m_trade,context,peer,generation);
            }
            bool invitation(const Json &context,uint64_t peer,uint64_t revision) const
            {
                return m_invitation && matches(*m_invitation,context,peer,revision);
            }
    };
}
