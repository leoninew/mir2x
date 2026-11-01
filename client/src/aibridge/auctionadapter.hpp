#pragma once
#include <memory>
class Client;
namespace aibridge
{
    class AiBridgeRuntime;
    class AiAuctionAdapter
    {
        public:
            AiAuctionAdapter(Client &, AiBridgeRuntime &);
            ~AiAuctionAdapter();
            void registerTools();
            void observe();
        private:
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };
}
