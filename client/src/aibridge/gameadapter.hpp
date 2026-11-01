#pragma once
#include <memory>
#include "snapshot.hpp"

class Client;
namespace aibridge
{
    class AiBridgeRuntime;
    class AiGameAdapter
    {
        public:
            AiGameAdapter(Client &, AiBridgeRuntime &);
            ~AiGameAdapter();
            void registerTools();
            void beginPoll() noexcept;
            void observe();
            void markChanged() noexcept;
            TargetIdentity resolveTarget(std::string_view, const Json &) const;
            std::string targetHandle(uint64_t, const Json &) const;
            GroundIdentity resolveGround(std::string_view, const Json &) const;
        private:
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };
}
