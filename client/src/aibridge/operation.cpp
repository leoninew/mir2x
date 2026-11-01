#include "operation.hpp"

namespace aibridge
{
    Json OperationError::json() const
    {
        Json value = {{"code", code}, {"message", message}, {"details", details}};
        validateDefinition(value, "error");
        return value;
    }

    bool OperationRecord::terminal() const
    {
        return state != "accepted" && state != "running";
    }

    Json OperationRecord::json() const
    {
        Json value = {
            {"operation_id", view.operationId}, {"request_id", requestId}, {"method", view.method},
            {"state", state}, {"context", view.context}, {"started_at", std::to_string(startedAt)},
            {"deadline_at", std::to_string(deadlineAt)},
            {"completion_scope", completionScope ? Json(*completionScope) : Json(nullptr)},
            {"evidence", evidence}, {"error", error ? error->json() : Json(nullptr)},
            {"cancel_requested", cancelRequested}, {"side_effects_possible", sideEffectsPossible},
            {"late_resolution", lateResolution}, {"result", result},
        };
        validateDefinition(value, "operation");
        return value;
    }
}
