#include "actions.hpp"
#include <set>

namespace aibridge
{
    std::optional<OperationError> validateCastTarget(std::string_view type, bool beneficial,
        bool target, bool grid, bool self)
    {
        bool valid = false;
        if(type == "target"){ valid = !grid && (target || beneficial) && (!self || beneficial); }
        else if(type == "ground"){ valid = grid && !target; }
        else if(type == "self" || type == "toggle"){ valid = !grid && !target; }
        else { return OperationError{"unsupported_skill", "技能没有受支持的普通施放路径", Json::object()}; }
        if(!valid){ return OperationError{"invalid_argument", "目标类型不符合该技能的真实施放路径", Json::object()}; }
        return {};
    }

    std::optional<OperationError> validateCombatPeers(const Policy &policy, const std::vector<uint64_t> &peers)
    {
        if(peers.size() > FrozenPages::maxRows){
            return OperationError{"unavailable", "受影响玩家集合超过有界检查容量", Json::object()};
        }
        for(const auto uid: peers){
            if(!policy.allowPvp || !policy.allowsPeer(uid)){
                return OperationError{"policy_denied", "可能受影响的玩家未获独立 PVP 与目标授权", Json::object()};
            }
        }
        return {};
    }

    ActionProgress::ActionProgress(ActionKind kind, const Json &context, uint64_t startedMs,
        uint64_t timeoutMs, uint32_t itemId, uint32_t seqId, uint32_t count, int equipmentSlot, uint64_t equipmentGeneration)
        : m_kind(kind), m_context(context), m_startedMs(startedMs), m_timeoutMs(timeoutMs),
          m_itemId(itemId), m_seqId(seqId), m_count(count), m_equipmentSlot(equipmentSlot), m_equipmentGeneration(equipmentGeneration)
    {
        validateDefinition(context, "context");
        if(!timeoutMs || timeoutMs > 30000 || !count || count > 65535){
            throw ProtocolError("invalid_argument", "动作期限或数量无效");
        }
        if(kind == ActionKind::EquipWear && (count != 1 || !itemId || !seqId || equipmentSlot < 0 || !equipmentGeneration)){
            throw ProtocolError("invalid_argument", "Equipment requires one exact instance, slot and send generation");
        }
    }

    OperationUpdate ActionProgress::cancel(bool submitted)
    {
        if(m_terminal){ return *m_terminal; }
        m_submitted |= submitted;
        OperationUpdate result;
        result.submitted = m_submitted;
        result.outcome = m_submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled;
        result.error = OperationError{"paused", "动作已停止；已发送的请求不能保证撤回", Json::object()};
        result.result = {{"submitted", m_submitted}};
        return *(m_terminal = result);
    }

