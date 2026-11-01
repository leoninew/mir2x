#pragma once
#include <memory>
#include "npcmodel.hpp"

class Client;
namespace aibridge
{
    class AiBridgeRuntime;
    class AiNpcAdapter
    {
        public:
            AiNpcAdapter(Client &, AiBridgeRuntime &);
            ~AiNpcAdapter();
            void registerTools();
            void observe();
            void serverPanel(NpcPanelKind, uint64_t npcUid);
            NpcPanelIdentity resolvePanel(NpcPanelKind, std::string_view handle, const Json &context);
            std::string panelHandle(NpcPanelKind);
        private:
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };
}
