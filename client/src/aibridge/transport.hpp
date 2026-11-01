#pragma once
#include "protocol.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>

namespace aibridge
{
    struct InstanceInfo;
    struct Policy;
    struct BridgeRequest
    {
        uint64_t connectionId;
        Json message;
    };

    // Main-thread-only: never runs a background thread or accesses game objects.
    class AiBridgeTransport
    {
        public:
            struct Options
            {
                std::filesystem::path directory;
                std::filesystem::path policyPath;
                std::string build;
            };
            struct Limits
            {
                static constexpr size_t pendingConnections = 4;
                static constexpr size_t pendingRequests = 64;
                static constexpr size_t outgoingBytes = 2 * 1024 * 1024;
                static constexpr size_t requestsPerSecond = 20;
            };
            AiBridgeTransport();
            ~AiBridgeTransport();
            AiBridgeTransport(const AiBridgeTransport &) = delete;
            AiBridgeTransport &operator=(const AiBridgeTransport &) = delete;
            void start(const Options &);
            void close() noexcept;
            size_t pollBudget(size_t maxHandlers = 16, std::chrono::microseconds budget = std::chrono::milliseconds(2));
            std::vector<BridgeRequest> takeRequests(size_t limit = 8);
            bool sendResponse(uint64_t connectionId, const Json &response);
            // T02 must pause control for every returned disconnected connection.
            std::vector<uint64_t> takeDisconnected();
            const InstanceInfo *instance() const noexcept;
            const Policy *policy() const noexcept;
            bool authenticated() const noexcept;
            size_t queuedRequests() const noexcept;
            size_t queuedOutputBytes() const noexcept;
        private:
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };
}