    OperationUpdate ActionProgress::observe(const ActionObservation &s, uint64_t now)
    {
        if(m_terminal){ return *m_terminal; }
        if(s.itemChanges.size() > 100){ throw ProtocolError("unavailable", "物品变化超过本次证据容量"); }
        std::set<std::pair<uint32_t, uint32_t>> items;
        for(const auto &item: s.itemChanges){
            if(!items.emplace(item.itemId, item.seqId).second){
                throw ProtocolError("unavailable", "物品变化包含重复身份");
            }
        }
        m_submitted |= s.submitted;
        m_actionStarted |= s.actionStarted;
        OperationUpdate result;
        result.submitted = m_submitted;
        result.result = {{"submitted", m_submitted}};
        const auto evidence = [&](const char *kind, const char *description, bool server = false){
            result.evidence.push_back({{"source", server ? "server" : "client"}, {"kind", kind},
                {"observed_at", std::to_string(s.stamp.observedAt)}, {"snapshot_seq", std::to_string(s.stamp.sequence)},
                {"server_code", nullptr}, {"description", description}});
        };
        bool consumed = false;
        if(!s.itemChanges.empty()){
            result.result["item_changes"] = Json::array();
            for(const auto &item: s.itemChanges){
                result.result["item_changes"].push_back({{"item_id", item.itemId}, {"seq_id", item.seqId},
                    {"before", item.before}, {"after", item.after}});
                if(item.itemId == m_itemId && item.seqId == m_seqId && item.before >= item.after
                    && item.before - item.after == m_count){ consumed = true; }
            }
            evidence("inventory_delta", "客户端观察到库存数量变化；不表示所有变化均由本操作造成");
        }
        if(s.goldBefore && s.goldAfter){
            result.result["gold_before"] = std::to_string(*s.goldBefore);
            result.result["gold_after"] = std::to_string(*s.goldAfter);
            evidence("gold_delta", "客户端观察到金币变化；无操作关联回执时不归因于本操作");
        }
        const auto fail = [&](OperationOutcome outcome, const char *code, const char *message){
            result.outcome = outcome;
            result.error = OperationError{code, message, Json::object()};
        };
        const auto success = [&](const char *scope, const char *kind, const char *description, bool server = false){
            result.outcome = OperationOutcome::Succeeded;
            result.completionScope = scope;
            evidence(kind, description, server);
        };
        if(s.context != m_context || !s.alive){
            fail(m_submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled,
                s.context != m_context ? "stale_context" : "paused", "角色、地图或控制状态已失效，停止旧动作");
        }
        else if(now < m_startedMs){ fail(OperationOutcome::Failed, "unavailable", "动作时钟发生倒退"); }
        else if(now - m_startedMs >= m_timeoutMs){
            fail(m_submitted ? OperationOutcome::Unknown : OperationOutcome::Failed, "timeout", "动作期限内未取得足够结果证据");
        }
        else if(s.rejection){
            result.outcome = OperationOutcome::Failed; result.error = s.rejection;
            evidence("action", "本操作遇到明确拒绝；不推断服务器已经撤回先前动作");
        }
        else if(m_kind == ActionKind::Toggle && s.toggleEnabled){
            result.result["toggle_enabled"] = *s.toggleEnabled;
            success("client_state", "skill", "普通技能入口已改变客户端开关状态；不保证伤害或服务器效果");
        }
        else if(m_kind == ActionKind::UseItem && m_submitted && consumed){
            success("observed_postcondition", "inventory_delta", "指定物品实例数量减少到请求后置条件；不保证治疗或学会技能");
        }
        else if(m_kind == ActionKind::EquipWear && m_submitted && s.equipment
            && s.equipment->itemId == m_itemId && s.equipment->seqId == m_seqId
            && s.equipment->slot == m_equipmentSlot && s.equipment->generation == m_equipmentGeneration){
            const auto description = "Equipment state observed item_id=" + std::to_string(m_itemId)
                + ", seq_id=" + std::to_string(m_seqId) + " in slot=" + std::to_string(m_equipmentSlot)
                + "; does not imply consumption or a request-correlated acknowledgement";
            success("observed_postcondition", "action", description.c_str());
        }
        else if(m_kind == ActionKind::PickUp && m_submitted && s.correlatedReceipt){
            success("server_ack", "server_ack", "收到与本次拾取严格关联的成功回执", true);
        }
        else if(m_kind == ActionKind::AttackUntilDead && m_submitted && s.targetDead){
            success("observed_postcondition", "target_state", "同一目标身份已有死亡证据；不声明击杀归属");
        }
        else if((m_kind == ActionKind::AttackOnce || m_kind == ActionKind::Cast)
            && m_submitted && m_actionStarted && s.actionFinished){
            success("client_state", "action", "本次普通动作已开始并完成；不保证命中、伤害或击杀");
        }
        else if((m_kind == ActionKind::AttackOnce || m_kind == ActionKind::AttackUntilDead) && !s.targetVisible){
            fail(OperationOutcome::Failed, "target_lost", "目标已不可见，不能推断为死亡或击杀");
        }
        validateDefinition(result.result, "actionResult");
        for(const auto &entry: result.evidence){ validateDefinition(entry, "evidence"); }
        if(result.outcome != OperationOutcome::Running){ m_terminal = result; }
        return result;
    }
}
