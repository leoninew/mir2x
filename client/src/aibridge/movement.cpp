#include "movement.hpp"

namespace aibridge
{
    MoveProgress::MoveProgress(GridPoint target, int tolerance, const Json &context,
        GridPoint initialPosition, uint64_t startedMs, uint64_t timeoutMs)
        : m_target(target), m_tolerance(tolerance), m_context(context), m_lastPosition(initialPosition),
          m_startedMs(startedMs), m_lastProgressMs(startedMs), m_timeoutMs(timeoutMs)
    {
        gridPoint(target); gridPoint(initialPosition); withinTolerance(target, target, tolerance);
        validateDefinition(context, "context");
        if(!timeoutMs || timeoutMs > 30000){ throw ProtocolError("invalid_argument", "Movement timeout must be at most 30 seconds"); }
    }
    OperationUpdate MoveProgress::observe(const MoveObservation &s, uint64_t now, bool submitted)
    {
        if(m_terminal){ return *m_terminal; }
        m_submitted |= submitted;
        submitted = m_submitted;
        OperationUpdate result;
        result.submitted = submitted;
        result.result = {{"position", gridPoint(s.position)}, {"submitted", submitted}};
        std::string description = "客户端观测格位置（" + std::to_string(s.position.x) + "," + std::to_string(s.position.y)
            + "），动作终点（" + std::to_string(s.actionEndpoint.x) + "," + std::to_string(s.actionEndpoint.y) + "）";
        if(s.lastServerPosition){
            description += "；最近服务端位置（非本操作回执）：（" + std::to_string(s.lastServerPosition->x) + "," + std::to_string(s.lastServerPosition->y) + "）";
        }
        result.evidence = Json::array({{{"source", "client"}, {"kind", "position"},
            {"observed_at", std::to_string(s.stamp.observedAt)}, {"snapshot_seq", std::to_string(s.stamp.sequence)},
            {"server_code", nullptr}, {"description", description}}});
        const auto fail = [&](std::string code)
        {
            result.outcome = OperationOutcome::Failed;
            result.error = OperationError{std::move(code), "移动未达到停止后的目标条件", Json::object()};
        };
        if(s.context != m_context){
            result.outcome = submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled;
            result.error = OperationError{"stale_context", "移动所属角色、地图或控制代次已变化", Json::object()};
        }
        else if(!s.alive){
            result.outcome = submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled;
            result.error = OperationError{"paused", "角色当前不处于存活状态", Json::object()};
        }
        else if(now < m_startedMs || now < m_lastProgressMs){ fail("unavailable"); }
        else if(now - m_startedMs >= m_timeoutMs){
            result.outcome = submitted ? OperationOutcome::Unknown : OperationOutcome::Failed;
            result.error = OperationError{"timeout", "移动已超过执行期限", Json::object()};
        }
        else if(withinTolerance(s.position, m_target, m_tolerance) && s.stopped){
            result.outcome = OperationOutcome::Succeeded;
            result.completionScope = "observed_postcondition";
        }
        else{
            if(s.position != m_lastPosition){ m_lastPosition = s.position; m_lastProgressMs = now; }
            if(now - m_lastProgressMs >= 3000){ fail("stalled"); }
        }
        if(result.outcome != OperationOutcome::Running){ m_terminal = result; }
        return result;
    }
}
