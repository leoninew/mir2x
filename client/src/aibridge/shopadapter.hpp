#pragma once
#include <cstdint>
#include <memory>

class Client;
namespace aibridge
{
    class AiBridgeRuntime;
    enum class NpcPanelKind;
    class AiShopAdapter
    {
        public:
            AiShopAdapter(Client &, AiBridgeRuntime &);
            ~AiShopAdapter();
            void registerTools();
            void observe();
            void buyResult(uint64_t npcUid, uint32_t itemId, uint32_t seqId, int error);
            void panelSource(NpcPanelKind);
        private:
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };
}
