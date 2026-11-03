#include "npcadapter.hpp"
#include "runtime.hpp"
#include "gameadapter.hpp"
#include "tradeadapter.hpp"
#include "shopadapter.hpp"
#include "client.hpp"
#include "processrun.hpp"
#include "inputstringboard.hpp"
#include "inventoryboard.hpp"
#include "gui/npcchatboard/npcchatboard.hpp"
#include "gui/purchaseboard/purchaseboard.hpp"
#include <chrono>
#include <limits>

namespace aibridge
{
    namespace
    {
        uint64_t wallMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
        OperationError error(std::string code) { return {std::move(code), "当前 NPC 交互无法执行", Json::object()}; }
        OperationUpdate failed(std::string code, bool submitted = false)
        {
            return {.outcome=OperationOutcome::Failed, .submitted=submitted, .error=error(std::move(code))};
        }
        Json scope(Json value)
        {
            if(value.is_object()){ value.erase("control_epoch"); }
            return value;
        }
    }
    struct AiNpcAdapter::Impl
    {
        Client &client;
        AiBridgeRuntime &runtime;
        NpcModel model;
        Json currentScope = nullptr;
        std::array<std::optional<NpcPanelIdentity>, 4> panels;
        std::optional<uint64_t> dialogueLayoutRevision, inputLayoutRevision;
        uint64_t serverRevision = 0, snapshotSequence = 0;
        std::string operationId;
        uint64_t pendingNpc = 0, pendingServerRevision = 0;
        bool submitted = false;
        std::optional<uint64_t> notifiedFrame;
        std::string lastVisibility;
        bool notificationPending = false;
        uint64_t interactionRevision = 0;
        std::string interactionFingerprint;

