#include "controller.hpp"
#include "audit.hpp"
#include "events.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <limits>
#include <map>
#include <random>
#include <sstream>

namespace aibridge
{
    namespace
    {
        uint64_t steadyMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        uint64_t wallMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
        uint64_t plus(uint64_t base, uint64_t delta)
        {
            return delta > std::numeric_limits<uint64_t>::max() - base ? std::numeric_limits<uint64_t>::max() : base + delta;
        }
        void increment(uint64_t &value)
        {
            if(value == std::numeric_limits<uint64_t>::max()){ throw ProtocolError("unavailable", "控制代次已耗尽"); }
            ++value;
        }
        std::string randomSession()
        {
            std::random_device source;
            std::ostringstream text;
            text << std::hex << std::setfill('0');
            for(int i = 0; i < 4; ++i){ text << std::setw(8) << static_cast<uint32_t>(source()); }
            return text.str();
        }
        OperationError failure(std::string code)
        {
            return {std::move(code), "操作未完成，请检查当前状态和错误码", Json::object()};
        }
        std::string_view faultCategory() noexcept
        {
            const auto exception = std::current_exception();
            if(!exception){ return "internal"; }
            try{ std::rethrow_exception(exception); }
            catch(const ProtocolError &){ return "protocol"; }
            catch(const Json::exception &){ return "json"; }
            catch(const std::length_error &){ return "length"; }
            catch(const std::out_of_range &){ return "range"; }
            catch(const std::exception &){ return "standard"; }
            catch(...){ return "nonstandard"; }
        }
        bool economicMethod(std::string_view method)
        {
            constexpr std::string_view methods[] = {"game_use_item", "game_pick_up", "game_shop_buy", "game_shop_sell",
                "game_trade_offer", "game_trade_commit", "game_auction_register", "game_auction_buy", "game_auction_cancel", "game_delivery_claim"};
            return std::find(std::begin(methods), std::end(methods), method) != std::end(methods);
        }
        bool sameBudget(const BudgetRequest &a, const BudgetRequest &b)
        {
            return a.economic == b.economic && a.gold == b.gold && a.itemCount == b.itemCount
                && a.itemIds == b.itemIds && a.peerUid == b.peerUid && a.pvp == b.pvp;
        }
    }

    struct AiBridgeController::Impl
    {
        ControllerOptions options;
        RuntimeState runtime;
        ControlState control;
        std::string reason;
        std::string sessionId;
        uint64_t worldEpoch = 0;
        uint64_t controlEpoch = 0;
        uint64_t connection = 0;
        uint64_t lastConnection = 0;
        uint64_t operationSequence = 0;
        uint64_t spent = 0;
        uint64_t reserved = 0;
        bool uncertain = false;
        bool resetBudgetOnRun = true;
        std::optional<uint64_t> budgetRole;
        bool closing = false;
        bool stopping = false;
        bool faulted = false;
        std::string faultDiagnostic;
        std::map<std::string, ActionAdapter, std::less<>> actions;
        std::map<std::string, Query, std::less<>> queries;
        std::map<std::string, OperationRecord, std::less<>> records;
        std::map<std::string, std::string, std::less<>> ledger;
        std::optional<std::string> active;
        StopHook stopHook;
        EventRing events;
        AuditLog audit;
        std::shared_ptr<void> life = std::make_shared<int>(0);

        explicit Impl(ControllerOptions o)
            : options(std::move(o)), control(options.enabled ? ControlState::ReadOnly : ControlState::Disabled), audit(options.auditDirectory)
        {
            if(!options.monotonicMs){ options.monotonicMs = steadyMs; }
            if(!options.unixMs){ options.unixMs = wallMs; }
            if(!options.newSessionId){ options.newSessionId = randomSession; }
            if(!options.enabled && options.instanceId.empty()){ options.instanceId = std::string(32, '0'); }
            validateSchema(options.instanceId, contract().at("$defs").at("context").at("properties").at("instance_id"));
            validateSchema(options.build, contract().at("$defs").at("capabilities").at("properties").at("build"));
        }

