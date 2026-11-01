#pragma once
#include "snapshot.hpp"
#include "operation.hpp"

namespace aibridge
{
    // Finite bidirectional search rounds; exhaustion is unavailable, not proof of an unreachable goal.
    inline constexpr size_t maxMoveSearchRounds = 4096;

    struct MoveObservation
    {
        SnapshotStamp stamp;
        Json context;
        GridPoint position;
        GridPoint actionEndpoint;
        bool stopped = false;
        bool alive = false;
        std::optional<GridPoint> lastServerPosition; // Uncorrelated observation, never an operation ack.
    };
    class MoveProgress
    {
        public:
            MoveProgress(GridPoint target, int tolerance, const Json &context,
                GridPoint initialPosition, uint64_t startedMs, uint64_t timeoutMs);
            // Pure observation. Caller stops/releases intent on every terminal result.
            OperationUpdate observe(const MoveObservation &, uint64_t nowMs, bool submitted);
        private:
            GridPoint m_target;
            int m_tolerance;
            Json m_context;
            GridPoint m_lastPosition;
            uint64_t m_startedMs;
            uint64_t m_lastProgressMs;
            uint64_t m_timeoutMs;
            bool m_submitted = false;
            std::optional<OperationUpdate> m_terminal;
    };
}