        Impl(Client &c, AiBridgeRuntime &r): client(c), runtime(r) {}
        AiBridgeController &controller() const { return *runtime.controller(); }
        ProcessRun &run() const
        {
            auto scene = client.getProcess<ProcessRun>();
            if(!scene || scene->aiState().phase != "run" || controller().context().is_null()){
                throw ProtocolError("not_in_game", "尚未进入角色场景");
            }
            return *scene;
        }
        template<typename T> T &board(const char *name) const
        {
            auto result = dynamic_cast<T *>(run().getWidget(name));
            if(!result){ throw ProtocolError("unavailable", "当前交互面板不可用"); }
            return *result;
        }
        void ensureScope()
        {
            const auto next = scope(controller().context());
            if(next != currentScope){
                model.clear(); panels = {}; lastVisibility.clear();
                dialogueLayoutRevision.reset(); inputLayoutRevision.reset();
                currentScope = next;
            }
        }
        void refresh()
        {
            auto &scene = run();
            ensureScope();
            updateVisibility();
            auto &chat = board<NPCChatBoard>("NPCChatBoard");
            auto &input = board<InputStringBoard>("InputStringBoard");
            std::optional<NpcDialogue> dialogue;
            if(chat.show() && dialogueLayoutRevision == chat.revision()){
                try { dialogue = chat.dialogue(runtime.game()->targetHandle(chat.npcUid(), controller().context())); }
                catch(const ProtocolError &e){ if(e.code != "stale_target"){ throw; } }
                catch(const std::length_error &){ throw ProtocolError("unavailable", "NPC 对话超出观察上限"); }
            }
            auto shop = panels.at(static_cast<size_t>(NpcPanelKind::Shop));
            auto invop = panels.at(static_cast<size_t>(NpcPanelKind::InvOp));
            if(!scene.getWidget("PurchaseBoard")->show()){ shop.reset(); }
            const auto &inventory = board<InventoryBoard>("InventoryBoard");
            if(!inventory.show() || inventory.aiInvOp().invOp == INVOP_NONE
                || (invop && inventory.aiInvOp().uid != invop->npcUid)){ invop.reset(); }
            try { model.sync(controller().context(), std::move(dialogue),
                inputLayoutRevision == input.revision() ? input.npcInput() : std::nullopt, shop, invop); }
            catch(const std::length_error &){ throw ProtocolError("unavailable", "NPC 输入提示超出观察上限"); }
        }
        void updateVisibility()
        {
            auto &chat = board<NPCChatBoard>("NPCChatBoard");
            auto &input = board<InputStringBoard>("InputStringBoard");
            const auto &inventory = board<InventoryBoard>("InventoryBoard");
            const auto visibility = std::to_string(serverRevision) + ":" + std::to_string(chat.revision()) + ":"
                + std::to_string(chat.show()) + ":" + std::to_string(input.revision()) + ":" + std::to_string(input.show()) + ":"
                + std::to_string(run().getWidget("PurchaseBoard")->show()) + ":" + std::to_string(inventory.show()) + ":"
                + std::to_string(inventory.aiInvOp().invOp) + ":" + std::to_string(inventory.aiInvOp().uid) + ":"
                + std::to_string(inventory.invOpRevision()) + ":" + std::to_string(board<PurchaseBoard>("PurchaseBoard").catalogRevision());
            if(visibility != lastVisibility){
                model.clear(); lastVisibility = visibility; notificationPending = true;
            }
        }
        SnapshotStamp stamp()
        {
            if(snapshotSequence == std::numeric_limits<uint64_t>::max()){ throw ProtocolError("unavailable", "NPC snapshot exhausted"); }
            return {++snapshotSequence, wallMs()};
        }
        Json interaction()
        {
            refresh();
            const auto trade = runtime.trade()->interaction();
            const bool auctionOpen = run().getWidget("AuctionBoard")->show();
            const auto key = std::to_string(model.revision()) + ":" + std::to_string(trade.revision) + ":" + std::to_string(auctionOpen);
            if(key != interactionFingerprint){
                if(interactionRevision == UINT64_MAX){ throw ProtocolError("unavailable", "交互版本已耗尽"); }
                ++interactionRevision; interactionFingerprint = key;
            }
            auto result = model.snapshot(stamp(), auctionOpen);
            result["unavailable_fields"] = Json::array();
            result["interaction"]["trade"] = trade.trade ? trade.trade->json() : Json(nullptr);
            result["interaction"]["invitation"] = trade.invitation ? trade.invitation->json() : Json(nullptr);
            result["interaction"]["revision"] = std::to_string(interactionRevision);
            if(trade.trade){
                const auto unknownName = [](const auto &offer){ return std::any_of(offer.items.begin(),offer.items.end(),[](const auto &item){ return !item.name; }); };
                if(unknownName(trade.trade->local)){ result["unavailable_fields"].push_back("interaction.trade.local.items[].name"); }
                if(unknownName(trade.trade->peer)){ result["unavailable_fields"].push_back("interaction.trade.peer.items[].name"); }
            }
            validateDefinition(result,"interactionSnapshot");
            return result;
        }
        uint64_t checkNpc(uint64_t uid, const Json &context)
        {
            if(!uidf::isNPChar(uid)){ throw ProtocolError("stale_target", "目标不是当前 NPC"); }
            auto &scene = run();
            auto &hero = *scene.getMyHero();
            auto handle = runtime.game()->targetHandle(uid, context);
            const auto identity = runtime.game()->resolveTarget(handle, context);
            const auto target = scene.findUID(identity.uid, true);
            if(!target){ throw ProtocolError("stale_target", "NPC 已离开可见范围"); }
            const auto [x, y] = target->location();
            const int64_t dx = int64_t(hero.currMotion()->endX) - x;
            const int64_t dy = int64_t(hero.currMotion()->endY) - y;
            if(dx * dx + dy * dy >= int64_t(SYS_MAXNPCDISTANCE) * SYS_MAXNPCDISTANCE){
                throw ProtocolError("need_move", "需要先移动到 NPC 附近");
            }
            return uid;
        }
        uint64_t validate(std::string_view method, const Json &params, const Json &context)
        {
            refresh();
            if(controller().context() != context){ throw ProtocolError("stale_context", "角色控制上下文已改变"); }
            auto &scene = run();
            if(!scene.aiState().alive){ throw ProtocolError("paused", "角色当前不能交互"); }
            if(method == "game_npc_close"){
                return model.dialogue(params.at("dialogue_id").get<std::string>(), context).npcUid;
            }
            const auto &hero = *scene.getMyHero();
            if(!hero.aiCanStartMove() || board<DirectTradeBoard>("DirectTradeBoard").peerUID()
                || board<AuctionBoard>("AuctionBoard").pending() || board<AuctionRegisterBoard>("AuctionRegisterBoard").pending()
                || scene.getFocusUID(FOCUS_ATTACK) || scene.getFocusUID(FOCUS_FOLLOW) || scene.getFocusUID(FOCUS_MAGIC)){
                throw ProtocolError("busy", "已有角色动作或交易正在进行");
            }
            if(method == "game_npc_input"){
                const auto &input = model.input(params.at("input_handle").get<std::string>(), context);
                if(!npcInputFits(params.at("text").get_ref<const std::string &>(), input.maxBytes)){
                    throw ProtocolError("invalid_argument", "输入超过游戏限制或包含无效字符");
                }
                return checkNpc(input.npcUid, context);
            }
            if(board<InputStringBoard>("InputStringBoard").show()){
                throw ProtocolError("busy", "请先处理当前输入面板");
            }
            if(method == "game_npc_open"){
                return checkNpc(runtime.game()->resolveTarget(params.at("target_handle").get<std::string>(), context).uid, context);
            }
            const auto id = params.at("dialogue_id").get<std::string>();
            const auto choice = model.option(id, params.at("option_handle").get<std::string>(), context);
            if(!npcEventFits(choice)){ throw ProtocolError("invalid_argument", "选项不符合游戏事件限制"); }
            return checkNpc(model.dialogue(id, context).npcUid, context);
        }
        OperationUpdate begin(const OperationView &view)
        {
            const auto uid = validate(view.method, view.params, view.context);
            if(view.method == "game_npc_close"){
                board<NPCChatBoard>("NPCChatBoard").closeDialogue();
                refresh();
                return npcObserved(stamp(), false, true, "本地 NPC 对话已关闭；已发送的脚本事件不受影响");
            }
            operationId = view.operationId;
            pendingNpc = uid;
            pendingServerRevision = serverRevision;
            submitted = false;
            if(view.method == "game_npc_open"){
                run().sendNPCEvent(uid, {}, SYS_ENTER);
            }
            else if(view.method == "game_npc_choose"){
                const auto id = view.params.at("dialogue_id").get<std::string>();
                const auto &dialogue = model.dialogue(id, view.context);
                const auto option = model.option(id, view.params.at("option_handle").get<std::string>(), view.context);
                if(!board<NPCChatBoard>("NPCChatBoard").choose(dialogue.revision, option.paragraph, option.leaf)){
                    operationId.clear(); return failed("stale_target");
                }
            }
            else{
                const auto &input = model.input(view.params.at("input_handle").get<std::string>(), view.context);
                if(!board<InputStringBoard>("InputStringBoard").submitNpcInput(input.revision, view.params.at("text").get<std::string>())){
                    operationId.clear(); return failed("invalid_argument");
                }
            }
            submitted = true;
            return {.submitted=true};
        }
        std::optional<OperationUpdate> tick(const OperationView &view)
        {
            if(operationId != view.operationId){ return failed("stale_context", view.submitted); }
            for(const auto &panel: panels){
                if(panel && panel->npcUid == pendingNpc && panel->revision > pendingServerRevision){
                    auto update = observeNpcReply(stamp(), [&]{
                        refresh();
                        Json result = Json::object();
                        if(auto handle = model.handle(NpcPanelKind::Dialogue); !handle.empty()){ result["dialogue_id"] = std::move(handle); }
                        if(auto handle = model.handle(NpcPanelKind::Input); !handle.empty()){ result["input_handle"] = std::move(handle); }
                        return result;
                    });
                    operationId.clear();
                    return update;
                }
            }
            return OperationUpdate{.submitted=submitted};
        }
    };