        bool inGame() const { return runtime.phase == "run" && runtime.roleUid && runtime.mapUid && !sessionId.empty(); }
        Json context() const
        {
            if(!inGame()){ return nullptr; }
            return {{"instance_id", options.instanceId}, {"session_id", sessionId},
                {"world_epoch", std::to_string(worldEpoch)}, {"control_epoch", std::to_string(controlEpoch)}};
        }
        std::string stateName() const
        {
            switch(control){
                case ControlState::Disabled: return "disabled";
                case ControlState::ReadOnly: return "read_only";
                case ControlState::Active: return "active";
                case ControlState::Paused: return "paused";
            }
            return "disabled";
        }
        std::vector<std::string> tools() const
        {
            std::vector<std::string> result = {"game_get_session", "game_get_operation", "game_get_events", "game_stop"};
            for(const auto &[method, adapter]: actions){ result.push_back(method); }
            for(const auto &[method, query]: queries){ result.push_back(method); }
            return result;
        }
        Json session() const
        {
            Json allowed = Json::array();
            for(const auto &method: tools()){
                if((isAction(method) || method == "game_stop") && options.policy.allowsAction(method)){ allowed.push_back(method); }
            }
            const auto used = plus(spent, reserved);
            const auto remaining = used >= options.policy.maxGoldPerSession ? 0 : options.policy.maxGoldPerSession - used;
            Json policy = {{"allowed_actions", allowed}, {"allow_pvp", options.policy.allowPvp},
                {"allow_npc_dialogue", options.policy.allowNpcDialogue},
                {"max_gold_per_operation", std::to_string(options.policy.maxGoldPerOperation)},
                {"max_gold_per_session", std::to_string(options.policy.maxGoldPerSession)},
                {"max_item_count_per_operation", options.policy.maxItemCountPerOperation},
                {"remaining_gold_budget", std::to_string(remaining)}, {"economic_uncertain", uncertain}};
            Json result = {{"instance_id", options.instanceId}, {"phase", runtime.phase}, {"control_state", stateName()},
                {"context", context()}, {"character_uid", inGame() ? Json(std::to_string(*runtime.roleUid)) : Json(nullptr)},
                {"pause_reason", reason.empty() ? Json(nullptr) : Json(reason)},
                {"capabilities", {{"protocol_version", 1}, {"build", options.build}, {"tools", tools()},
                    {"supported_skills", Json::array()}, {"policy", policy},
                    {"limitations", Json::array({"仅声明已注册适配器；认证不会自动开启角色控制", "NPC脚本可能有未预告消耗，结构化交易预算不覆盖所有脚本副作用"})}}}};
            validateDefinition(result, "session");
            return result;
        }
        void event(std::string_view kind, const OperationRecord *record = nullptr)
        {
            const auto scope = record ? record->view.context : context();
            events.append(options.unixMs(), kind, scope, record ? std::optional<std::string_view>(record->view.operationId) : std::nullopt,
                "桥接状态已更新");
            if(!faulted){ audit.write(options.unixMs(), record ? record->view.method : "game_get_session", scope, record); }
        }
        Json response(const Json &request, Json data)
        {
            Json result = {{"v", 1}, {"id", request.at("id")}, {"ok", true}, {"data", std::move(data)}, {"context", context()}};
            validateDefinition(result, request.at("method").get<std::string>() + "Response");
            return result;
        }
        Json error(const Json &request, const OperationError &e)
        {
            Json result = {{"v", 1}, {"id", request.at("id")}, {"ok", false}, {"error", e.json()}, {"context", context()}};
            validateDefinition(result, "transportError");
            if(!faulted){ audit.write(options.unixMs(), request.at("method").get<std::string>(), context(), nullptr, e.code); }
            return result;
        }
        Json safeError(const Json &request, std::string code) const
        {
            Json id = nullptr;
            if(request.is_object() && request.contains("id") && request.at("id").is_string()){
                const auto &text = request.at("id").get_ref<const std::string &>();
                if(!text.empty() && text.size() <= 64){ id = text; }
            }
            try{ failure(code).json(); }
            catch(...){ code = "unavailable"; }
            auto result = errorResponse(id, code, context());
            validateDefinition(result, "transportError");
            return result;
        }

