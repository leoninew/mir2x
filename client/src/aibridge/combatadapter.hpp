#pragma once
#include <memory>

class Client;
namespace aibridge
{
    class AiBridgeRuntime;
    class AiCombatAdapter
    {
        public:
            AiCombatAdapter(Client &, AiBridgeRuntime &);
            ~AiCombatAdapter();
            void registerTools();
        private:
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };
}
