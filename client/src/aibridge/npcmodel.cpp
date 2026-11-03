#include "npcmodel.hpp"
#include <limits>
#include <utf8.h>

namespace aibridge
{
    namespace
    {
        Json scope(Json value)
        {
            validateDefinition(value, "context");
            value.erase("control_epoch");
            return value;
        }
    }

    bool npcEventFits(const NpcOption &v) noexcept
    {
        const auto fits = [](const std::string &s, size_t limit){ return s.size() <= limit && s.find('\0') == std::string::npos; };
        return fits(v.path, 99) && !v.event.empty() && fits(v.event, 99) && (!v.args || fits(*v.args, 199));
    }
    bool npcInputFits(std::string_view text, size_t maxBytes) noexcept
    {
        return maxBytes > 0 && maxBytes <= 199 && text.size() <= maxBytes && text.find('\0') == std::string_view::npos
            && utf8::is_valid(text.begin(), text.end());
    }
    std::string NpcModel::issue(NpcPanelKind kind)
    {
        if(m_revision == std::numeric_limits<uint64_t>::max()){ throw ProtocolError("unavailable", "NPC identity exhausted"); }
        return "npc-" + std::to_string(static_cast<int>(kind)) + "-" + std::to_string(++m_revision);
    }
    void NpcModel::clear()
    {
        m_scope = nullptr;
        m_dialogue.reset(); m_input.reset(); m_shop.reset(); m_invop.reset();
        for(auto &h: m_handles){ h.clear(); }
        issue(NpcPanelKind::Dialogue);
    }
    void NpcModel::sync(const Json &context, std::optional<NpcDialogue> dialogue, std::optional<NpcInput> input,
        std::optional<NpcPanelIdentity> shop, std::optional<NpcPanelIdentity> invop)
    {
        const auto nextScope = scope(context);
        if(nextScope != m_scope){ clear(); m_scope = nextScope; }
        if(dialogue){
            if(dialogue->text.size() > 32768 || dialogue->options.size() > 100 || dialogue->npcHandle.empty()){
                throw ProtocolError("unavailable", "NPC dialogue exceeds observation limit");
            }
            for(const auto &option: dialogue->options){
                if(option.text.size() > 2048 || !npcEventFits(option)){
                    throw ProtocolError("unavailable", "NPC option exceeds normal event limits");
                }
            }
        }
        if(input && (input->prompt.size() > 2048 || input->maxBytes == 0 || input->maxBytes > 199)){
            throw ProtocolError("unavailable", "NPC input exceeds normal input limits");
        }
        const auto update = [this](auto &current, auto next, NpcPanelKind kind){
            if(current != next){
                auto id = issue(kind);
                current = std::move(next);
                m_handles.at(static_cast<size_t>(kind)) = current ? std::move(id) : std::string{};
            }
        };
        update(m_dialogue, std::move(dialogue), NpcPanelKind::Dialogue);
        update(m_input, std::move(input), NpcPanelKind::Input);
        update(m_shop, shop, NpcPanelKind::Shop);
        update(m_invop, invop, NpcPanelKind::InvOp);
    }
    std::string NpcModel::handle(NpcPanelKind kind) const { return m_handles.at(static_cast<size_t>(kind)); }
    void NpcModel::checkContext(const Json &context) const
    {
        if(m_scope.is_null() || scope(context) != m_scope){ throw ProtocolError("stale_context", "NPC context changed"); }
    }
    const NpcDialogue &NpcModel::dialogue(std::string_view id, const Json &context) const
    {
        checkContext(context);
        if(!m_dialogue || id != handle(NpcPanelKind::Dialogue)){ throw ProtocolError("stale_target", "NPC dialogue changed"); }
        return *m_dialogue;
    }
    NpcOption NpcModel::option(std::string_view id, std::string_view optionHandle, const Json &context) const
    {
        const auto &d = dialogue(id, context);
        for(size_t i = 0; i < d.options.size(); ++i){
            if(optionHandle == std::string(id) + ":" + std::to_string(i)){ return d.options[i]; }
        }
        throw ProtocolError("stale_target", "NPC option is not in the current dialogue");
    }
    const NpcInput &NpcModel::input(std::string_view id, const Json &context) const
    {
        checkContext(context);
        if(!m_input || id != handle(NpcPanelKind::Input)){ throw ProtocolError("stale_target", "NPC input changed"); }
        return *m_input;
    }
    NpcPanelIdentity NpcModel::panel(NpcPanelKind kind, std::string_view id, const Json &context) const
    {
        checkContext(context);
        const auto p = kind == NpcPanelKind::Shop ? m_shop : kind == NpcPanelKind::InvOp ? m_invop : std::nullopt;
        if(!p || id != handle(kind)){ throw ProtocolError("stale_target", "NPC panel changed"); }
        return *p;
    }
    Json NpcModel::snapshot(SnapshotStamp stamp, bool auctionOpen) const
    {
        auto result = snapshotMetadata(stamp, "current_panel", {"interaction.trade", "interaction.invitation"});
        Json d = nullptr, input = nullptr;
        if(m_dialogue){
            Json options = Json::array();
            for(size_t i = 0; i < m_dialogue->options.size(); ++i){
                const auto &o = m_dialogue->options[i];
                options.push_back({{"option_handle", handle(NpcPanelKind::Dialogue) + ":" + std::to_string(i)},
                    {"text", o.text}, {"closes_dialogue", o.autoClose}});
            }
            d = {{"dialogue_id", handle(NpcPanelKind::Dialogue)}, {"npc_handle", m_dialogue->npcHandle},
                {"revision", std::to_string(m_dialogue->revision)}, {"text", m_dialogue->text}, {"options", std::move(options)}, {"source", "game_npc"}};
        }
        if(m_input){
            input = {{"input_handle", handle(NpcPanelKind::Input)}, {"prompt", m_input->prompt},
                {"max_utf8_bytes", m_input->maxBytes}, {"sensitive", m_input->sensitive}};
        }
        result["interaction"] = {{"dialogue", std::move(d)}, {"input", std::move(input)},
            {"shop_handle", m_shop ? Json(handle(NpcPanelKind::Shop)) : Json(nullptr)},
            {"invop_handle", m_invop ? Json(handle(NpcPanelKind::InvOp)) : Json(nullptr)},
            {"trade", nullptr}, {"invitation", nullptr}, {"auction_open", auctionOpen}, {"revision", std::to_string(m_revision)}};
        validateDefinition(result, "interactionSnapshot");
        return result;
    }
    OperationUpdate npcObserved(SnapshotStamp stamp, bool submitted, bool localClose, std::string_view description, Json result)
    {
        return {.outcome = OperationOutcome::Succeeded, .submitted = submitted,
            .completionScope = localClose ? "client_state" : "observed_postcondition",
            .evidence = Json::array({{{"source", "client"}, {"kind", "dialogue"},
                {"observed_at", std::to_string(stamp.observedAt)}, {"snapshot_seq", std::to_string(stamp.sequence)},
                {"server_code", nullptr}, {"description", description}}}), .result = std::move(result)};
    }
    OperationUpdate observeNpcReply(SnapshotStamp stamp, const std::function<Json()> &readPanel)
    {
        Json result;
        try{ result = readPanel(); }
        catch(const ProtocolError &failure){
            if(failure.code != "unavailable" && failure.code != "stale_target" && failure.code != "not_in_game"
                && failure.code != "stale_context"){ throw; }
            return {.outcome=OperationOutcome::Unknown, .submitted=true,
                .error=OperationError{failure.code, "NPC reply could not be observed", Json::object()}};
        }
        return npcObserved(stamp, true, false, "Observed a new NPC panel; other script effects remain unconfirmed", std::move(result));
    }
}