        std::optional<OperationError> permitted(const std::string &method, const Json &params, const Json &scope) const
        {
            if(!inGame()){ return failure("not_in_game"); }
            if(scope != context()){ return failure("stale_context"); }
            if(control != ControlState::Active || !runtime.alive || closing || stopping){ return failure("paused"); }
            if(!options.policy.allowsAction(method)){ return failure("policy_denied"); }
            if((method == "game_npc_open" || method == "game_npc_choose" || method == "game_npc_input") && !options.policy.allowNpcDialogue){
                return failure("policy_denied");
            }
            if(economicMethod(method) && uncertain){ return failure("economic_uncertain"); }
            if(params.contains("item_id") && !options.policy.allowsItem(params.at("item_id").get<uint32_t>())){ return failure("policy_denied"); }
            if(params.contains("count") && params.at("count").get<uint32_t>() > options.policy.maxItemCountPerOperation){ return failure("budget_exceeded"); }
            return std::nullopt;
        }

        ValidationResult validate(const std::string &method, const Json &params, const Json &scope)
        {
            if(auto e = permitted(method, params, scope)){ return {*e, {}}; }
            auto found = actions.find(method);
            if(found == actions.end()){ return {failure("unavailable"), {}}; }
            auto validation = found->second.validate(params, scope);
            validation.budget.economic |= economicMethod(method);
            if(auto e = permitted(method, params, scope)){ return {*e, {}}; }
            if(validation.error){ validation.error->json(); return validation; }
            const auto &b = validation.budget;
            if(b.economic && uncertain){ validation.error = failure("economic_uncertain"); }
            else if(b.pvp && (!options.policy.allowPvp || !b.peerUid)){ validation.error = failure("policy_denied"); }
            else if(b.peerUid && !options.policy.allowsPeer(*b.peerUid)){ validation.error = failure("policy_denied"); }
            else if(std::any_of(b.itemIds.begin(), b.itemIds.end(), [&](auto item){ return !options.policy.allowsItem(item); })){ validation.error = failure("policy_denied"); }
            else if(b.gold > options.policy.maxGoldPerOperation || b.itemCount > options.policy.maxItemCountPerOperation){ validation.error = failure("budget_exceeded"); }
            return validation;
        }
        void release(OperationRecord &record, bool commit)
        {
            if(!record.budgetHeld){ return; }
            reserved -= record.budget.gold;
            if(commit){ spent += record.budget.gold; }
            record.budgetHeld = false;
        }
        void finish(OperationRecord &record)
        {
            record.json();
            if(record.terminal()){
                if(record.state == "unknown" && record.budget.economic){ uncertain = true; }
                else{ release(record, record.state == "succeeded"); }
                if(active && *active == record.view.operationId){ active.reset(); }
            }
            event("operation", &record);
        }
        void apply(OperationRecord &record, const OperationUpdate &update)
        {
            // Validate adapter output before copying any untrusted domain text into state.
            for(const auto &e: update.evidence){ validateDefinition(e, "evidence"); }
            if(!update.evidence.is_array() || update.evidence.size() > 100){ throw ProtocolError("invalid_argument", "操作证据无效"); }
            if(!update.result.is_null()){ validateDefinition(update.result, "actionResult"); }
            if(update.error){ update.error->json(); }
            if(update.outcome == OperationOutcome::Succeeded && (!update.completionScope || update.evidence.empty())){
                throw ProtocolError("invalid_argument", "成功操作缺少完成证据");
            }
            if(update.completionScope && *update.completionScope != "server_ack" && *update.completionScope != "observed_postcondition"
                && *update.completionScope != "client_state"){
                throw ProtocolError("invalid_argument", "完成范围无效");
            }
            record.view.submitted |= update.submitted;
            if(record.terminal()){
                if(record.state == "unknown" && record.lateResolution.is_null()
                    && (update.outcome == OperationOutcome::Succeeded || update.outcome == OperationOutcome::Failed)){
                    record.lateResolution = {{"state", update.outcome == OperationOutcome::Succeeded ? "succeeded" : "failed"}, {"evidence", update.evidence}};
                    release(record, update.outcome == OperationOutcome::Succeeded);
                    // Remain paused and uncertain until a fresh login; late evidence never resumes actions.
                    record.json();
                    event("late_resolution", &record);
                }
                return;
            }
            record.evidence = update.evidence;
            record.result = update.result;
            record.error = update.error;
            record.completionScope.reset();
            switch(update.outcome){
                case OperationOutcome::Running: record.state = "running"; break;
                case OperationOutcome::Succeeded: record.state = "succeeded"; record.completionScope = update.completionScope; break;
                case OperationOutcome::Failed: record.state = "failed"; if(!record.error){ record.error = failure("server_rejected"); } break;
                case OperationOutcome::Cancelled:
                    record.state = record.view.submitted ? "unknown" : "cancelled";
                    break;
                case OperationOutcome::Unknown: record.state = "unknown"; break;
            }
            record.sideEffectsPossible = record.state == "unknown" || (record.view.submitted && record.cancelRequested);
            finish(record);
        }

