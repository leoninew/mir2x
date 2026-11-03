#pragma once
#include "operation.hpp"
#include "policy.hpp"
#include "transport.hpp"
#include <filesystem>
#include <memory>

namespace aibridge
{
    enum class ControlState { Disabled, ReadOnly, Active, Paused };
    struct RuntimeState
    {
        std::string phase = "login";
        std::optional<uint64_t> roleUid;
        std::optional<uint64_t> mapUid;
        bool alive = false;
        bool lifeKnown = true; // Boolean observations are known; client health uses observeLife().

        void observeLife(std::optional<bool> dead, bool confirmedDeath = false) noexcept
        {
            lifeKnown = confirmedDeath || dead.has_value();
            alive = !confirmedDeath && dead == false;
        }
        bool knownAlive() const noexcept { return lifeKnown && alive; }
        bool knownDead() const noexcept { return lifeKnown && !alive; }
        bool operator==(const RuntimeState &) const = default;
    };

    struct ControllerOptions
    {
        std::string instanceId;
        std::string build;
        Policy policy;
        bool enabled = true;
        bool autoEnable = true; // One initial RUN/authentication grant; never an automatic resume.
        std::filesystem::path auditDirectory; // Empty disables disk audit in pure tests.
        std::function<uint64_t()> monotonicMs;
        std::function<uint64_t()> unixMs;
        std::function<std::string()> newSessionId;
    };

    // No SDL, Client, socket or widget ownership. All calls are on the main thread.
    class AiBridgeController
    {
        public:
            using Query = std::function<Json(const Json &params, const Json &context)>;
            using StopHook = std::function<std::vector<std::string>()>;
            explicit AiBridgeController(ControllerOptions);
            ~AiBridgeController();
            AiBridgeController(const AiBridgeController &) = delete;
            AiBridgeController &operator=(const AiBridgeController &) = delete;

            void registerAction(std::string method, ActionAdapter);
            void registerQuery(std::string method, Query);
            void setStopHook(StopHook);
            void observe(const RuntimeState &);
            // Domain observers coalesce changes to at most one notification per frame.
            void notifySnapshot(); // Fixed event/current context; no caller-supplied text.
            void onConnected(uint64_t connectionId);
            void onDisconnected(uint64_t connectionId);
            bool enableByUser(); // Local GUI only; never exposed as an IPC method.
            void disableByUser(); // Keeps authenticated read-only diagnostics available.
            void pauseByUser();
            void manualInput(); // Before forwarding the original SDL event to GUI/game.
            void shutdown();

            Json handleRequest(const BridgeRequest &); // Accepts, never begins an action.
            void checkDeadlines(); // Mandatory each frame, even without ordinary work budget; never begin/tick.
            void advance(); // Double-checks accepted work, then begins/ticks at most one.
            void report(std::string_view operationId, const OperationUpdate &);
            Json session() const;
            Json context() const;
            ControlState controlState() const;
            std::string pauseReason() const;
            std::string faultDiagnostic() const;
            std::optional<std::string> activeOperation() const;
            std::weak_ptr<void> lifetime() const; // Scoped callbacks lock before report().
        private:
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };
}
