#pragma once
#include "protocol.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace aibridge
{
    struct OperationError
    {
        std::string code;
        std::string message;
        Json details = Json::object();
        Json json() const;
    };

    struct BudgetRequest
    {
        bool economic = false;
        uint64_t gold = 0;
        uint32_t itemCount = 0;
        std::vector<uint32_t> itemIds;
        std::optional<uint64_t> peerUid;
        bool pvp = false;
    };

    struct ValidationResult
    {
        std::optional<OperationError> error;
        BudgetRequest budget;
    };

    struct OperationView
    {
        std::string operationId;
        std::string method;
        Json params;
        Json context;
        uint64_t deadlineMs = 0; // Monotonic clock, not wire timestamp.
        bool submitted = false;
    };

    enum class OperationOutcome { Running, Succeeded, Failed, Cancelled, Unknown };
    struct OperationUpdate
    {
        OperationOutcome outcome = OperationOutcome::Running;
        bool submitted = false; // Sticky: a later update cannot undo submission.
        std::optional<std::string> completionScope;
        Json evidence = Json::array();
        std::optional<OperationError> error;
        Json result = nullptr;
    };

    struct ActionAdapter
    {
        std::function<ValidationResult(const Json &params, const Json &context)> validate;
        std::function<OperationUpdate(const OperationView &)> begin;
        std::function<std::optional<OperationUpdate>(const OperationView &)> tick;
        std::function<OperationUpdate(const OperationView &)> cancel;
    };

    struct OperationRecord
    {
        OperationView view;
        std::string requestId;
        std::string canonical;
        std::string state = "accepted";
        uint64_t startedAt = 0;
        uint64_t deadlineAt = 0;
        BudgetRequest budget;
        bool budgetHeld = false;
        bool begun = false;
        bool cancelRequested = false;
        bool sideEffectsPossible = false;
        std::optional<std::string> completionScope;
        Json evidence = Json::array();
        std::optional<OperationError> error;
        Json result = nullptr;
        Json lateResolution = nullptr;
        bool terminal() const;
        Json json() const;
    };
}