        std::vector<std::string> interrupt(std::string_view why, bool timeout = false, bool stopIdle = false)
        {
            if(stopping){ return {}; }
            stopping = true;
            struct Guard { bool &flag; ~Guard(){ flag = false; } } guard{stopping};
            const auto id = active;
            std::vector<std::string> stopped;
            OperationRecord *record = id ? &records.at(*id) : nullptr;
            if(stopIdle && (!record || !record->begun) && stopHook){ stopped = stopHook(); }
            if(record){
                record->cancelRequested = !timeout;
                if(timeout){
                    // Establish the deadline result before cancellation callbacks can deliver a late reply.
                    record->state = record->view.submitted ? "unknown" : "failed";
                    record->sideEffectsPossible = record->view.submitted;
                    record->error = failure("timeout");
                    record->completionScope.reset();
                }
                if(record->begun){
                    try{
                        const auto update = actions.at(record->view.method).cancel(record->view);
                        record->view.submitted |= update.submitted;
                        if(update.outcome == OperationOutcome::Succeeded || update.outcome == OperationOutcome::Failed){ apply(*record, update); }
                    }catch(...){ record->view.submitted = true; }
                }
                if(stopHook && record->begun){
                    try{ stopped = stopHook(); }
                    catch(...){ record->view.submitted = true; }
                }
                if(timeout){
                    if(record->view.submitted && record->state == "failed"){
                        record->state = "unknown";
                        record->sideEffectsPossible = true;
                    }
                    finish(*record);
                }
                if(!record->terminal()){
                    const bool localStopped = !record->budget.economic
                        && ((record->view.method == "game_move_to" && std::find(stopped.begin(), stopped.end(), "move") != stopped.end())
                            || (record->view.method == "game_attack" && std::find(stopped.begin(), stopped.end(), "attack") != stopped.end()));
                    record->state = record->view.submitted && !localStopped ? "unknown" : timeout ? "failed" : "cancelled";
                    record->sideEffectsPossible = record->view.submitted;
                    record->error = failure(std::string(why));
                    record->completionScope.reset();
                    record->result = {{"submitted", record->view.submitted}};
                    if(!record->begun){ stopped.push_back("queued"); }
                    finish(*record);
                }
            }
            std::erase_if(stopped, [](const auto &s){ return s != "move" && s != "attack" && s != "follow" && s != "queued"; });
            return stopped;
        }

        void pause(std::string why)
        {
            if(control == ControlState::Disabled){ return; }
            const bool hadControl = control == ControlState::Active;
            increment(controlEpoch);
            control = ControlState::Paused;
            reason = std::move(why);
            interrupt(reason == "transport_lost" ? "transport_lost" : "paused", false, hadControl);
            event("control");
        }
        void expire(OperationRecord &record)
        {
            if(!record.terminal() && options.monotonicMs() >= record.view.deadlineMs){
                interrupt("timeout", true);
            }
        }
        void abortOnFault(std::string_view stage)
        {
            if(!faulted){ faultDiagnostic = std::string(stage) + "/" + std::string(faultCategory()); }
            faulted = true;
            control = options.enabled ? ControlState::Paused : ControlState::Disabled;
            reason = "controller_error";
            increment(controlEpoch);
            if(active){
                auto &record = records.at(*active);
                record.state = record.begun ? "unknown" : "failed";
                record.sideEffectsPossible = record.begun;
                record.error = failure("unavailable");
                record.completionScope.reset();
                record.evidence = Json::array(); record.result = nullptr;
                if(record.begun){
                    record.view.submitted = true;
                    try{ actions.at(record.view.method).cancel(record.view); }catch(...){}
                    if(stopHook){ try{ stopHook(); }catch(...){} }
                }
                finish(record);
            }
            event("control");
        }
    };

