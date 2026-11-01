#include "tradeadapter.hpp"
#include "runtime.hpp"
#include "gameadapter.hpp"
#include "economy.hpp"
#include "tradesource.hpp"
#include "client.hpp"
#include "processrun.hpp"
#include "inputstringboard.hpp"
#include "gui/directtradeboard/directtradeboard.hpp"
#include "dbcomid.hpp"
#include "uidf.hpp"
#include <chrono>
#include <climits>
#include <limits>
#include <map>
#include <set>

namespace aibridge
{
    namespace
    {
        uint64_t wallMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }
        uint64_t monotonicMs()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        OperationError tradeError(std::string code)
        {
            return {std::move(code), "交易未完成，请核对当前面板与操作结果", Json::object()};
        }
        Json offerJson(const TradeOfferSnapshot &offer)
        {
            Json items = Json::array();
            for(const auto &item: offer.items){
                items.push_back({{"item_id",item.itemId},{"seq_id",item.seqId},{"count",item.count},
                    {"name",item.name ? Json(*item.name) : Json(nullptr)}});
            }
            return {{"revision",std::to_string(offer.revision)},{"gold",std::to_string(offer.gold)},
                {"items",std::move(items)},{"locked",offer.locked},{"confirmed",offer.confirmed},{"pending",offer.pending}};
        }
        std::vector<TradeItemSnapshot> itemSnapshots(const std::vector<SDItem> &items)
        {
            if(items.size()>100){ throw ProtocolError("unavailable","交易物品超过观察上限"); }
            std::vector<TradeItemSnapshot> result;
            for(const auto &item: items){
                if(item.count>std::numeric_limits<uint32_t>::max()){ throw ProtocolError("unavailable","交易数量超出契约"); }
                std::optional<std::string> name;
                if(const auto &record=DBCOM_ITEMRECORD(item.itemID); record){
                    std::string text=to_cstr(record.name);
                    if(text.size()<=256){ name=std::move(text); }
                }
                result.push_back({item.itemID,item.seqID,uint32_t(item.count),std::move(name)});
            }
            return result;
        }
    }

    Json TradePanelSnapshot::json() const
    {
        Json value={{"trade_handle",handle},{"peer_uid",std::to_string(peerUid)},
            {"local",offerJson(local)},{"peer",offerJson(peer)}};
        validateDefinition(value,"trade");
        return value;
    }
    Json TradeInvitationSnapshot::json() const
    {
        return {{"invitation_handle",handle},{"peer_uid",std::to_string(peerUid)}};
    }

    struct AiTradeAdapter::Impl
    {
        Client &client;
        AiBridgeRuntime &runtime;
        Json scope=nullptr;
        TradeSources sources;
        uint64_t revision=0, serial=0, generation=0, invitationRevision=0, invitationPeer=0;
        std::string tradeHandle, invitationHandle, fingerprint;
        const std::string nonce=economyNonce();
        struct Pending
        {
            OperationView view;
            uint64_t peer=0, generation=0, localRevision=0;
            bool submitted=false, cancelled=false;
            uint32_t unrelatedError=0;
            std::optional<OperationUpdate> result;
        };
        std::optional<Pending> pending;

        Impl(Client &c,AiBridgeRuntime &r):client(c),runtime(r) {}
        AiBridgeController &controller() const { return *runtime.controller(); }
        ProcessRun &run() const
        {
            auto scene=client.getProcess<ProcessRun>();
            if(!scene || scene->aiState().phase!="run" || controller().context().is_null()){
                throw ProtocolError("not_in_game","角色尚未进入地图");
            }
            return *scene;
        }
        DirectTradeBoard &board() const { return *dynamic_cast<DirectTradeBoard *>(run().getWidget("DirectTradeBoard")); }
        InputStringBoard &input() const { return *dynamic_cast<InputStringBoard *>(run().getWidget("InputStringBoard")); }
        void refresh()
        {
            const auto context=controller().context();
            sources.sync(context);
            auto current=context;
            if(!current.is_null()){ current.erase("control_epoch"); }
            if(current!=scope){
                scope=current; tradeHandle.clear(); invitationHandle.clear(); fingerprint.clear();
                generation=invitationRevision=invitationPeer=0; ++revision;
            }
            const auto scene=client.getProcess<ProcessRun>();
            if(scope.is_null() || !scene || scene->aiState().phase!="run"){
                sources.clear(); tradeHandle.clear(); invitationHandle.clear(); return;
            }
            auto &trade=board(); auto &choice=input();
            if(!sources.trade(context,trade.peerUID(),trade.generation())) { tradeHandle.clear(); generation=0; }
            else if(tradeHandle.empty() || generation!=trade.generation()){
                generation=trade.generation(); tradeHandle="trade-"+nonce+"-"+std::to_string(++serial);
            }
            if(!choice.tradePeerUid()){ sources.clearInvitation(); }
            if(!sources.invitation(context,choice.tradePeerUid(),choice.revision())) { invitationHandle.clear(); invitationRevision=invitationPeer=0; }
            else if(invitationHandle.empty() || invitationRevision!=choice.revision() || invitationPeer!=choice.tradePeerUid()){
                invitationRevision=choice.revision(); invitationPeer=choice.tradePeerUid();
                invitationHandle="invitation-"+nonce+"-"+std::to_string(++serial);
            }
            const auto key=tradeHandle+":"+invitationHandle+":"+std::to_string(trade.localRevision())+":"
                +std::to_string(trade.peerRevision())+":"+std::to_string(trade.offerPending())+":"
                +std::to_string(trade.localConfirmed())+":"+std::to_string(trade.closePending());
            if(key!=fingerprint){ fingerprint=key; ++revision; runtime.game()->markChanged(); }
        }
        TradeInteractionSnapshot interaction()
        {
            refresh();
            TradeInteractionSnapshot result{.revision=revision};
            if(!tradeHandle.empty()){
                auto &trade=board();
                const auto &ack=trade.localAck();
                TradePanelSnapshot panel{.handle=tradeHandle,.peerUid=trade.peerUID()};
                if(ack.gold>INT_MAX){ throw ProtocolError("unavailable","交易金币超出契约"); }
                panel.local={trade.localRevision(),uint32_t(ack.gold),itemSnapshots(ack.itemList),trade.localLocked(),ack.confirmed,
                    trade.offerPending() || trade.closePending() || (trade.localConfirmed()!=ack.confirmed)};
                if(trade.peerGold()>INT_MAX){ throw ProtocolError("unavailable","交易金币超出契约"); }
                panel.peer={trade.peerRevision(),uint32_t(trade.peerGold()),itemSnapshots(trade.peerItems()),trade.peerLocked(),trade.peerConfirmed(),false};
                result.trade=std::move(panel);
            }
            if(!invitationHandle.empty()){ result.invitation=TradeInvitationSnapshot{invitationHandle,invitationPeer}; }
            return result;
        }
        uint64_t peerFor(std::string_view method,const Json &params,const Json &context)
        {
            if(method=="game_trade_request"){
                return runtime.game()->resolveTarget(params.at("target_handle").get<std::string>(),context).uid;
            }
            if(method=="game_trade_respond"){
                if(invitationHandle.empty() || params.at("invitation_handle")!=invitationHandle){ throw ProtocolError("stale_target","邀请已失效"); }
                return invitationPeer;
            }
            if(tradeHandle.empty() || params.at("trade_handle")!=tradeHandle){ throw ProtocolError("stale_target","交易面板已失效"); }
            return board().peerUID();
        }
        void checkPeer(uint64_t peer,const MyHero &hero)
        {
            auto &scene=run();
            if(scene.getCOList().size()>8192){ throw ProtocolError("unavailable","可见对象扫描超限"); }
            const auto co=scene.findUID(peer,true);
            if(!uidf::isPlayer(peer) || peer==hero.UID() || !co || !hero.inViewRange(co)){
                throw ProtocolError("stale_target","交易玩家不在当前真实视野");
            }
            if(!co->alive() || co->dead()==true){ throw ProtocolError("target_lost","交易玩家已死亡"); }
        }
        std::vector<SDDirectTradeItem> requestedItems(const Json &params) const
        {
            std::vector<SDDirectTradeItem> result;
            for(const auto &item:params.at("items")){
                result.push_back({.itemID=item.at("item_id").get<uint32_t>(),.seqID=item.at("seq_id").get<uint32_t>(),.count=item.at("count").get<uint32_t>()});
            }
            return result;
        }
        BudgetRequest validate(std::string_view method,const Json &params,const Json &context)
        {
            refresh(); auto &scene=run(); auto &hero=*scene.getMyHero(); auto &trade=board();
            if(context!=controller().context()){ throw ProtocolError("stale_context","角色上下文已改变"); }
            if(!scene.aiState().alive){ throw ProtocolError("paused","角色当前不能交易"); }
            const auto peer=peerFor(method,params,context);
            if(method!="game_trade_cancel" && !(method=="game_trade_respond" && !params.at("accept").get<bool>())){ checkPeer(peer,hero); }
            if(!runtime.policy() || !runtime.policy()->allowsPeer(peer)){ throw ProtocolError("policy_denied","交易玩家未获授权"); }
            BudgetRequest budget{.economic=true,.peerUid=peer};
            if(method=="game_trade_request" || method=="game_trade_respond"){
                if(trade.peerUID() || !hero.aiCanStartMove()
                    || (method=="game_trade_request" && input().show())
                    || dynamic_cast<AuctionBoard *>(scene.getWidget("AuctionBoard"))->pending()
                    || dynamic_cast<AuctionRegisterBoard *>(scene.getWidget("AuctionRegisterBoard"))->pending()){
                    throw ProtocolError("busy","已有动作或交易面板");
                }
                if((method=="game_trade_request" || params.value("accept",false)) && !scene.getRuntimeConfig<RTCFG_允许交易>()){
                    throw ProtocolError("busy","请先在游戏设置中允许交易");
                }
                return budget;
            }
            if(trade.closePending()){ throw ProtocolError("busy","正在等待交易关闭结果"); }
            if(method=="game_trade_cancel"){ return budget; }
            if(trade.offerPending() || trade.localConfirmed()){ throw ProtocolError("busy","交易请求正在等待结果"); }
            if(method=="game_trade_lock"){
                if(trade.localLocked() || !trade.localOfferAcknowledged() || std::stoull(params.at("offer_revision").get<std::string>())!=trade.localRevision()){
                    throw ProtocolError("stale_target","本方报价版本已改变");
                }
            }
            if(method=="game_trade_commit"){
                if(!trade.localLocked() || !trade.peerLocked() || !trade.localOfferAcknowledged()
                    || std::stoull(params.at("local_revision").get<std::string>())!=trade.localRevision()
                    || std::stoull(params.at("peer_revision").get<std::string>())!=trade.peerRevision()){
                    throw ProtocolError("stale_target","双方锁定或报价版本不匹配");
                }
            }
            if(method=="game_trade_offer" && trade.localLocked()){ throw ProtocolError("busy","本方报价已锁定"); }
            if(!hero.inventoryKnown() || !hero.goldKnown()){ throw ProtocolError("unavailable","金币或背包尚未收到"); }
            const uint64_t gold=method=="game_trade_offer" ? std::stoull(params.at("gold").get<std::string>()) : trade.localAck().gold;
            const auto &policy=*runtime.policy();
            if(gold>INT_MAX || gold>hero.getGold()){ throw ProtocolError("invalid_argument","金币不足或超过游戏上限"); }
            if(gold>policy.maxGoldPerOperation){ throw ProtocolError("budget_exceeded","报价超过单次金币权限"); }
            auto items=method=="game_trade_offer" ? requestedItems(params) : std::vector<SDDirectTradeItem>{};
            if(method!="game_trade_offer"){
                for(const auto &item:trade.localAck().itemList){
                    if(item.count>std::numeric_limits<uint32_t>::max()){ throw ProtocolError("unavailable","交易物品数量超出契约"); }
                    items.push_back({.itemID=item.itemID,.seqID=item.seqID,.count=uint32_t(item.count)});
                }
            }
            const auto &pack=hero.getInvPack();
            if(pack.getGrabbedItem() || pack.getPackBinList().size()>8192 || items.size()>100){ throw ProtocolError("busy","背包暂存或观察上限阻止交易"); }
            std::map<std::pair<uint32_t,uint32_t>,uint64_t> counts;
            for(const auto &bin:pack.getPackBinList()){ counts[{bin.item.itemID,bin.item.seqID}]+=bin.item.count; }
            for(const auto &item:trade.localItemList()){ counts[{item.itemID,item.seqID}]+=item.count; }
            uint64_t total=0;
            std::set<std::pair<uint32_t,uint32_t>> seen;
            for(const auto &item:items){
                const auto key=std::pair{item.itemID,item.seqID};
                if(!item.count || !seen.insert(key).second || counts[key]<item.count){ throw ProtocolError("invalid_argument","物品重复、不可用或数量不足"); }
                if(!policy.allowsItem(item.itemID)){ throw ProtocolError("policy_denied","交易物品未获授权"); }
                total+=item.count;
                if(total>policy.maxItemCountPerOperation){ throw ProtocolError("budget_exceeded","交易物品数量超过权限"); }
                budget.itemIds.push_back(item.itemID);
            }
            // Offers and locks check policy but transfer nothing. Reserve only at commit.
            if(method=="game_trade_commit"){ budget.gold=gold; budget.itemCount=uint32_t(total); }
            return budget;
        }
        OperationUpdate observed(std::string description,OperationOutcome outcome=OperationOutcome::Succeeded)
        {
            return {.outcome=outcome,.submitted=true,.completionScope=outcome==OperationOutcome::Succeeded ? std::optional<std::string>("observed_postcondition") : std::nullopt,
                .evidence=Json::array({{{"source","server"},{"kind","action"},{"observed_at",std::to_string(wallMs())},
                    {"snapshot_seq",std::to_string(revision)},{"server_code",nullptr},{"description",std::move(description)}}})};
        }
        OperationUpdate begin(const OperationView &view)
        {
            try{
                validate(view.method,view.params,view.context);
                const auto peer=peerFor(view.method,view.params,view.context);
                pending=Pending{.view=view,.peer=peer,.generation=board().generation(),.localRevision=board().localRevision()};
                std::optional<OperationError> sendError;
                const auto canSend=[&]{
                    sendError=validateEconomicSend(view,controller().context(),controller().controlState()==ControlState::Active
                        && controller().activeOperation()==view.operationId,monotonicMs());
                    return !sendError;
                };
                if(!canSend()){ pending.reset(); return {.outcome=OperationOutcome::Failed,.error=sendError}; }
                bool sent=false;
                if(view.method=="game_trade_request"){ sent=run().requestDirectTrade(peer,canSend); }
                else if(view.method=="game_trade_respond"){
                    sent=input().respondTradeChoice(invitationRevision,peer,view.params.at("accept").get<bool>(),canSend);
                    if(sent && !view.params.at("accept").get<bool>()){
                        auto update=observed("已通过原邀请面板发送拒绝；不表示对端已收到");
                        update.evidence[0]["source"]="client";
                        update.completionScope="client_state"; pending.reset(); return update;
                    }
                }
                else if(view.method=="game_trade_offer"){ sent=board().replaceOffer(uint32_t(std::stoull(view.params.at("gold").get<std::string>())),requestedItems(view.params),canSend); }
                else if(view.method=="game_trade_lock"){ sent=board().lockOffer(canSend); }
                else if(view.method=="game_trade_commit"){ sent=board().confirmTrade(canSend); }
                else if(view.method=="game_trade_cancel"){ sent=board().requestClose(canSend); }
                if(!sent){ pending.reset(); return {.outcome=OperationOutcome::Failed,.error=sendError ? sendError : std::optional<OperationError>(tradeError("busy"))}; }
                pending->submitted=true;
                refresh();
                return {.submitted=true};
            }
            catch(const ProtocolError &error){
                const bool sent=pending && pending->view.operationId==view.operationId && pending->submitted;
                return {.outcome=sent ? OperationOutcome::Unknown : OperationOutcome::Failed,.submitted=sent,.error=tradeError(error.code)};
            }
        }
        OperationUpdate tick(const OperationView &view)
        {
            if(!pending || pending->view.operationId!=view.operationId){ return {.outcome=OperationOutcome::Unknown,.submitted=view.submitted}; }
            if(pending->result){ auto result=*pending->result; pending.reset(); return result; }
            if(pending->unrelatedError){
                return receiptUpdate(TradeReceipt::UnrelatedError,"收到未关联交易错误，不能确认本次请求是否执行",pending->unrelatedError);
            }
            return {.submitted=pending->submitted};
        }
        OperationUpdate receiptUpdate(TradeReceipt receipt,std::string_view description,uint32_t code=0)
        {
            auto result=tradeUpdate(pending->view.method,receipt,pending->submitted,code);
            for(auto &entry:result.evidence){
                entry["observed_at"]=std::to_string(wallMs());
                entry["snapshot_seq"]=std::to_string(revision);
                entry["description"]=description;
            }
            return result;
        }
        void event(TradeEvent event,uint64_t peer,uint32_t code,uint64_t sourceGeneration)
        {
            refresh();
            const auto context=controller().context();
            const auto scene=client.getProcess<ProcessRun>();
            if(context.is_null() || !scene || scene->aiState().phase!="run"){ return; }
            if(event==TradeEvent::Started && sourceGeneration && sourceGeneration==board().generation() && peer==board().peerUID()){
                sources.started(context,peer,sourceGeneration);
                sources.clearInvitation();
                refresh();
            }
            if(event==TradeEvent::Invitation && sourceGeneration && sourceGeneration==input().revision() && peer==input().tradePeerUid()){
                sources.invited(context,peer,sourceGeneration);
                refresh();
            }
            if(!pending || !pending->submitted || controller().context().is_null()
                || economyScope(pending->view.context)!=economyScope(controller().context())){ return; }
            if(event!=TradeEvent::Error && peer!=pending->peer){ return; }
            const auto &method=pending->view.method;
            if(event==TradeEvent::Started && (method=="game_trade_request" || method=="game_trade_respond")){
                if(sourceGeneration<=pending->generation || !sources.trade(context,peer,sourceGeneration)){ return; }
                pending->result=receiptUpdate(TradeReceipt::Started,"观察到与目标玩家建立的真实交易面板");
            }
            else if(event==TradeEvent::LocalOffer && sourceGeneration==pending->generation && board().generation()==pending->generation && board().localRevision()>pending->localRevision){
                if(method=="game_trade_offer" && !board().offerPending() && !board().localLocked()) {
                    pending->result=receiptUpdate(TradeReceipt::OfferAccepted,"观察到服务器返回本方报价；尚未转移金币或物品");
                }
                if(method=="game_trade_lock" && board().localLocked()) {
                    pending->result=receiptUpdate(TradeReceipt::Locked,"观察到本方报价已锁定；尚未完成交易");
                }
            }
            else if(event==TradeEvent::Completed && method=="game_trade_commit"){
                if(!TradeSources::matches({economyScope(pending->view.context),pending->peer,pending->generation},context,peer,sourceGeneration)){ return; }
                pending->result=receiptUpdate(TradeReceipt::Complete,"收到完整交易完成状态；正常游戏路径已应用权威金币与背包，不是单方确认");
            }
            else if(event==TradeEvent::Closed){
                if(!TradeSources::matches({economyScope(pending->view.context),pending->peer,pending->generation},context,peer,sourceGeneration)){ return; }
                pending->result=receiptUpdate(TradeReceipt::Closed,"收到交易关闭状态，原面板已按正常路径释放暂存；未观察到完整交易完成");
            }
            else if(event==TradeEvent::Error){
                // Error packets have no peer/request identity. Never release a commit reservation from one alone.
                pending->unrelatedError=code;
            }
        }
    };

    AiTradeAdapter::AiTradeAdapter(Client &client,AiBridgeRuntime &runtime):m_impl(std::make_unique<Impl>(client,runtime)) {}
    AiTradeAdapter::~AiTradeAdapter()=default;
    TradeInteractionSnapshot AiTradeAdapter::interaction(){ return m_impl->interaction(); }
    void AiTradeAdapter::serverEvent(TradeEvent event,uint64_t peer,uint32_t error,uint64_t generation){ m_impl->event(event,peer,error,generation); }
    void AiTradeAdapter::observe()
    {
        auto &s=*m_impl;
        s.refresh();
        if(s.pending && s.pending->result && s.pending->cancelled && !s.controller().context().is_null()
            && economyScope(s.pending->view.context)==economyScope(s.controller().context())){
            const auto id=s.pending->view.operationId;
            auto result=*s.pending->result; s.pending.reset();
            s.controller().report(id,result);
        }
    }
    void AiTradeAdapter::registerTools()
    {
        auto &s=*m_impl;
        for(const std::string method:{"game_trade_request","game_trade_respond","game_trade_offer","game_trade_lock","game_trade_commit","game_trade_cancel"}){
            ActionAdapter action;
            action.validate=[&s,method](const Json &params,const Json &context){
                try { return ValidationResult{.budget=s.validate(method,params,context)}; }
                catch(const ProtocolError &error){ return ValidationResult{.error=tradeError(error.code)}; }
            };
            action.begin=[&s](const OperationView &view){ return s.begin(view); };
            action.tick=[&s](const OperationView &view){ return s.tick(view); };
            action.cancel=[&s](const OperationView &view){
                const bool submitted=view.submitted || (s.pending && s.pending->view.operationId==view.operationId && s.pending->submitted);
                if(s.pending && s.pending->view.operationId==view.operationId){ s.pending->cancelled=true; }
                return OperationUpdate{.outcome=submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled,.submitted=submitted};
            };
            s.controller().registerAction(method,std::move(action));
        }
    }
}