    AiNpcAdapter::AiNpcAdapter(Client &client, AiBridgeRuntime &runtime): m_impl(std::make_unique<Impl>(client, runtime)) {}
    AiNpcAdapter::~AiNpcAdapter() = default;
    void AiNpcAdapter::serverPanel(NpcPanelKind kind, uint64_t uid)
    {
        auto &s = *m_impl;
        s.ensureScope();
        if(!uidf::isNPChar(uid) || s.currentScope.is_null()){ return; }
        if(kind==NpcPanelKind::Shop || kind==NpcPanelKind::InvOp){ s.runtime.shop()->panelSource(kind); }
        if(s.serverRevision == std::numeric_limits<uint64_t>::max()){ throw ProtocolError("unavailable", "NPC observation exhausted"); }
        s.panels.at(static_cast<size_t>(kind)) = NpcPanelIdentity{++s.serverRevision, uid};
        if(kind == NpcPanelKind::Dialogue){ s.dialogueLayoutRevision = s.board<NPCChatBoard>("NPCChatBoard").revision(); }
        if(kind == NpcPanelKind::Input){ s.inputLayoutRevision = s.board<InputStringBoard>("InputStringBoard").revision(); }
        s.model.clear();
        s.lastVisibility.clear();
    }
    void AiNpcAdapter::observe()
    {
        auto &s = *m_impl;
        s.ensureScope();
        if(s.currentScope.is_null()){ return; }
        s.updateVisibility();
        if(s.notificationPending && s.notifiedFrame != s.client.renderSequence()){
            s.controller().notifySnapshot(); s.notifiedFrame = s.client.renderSequence(); s.notificationPending = false;
        }
    }
    NpcPanelIdentity AiNpcAdapter::resolvePanel(NpcPanelKind kind, std::string_view handle, const Json &context)
    {
        m_impl->refresh();
        return m_impl->model.panel(kind, handle, context);
    }
    std::string AiNpcAdapter::panelHandle(NpcPanelKind kind)
    {
        m_impl->refresh();
        return m_impl->model.handle(kind);
    }
    void AiNpcAdapter::registerTools()
    {
        auto &s = *m_impl;
        s.controller().registerQuery("game_get_interaction", [&s](const Json &, const Json &){
            return s.interaction();
        });
        for(const std::string method: {"game_npc_open", "game_npc_choose", "game_npc_input", "game_npc_close"}){
            ActionAdapter action;
            action.validate = [&s, method](const Json &p, const Json &c){
                try { s.validate(method, p, c); return ValidationResult{}; }
                catch(const ProtocolError &e){ return ValidationResult{.error=error(e.code)}; }
            };
            action.begin = [&s](const OperationView &v){ return s.begin(v); };
            action.tick = [&s](const OperationView &v){ return s.tick(v); };
            action.cancel = [&s](const OperationView &v){
                const bool sent = v.submitted || (s.operationId == v.operationId && s.submitted);
                if(s.operationId == v.operationId){ s.operationId.clear(); }
                return OperationUpdate{.outcome=sent ? OperationOutcome::Unknown : OperationOutcome::Cancelled, .submitted=sent};
            };
            s.controller().registerAction(method, std::move(action));
        }
    }
}