    AiBridgeController::AiBridgeController(ControllerOptions options): m_impl(std::make_unique<Impl>(std::move(options))) {}
    AiBridgeController::~AiBridgeController()
    {
        // Runtime owns game hooks and must call shutdown before destroying their targets.
        m_impl->life.reset();
    }

    void AiBridgeController::registerAction(std::string method, ActionAdapter adapter)
    {
        if(!isAction(method) || !adapter.validate || !adapter.begin || !adapter.tick || !adapter.cancel || m_impl->actions.contains(method)){
            throw std::invalid_argument("invalid or duplicate bridge action adapter");
        }
        m_impl->actions.emplace(std::move(method), std::move(adapter));
    }
    void AiBridgeController::registerQuery(std::string method, Query query)
    {
        if(!contract().at("x-methods").contains(method) || isAction(method) || !query
            || method == "game_get_session" || method == "game_get_operation" || method == "game_get_events" || method == "game_stop"
            || m_impl->queries.contains(method)){
            throw std::invalid_argument("invalid or duplicate bridge query adapter");
        }
        m_impl->queries.emplace(std::move(method), std::move(query));
    }
    void AiBridgeController::setStopHook(StopHook hook){ m_impl->stopHook = std::move(hook); }

    void AiBridgeController::observe(const RuntimeState &state)
    {
        auto &s = *m_impl;
        validateSchema(state.phase, contract().at("$defs").at("session").at("properties").at("phase"));
        const bool wasInGame = s.inGame();
        const bool newInGame = state.phase == "run" && state.roleUid && state.mapUid;
        const bool sessionChange = wasInGame != newInGame || (newInGame && s.runtime.roleUid != state.roleUid);
        const bool mapChange = wasInGame && newInGame && s.runtime.mapUid != state.mapUid;
        const bool died = wasInGame && s.runtime.alive && !state.alive;
        if((sessionChange && wasInGame) || mapChange || died){ s.pause(died ? "death" : mapChange ? "map_changed" : "session_changed"); }
        const bool phaseChange = s.runtime.phase != state.phase;
        s.runtime = state;
        if(state.phase == "login" || state.phase == "select_character" || state.phase == "disconnected"){
            s.resetBudgetOnRun = true;
        }
        if(sessionChange){
            s.sessionId.clear();
            if(newInGame){
                // A transient loading process invalidates context, but is not a new login budget.
                if(s.resetBudgetOnRun || s.budgetRole != state.roleUid){
                    s.spent = 0; s.reserved = 0; s.uncertain = false;
                    for(auto &[id, record]: s.records){ record.budgetHeld = false; }
                }
                s.resetBudgetOnRun = false;
                s.budgetRole = state.roleUid;
                s.sessionId = s.options.newSessionId();
                validateSchema(s.sessionId, contract().at("$defs").at("context").at("properties").at("session_id"));
                increment(s.worldEpoch);
                increment(s.controlEpoch);
            }
        }
        else if(mapChange){ increment(s.worldEpoch); }
        if(sessionChange || mapChange || died || phaseChange){ s.event("session"); }
    }

    void AiBridgeController::notifySnapshot()
    {
        auto &s = *m_impl;
        if(!s.options.enabled || s.closing || s.faulted || !s.inGame()){ return; }
        try{ s.event("snapshot"); }
        catch(...){ s.abortOnFault("snapshot"); }
    }

