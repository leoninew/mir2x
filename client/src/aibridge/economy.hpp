#pragma once
#include "operation.hpp"
#include <deque>
#include <memory>

namespace aibridge
{
    inline constexpr uint64_t economyHandleLifetimeMs = 30000;
    struct QuoteMaterial { uint32_t itemId = 0, count = 0; };
    struct ShopQuote
    {
        uint32_t itemId = 0, seqId = 0, unitCount = 1;
        uint64_t gold = 0;
        std::vector<QuoteMaterial> materials;
        bool selling = false;
    };
    struct QuoteRecord
    {
        std::string handle, panelHandle;
        Json contextScope;
        uint64_t sourceRevision = 0, createdMonotonic = 0, expiresAt = 0;
        ShopQuote value;
    };
    class QuoteStore
    {
        public:
            static constexpr size_t capacity = 256;
            explicit QuoteStore(std::string nonce = {});
            QuoteRecord publish(ShopQuote, const Json &context, std::string panelHandle,
                uint64_t sourceRevision, uint64_t monotonicMs, uint64_t unixMs);
            const QuoteRecord &resolve(std::string_view handle, const Json &context,
                std::string_view panelHandle, uint64_t sourceRevision, uint64_t monotonicMs) const;
            void clear();
        private:
            std::string m_nonce;
            uint64_t m_sequence = 0;
            std::deque<QuoteRecord> m_records;
    };
    Json quoteJson(const QuoteRecord &);
    Json economyScope(const Json &);
    std::string economyNonce();
    enum class EconomicReceipt { Pending, Ok, Rejected, Unknown };
    // Shared by the real GUI request and the adapter. It contains no Process pointer.
    struct EconomicRequest
    {
        bool submitted = false;
        EconomicReceipt receipt = EconomicReceipt::Pending;
        uint32_t error = 0;
    };
    // A timed-out uncorrelated query key is quarantined until a new Process.
    struct EconomyRefresh
    {
        bool pending = false, quarantined = false, known = false;
        uint64_t started = 0, revision = 0;
        std::optional<OperationError> error;
        bool begin(uint64_t now);
        bool receive(uint64_t now);
        void expire(uint64_t now);
    };
    OperationUpdate economicUpdate(const EconomicRequest &, bool deliveryPending = false);
    std::optional<OperationError> validateEconomicSend(const OperationView &, const Json &currentContext,
        bool currentActiveOperation, uint64_t monotonicMs);
    enum class TradeReceipt { Pending, Started, OfferAccepted, Locked, Complete, Closed, UnrelatedError };
    OperationUpdate tradeUpdate(std::string_view method, TradeReceipt, bool submitted, uint32_t error = 0);
    struct EconomicHandle
    {
        std::string handle;
        Json scope, value;
        uint64_t revision = 0, created = 0;
    };
    class EconomicHandles
    {
        public:
            EconomicHandles(): m_nonce(economyNonce()) {}
            std::string publish(Json value, const Json &context, uint64_t revision, uint64_t now);
            const EconomicHandle &resolve(std::string_view, const Json &, uint64_t now) const;
            void clear() { m_entries.clear(); }
        private:
            std::string m_nonce;
            uint64_t m_sequence = 0;
            std::deque<EconomicHandle> m_entries;
    };
    class EconomicPages
    {
        public:
            EconomicPages(): m_nonce(economyNonce()) {}
            Json first(std::string kind, const Json &context, const Json &filters, Json metadata, std::vector<Json> rows);
            Json next(std::string_view kind, const Json &context, const Json &filters, std::string_view cursor) const;
            void clear(){ m_pages.clear(); }
        private:
            struct Page { std::string kind, id; Json scope, filters, metadata; std::vector<Json> rows; };
            std::string m_nonce;
            uint64_t m_sequence = 0;
            std::deque<Page> m_pages;
            Json page(const Page &, size_t) const;
    };
    struct DeliveryText { std::string record, text; };
    // Only the trusted wire peer is accepted; XML cannot confer system identity.
    std::vector<DeliveryText> systemDeliveries(bool trustedSystemPeer, std::string_view xml);
}
