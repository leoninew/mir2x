#pragma once
#include "operation.hpp"
#include "snapshot.hpp"
#include "policy.hpp"

namespace aibridge
{
    // Caller supplies every known player affected by the real damage path, not just its primary target.
    std::optional<OperationError> validateCombatPeers(const Policy &, const std::vector<uint64_t> &affectedUids);
    std::optional<OperationError> validateCastTarget(std::string_view actionType, bool beneficial,
        bool hasTarget, bool hasGrid, bool targetIsSelf = false);
    enum class ActionKind { PickUp, UseItem, AttackOnce, AttackUntilDead, Cast, Toggle };
    struct ItemChange
    {
        uint32_t itemId = 0, seqId = 0, before = 0, after = 0;
    };
    struct ActionObservation
    {
        SnapshotStamp stamp;
        Json context;
        bool alive = true;
        bool submitted = false; // Actual send, never queue acceptance.
        bool actionStarted = false, actionFinished = false;
        bool targetVisible = true, targetDead = false;
        std::optional<bool> toggleEnabled;
        std::vector<ItemChange> itemChanges;
        std::optional<uint64_t> goldBefore, goldAfter;
        // Uncorrelated inventory/gold/ground changes are evidence only.
        bool correlatedReceipt = false;
        std::optional<OperationError> rejection;
    };
    class ActionProgress
    {
        public:
            ActionProgress(ActionKind, const Json &context, uint64_t startedMs, uint64_t timeoutMs,
                uint32_t itemId = 0, uint32_t seqId = 0, uint32_t count = 1);
            OperationUpdate observe(const ActionObservation &, uint64_t nowMs);
            OperationUpdate cancel(bool submitted);
        private:
            ActionKind m_kind;
            Json m_context;
            uint64_t m_startedMs, m_timeoutMs;
            uint32_t m_itemId, m_seqId, m_count;
            bool m_submitted = false, m_actionStarted = false;
            std::optional<OperationUpdate> m_terminal;
    };
}