    void AiBridgeController::onConnected(uint64_t id)
    {
        auto &s = *m_impl;
        if(id == s.connection){ return; }
        if(!id || id <= s.lastConnection){ throw ProtocolError("unauthorized", "连接已失效"); }
        if(s.lastConnection){ s.pause("transport_lost"); }
        s.connection = id; s.lastConnection = id;
        s.records.clear(); s.ledger.clear(); s.active.reset();
        increment(s.controlEpoch);
        s.event("control");
    }
    void AiBridgeController::onDisconnected(uint64_t id)
    {
        auto &s = *m_impl;
        if(id != s.connection){ return; }
        s.pause("transport_lost");
        s.connection = 0;
        s.event("disconnected");
    }
    bool AiBridgeController::enableByUser()
    {
        auto &s = *m_impl;
        if(!s.options.enabled || s.closing || s.faulted || !s.inGame() || !s.runtime.alive || !s.connection){ return false; }
        if(s.control == ControlState::Active){ return true; }
        increment(s.controlEpoch);
        s.control = ControlState::Active;
        s.reason.clear();
        s.event("control");
        return true;
    }
    void AiBridgeController::pauseByUser(){ m_impl->pause("paused_by_user"); }
    void AiBridgeController::manualInput()
    {
        if(m_impl->control == ControlState::Active){ m_impl->pause("manual_input"); }
    }
    void AiBridgeController::shutdown()
    {
        auto &s = *m_impl;
        if(s.closing){ return; }
        s.closing = true;
        s.pause("shutdown");
        s.connection = 0;
        s.runtime.phase = "exiting";
        s.life.reset();
    }

    Json AiBridgeController::handleRequest(const BridgeRequest &input)
    {
        auto &s = *m_impl;
        Json request = input.message;
        try{
            request = decodeRequest(request.dump());
            if(!s.options.enabled || s.closing){ return s.error(request, failure("unavailable")); }
            onConnected(input.connectionId);
            const auto method = request.at("method").get<std::string>();
            const auto &params = request.at("params");
            if(method == "game_get_session"){ return s.response(request, s.session()); }
            if(method == "game_get_events"){
                return s.response(request, s.events.read(std::stoull(params.at("after_seq").get<std::string>()), params.value("limit", size_t{100})));
            }
            if(method == "game_get_operation"){
                const auto found = s.records.find(params.at("operation_id").get<std::string>());
                if(found == s.records.end()){ return s.error(request, failure("operation_not_found")); }
                s.expire(found->second);
                return s.response(request, found->second.json());
            }
            if(method == "game_stop"){
                const bool hadControl = s.control == ControlState::Active;
                auto id = s.active;
                if(!id && params.contains("operation_id") && s.records.contains(params.at("operation_id").get<std::string>())){
                    id = params.at("operation_id").get<std::string>();
                }
                increment(s.controlEpoch);
                s.control = ControlState::Paused;
                s.reason = "stop_requested";
                const auto stopped = s.interrupt("paused", false, hadControl);
                s.event("control");
                const auto *record = id ? &s.records.at(*id) : nullptr;
                return s.response(request, {{"paused", true}, {"stopped", stopped}, {"operation", record ? record->json() : Json(nullptr)},
                    {"side_effects_possible", record && record->sideEffectsPossible}});
            }
            if(isAction(method)){
                const auto requestId = request.at("request_id").get<std::string>();
                const auto canonical = canonicalRequest(request);
                if(const auto found = s.ledger.find(requestId); found != s.ledger.end()){
                    const auto &record = s.records.at(found->second);
                    if(record.canonical != canonical){ return s.error(request, failure("duplicate_conflict")); }
                    return s.response(request, {{"operation_id", record.view.operationId}, {"state", record.state}});
                }
                if(auto error = s.permitted(method, params, request.at("context"))){ return s.error(request, *error); }
                if(s.active){ return s.error(request, failure("busy")); }
                if(s.ledger.size() >= 4096){ return s.error(request, failure("ledger_full")); }
                auto validation = s.validate(method, params, request.at("context"));
                if(validation.error){ return s.error(request, *validation.error); }
                const auto used = plus(s.spent, s.reserved);
                if(used > s.options.policy.maxGoldPerSession || validation.budget.gold > s.options.policy.maxGoldPerSession - used){
                    return s.error(request, failure("budget_exceeded"));
                }
                increment(s.operationSequence);
                const auto operationId = "op-" + std::to_string(s.operationSequence);
                OperationRecord record;
                record.view = {operationId, method, params, request.at("context"), plus(s.options.monotonicMs(), request.at("timeout_ms").get<uint64_t>()), false};
                record.requestId = requestId; record.canonical = canonical;
                record.startedAt = s.options.unixMs();
                record.deadlineAt = plus(record.startedAt, request.at("timeout_ms").get<uint64_t>());
                record.budget = std::move(validation.budget); record.budgetHeld = true;
                record.json();
                s.reserved += record.budget.gold;
                s.ledger.emplace(requestId, operationId);
                auto [it, inserted] = s.records.emplace(operationId, std::move(record));
                s.active = operationId;
                s.event("operation", &it->second);
                return s.response(request, {{"operation_id", operationId}, {"state", "accepted"}});
            }
            if(!s.inGame()){ return s.error(request, failure("not_in_game")); }
            const auto query = s.queries.find(method);
            if(query == s.queries.end()){ return s.error(request, failure("unavailable")); }
            return s.response(request, query->second(params, s.context()));
        }
        catch(const ProtocolError &error){
            return s.safeError(request, error.code);
        }
        catch(const std::exception &){
            s.abortOnFault("request");
            return s.safeError(request, "unavailable");
        }
    }

    void AiBridgeController::checkDeadlines()
    {
        auto &s = *m_impl;
        if(!s.active || s.stopping || s.closing || s.faulted){ return; }
        try{ s.expire(s.records.at(*s.active)); }
        catch(...){ s.abortOnFault("deadline"); }
    }

    void AiBridgeController::advance()
    {
        auto &s = *m_impl;
        if(!s.active || s.stopping || s.closing){ return; }
        auto &record = s.records.at(*s.active);
        s.expire(record);
        if(record.terminal()){ return; }
        std::string_view stage = "validate";
        try{
            if(!record.begun){
                auto validation = s.validate(record.view.method, record.view.params, record.view.context);
                if(record.terminal()){ return; }
                s.expire(record);
                if(record.terminal()){ return; }
                if(validation.error || !sameBudget(validation.budget, record.budget)){
                    record.state = "failed"; record.error = validation.error.value_or(failure("stale_target")); s.finish(record); return;
                }
                record.begun = true;
                record.state = "running";
                stage = "begin";
                const auto update = s.actions.at(record.view.method).begin(record.view);
                stage = "begin_result";
                report(record.view.operationId, update);
            }
            else{
                stage = "tick";
                if(const auto update = s.actions.at(record.view.method).tick(record.view)){
                    stage = "tick_result";
                    report(record.view.operationId, *update);
                }
            }
        }
        catch(...){
            if(record.terminal() && record.begun && s.stopHook){ try{ s.stopHook(); }catch(...){} }
            s.abortOnFault(stage);
        }
    }

    void AiBridgeController::report(std::string_view id, const OperationUpdate &update)
    {
        auto &s = *m_impl;
        const auto found = s.records.find(id);
        if(found == s.records.end() || s.closing){ return; }
        found->second.view.submitted |= update.submitted;
        if(found->second.begun && found->second.state == "failed" && found->second.error
            && found->second.error->code == "timeout" && update.submitted){
            found->second.state = "unknown";
            found->second.sideEffectsPossible = true;
        }
        s.expire(found->second);
        s.apply(found->second, update);
    }
    Json AiBridgeController::session() const{ return m_impl->session(); }
    Json AiBridgeController::context() const{ return m_impl->context(); }
    ControlState AiBridgeController::controlState() const{ return m_impl->control; }
    std::string AiBridgeController::pauseReason() const{ return m_impl->reason; }
    std::string AiBridgeController::faultDiagnostic() const{ return m_impl->faultDiagnostic; }
    std::optional<std::string> AiBridgeController::activeOperation() const{ return m_impl->active; }
    std::weak_ptr<void> AiBridgeController::lifetime() const{ return m_impl->life; }
}
