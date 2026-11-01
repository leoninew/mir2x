#include "shopadapter.hpp"
#include "shopmodel.hpp"
#include "runtime.hpp"
#include "npcadapter.hpp"
#include "gameadapter.hpp"
#include "client.hpp"
#include "processrun.hpp"
#include "inventoryboard.hpp"
#include "inputstringboard.hpp"
#include "gui/purchaseboard/purchaseboard.hpp"
#include <chrono>
#include <limits>

namespace aibridge
{
    namespace
    {
        uint64_t wallMs(){ return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
        uint64_t steadyMs(){ return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
        OperationError error(std::string code){ return {std::move(code), "商店操作条件已变化或结果尚未确认，请核对当前报价、库存和金币", Json::object()}; }
        bool fresh(uint64_t received){ const auto now=SDL_GetTicks(); return now>=received && now-received<economyHandleLifetimeMs; }
        ShopQuote purchaseQuote(const SDSellItem &row)
        {
            if(!row.item || row.item.count>UINT32_MAX || row.costList.size()>100){ throw ProtocolError("unavailable", "商品或成本超出观察上限"); }
            ShopQuote q; q.itemId=row.item.itemID; q.seqId=row.item.seqID; q.unitCount=uint32_t(row.item.count);
            std::map<uint32_t,uint64_t> materials;
            for(const auto &cost:row.costList){
                if(!DBCOM_ITEMRECORD(cost.itemID)){ throw ProtocolError("unavailable", "未知商品成本类型"); }
                if(cost.isGold()){
                    if(q.gold>UINT64_MAX-cost.count){ throw ProtocolError("unavailable", "商品金币成本溢出"); }
                    q.gold+=cost.count;
                }
                else{
                    auto &sum=materials[cost.itemID];
                    if(cost.count>UINT32_MAX || sum>UINT32_MAX-cost.count){ throw ProtocolError("unavailable", "商品材料成本溢出"); }
                    sum+=cost.count;
                }
            }
            for(const auto &[id,count]:materials){ if(count){ q.materials.push_back({id,uint32_t(count)}); } }
            return q;
        }
    }
    struct AiShopAdapter::Impl
    {
        Client &client;
        AiBridgeRuntime &runtime;
        QuoteStore quotes;
        ShopPanelSource shopSource, invopSource;
        std::deque<QuoteRecord> records;
        Json currentScope=nullptr;
        uint64_t sequence=0, lastFrame=0;
        std::string fingerprint;
        bool shopShown=false, inventoryShown=false;
        struct Watch
        {
            OperationView view;
            Json scope;
            ShopQuote quote;
            std::string panel;
            uint64_t npc=0, querySerial=0;
            uint32_t count=0;
            ShopObservation before;
            bool sent=false;
            ShopBuyReceipt receipt;
        };
        std::deque<Watch> watches;
        Impl(Client &c,AiBridgeRuntime &r):client(c),runtime(r){}
        AiBridgeController &controller() const { return *runtime.controller(); }
        ProcessRun &run() const
        {
            const auto scene=client.getProcess<ProcessRun>();
            if(!scene || scene->aiState().phase!="run" || controller().context().is_null()){ throw ProtocolError("not_in_game", "尚未进入角色场景"); }
            return *scene;
        }
        template<typename T> T &board(const char *name) const
        {
            const auto result=dynamic_cast<T *>(run().getWidget(name));
            if(!result){ throw ProtocolError("unavailable", "商店面板不可用"); }
            return *result;
        }
        PurchaseBoard &shop() const { return board<PurchaseBoard>("PurchaseBoard"); }
        InventoryBoard &inventory() const { return board<InventoryBoard>("InventoryBoard"); }
        void sync()
        {
            const auto context=controller().context();
            const auto next=context.is_null() ? Json(nullptr) : economyScope(context);
            if(next!=currentScope){
                currentScope=next; quotes.clear(); records.clear(); watches.clear(); fingerprint.clear();
                shopShown=false; inventoryShown=false;
            }
            if(next.is_null()){ return; }
            if(shopShown && !shop().show()){ shop().invalidateDetails(); }
            if(inventoryShown && !inventory().show()){ inventory().clearInvOp(); }
            shopShown=shop().show(); inventoryShown=inventory().show();
        }
        void checkNpc(uint64_t uid,const Json &context) const
        {
            const auto handle=runtime.game()->targetHandle(uid,context);
            const auto identity=runtime.game()->resolveTarget(handle,context);
            const auto target=run().findUID(identity.uid,true);
            if(!target || !uidf::isNPChar(uid)){ throw ProtocolError("stale_target", "商店 NPC 已不可见"); }
            const auto [x,y]=target->location();
            const auto &motion=*run().getMyHero()->currMotion();
            const int64_t dx=int64_t(motion.endX)-x,dy=int64_t(motion.endY)-y;
            if(dx*dx+dy*dy>=int64_t(SYS_MAXNPCDISTANCE)*SYS_MAXNPCDISTANCE){ throw ProtocolError("need_move", "需要先移动到商店附近"); }
        }
        ShopObservation observation(uint32_t id=0,uint32_t seq=0) const
        {
            const auto hero=run().getMyHero();
            if(!hero->inventoryKnown() || !hero->goldKnown()){ throw ProtocolError("unavailable", "背包或金币尚未完整接收"); }
            const auto &bins=hero->getInvPack().getPackBinList();
            if(bins.size()>8192){ throw ProtocolError("unavailable", "背包超出观察上限"); }
            ShopObservation result; result.gold=hero->getGold();
            for(const auto &bin:bins){
                if(!bin.item){ continue; }
                if(bin.item.count>UINT32_MAX){ throw ProtocolError("unavailable", "物品数量超出协议上限"); }
                result.items[bin.item.itemID]+=bin.item.count;
                if(bin.item.itemID==id && bin.item.seqID==seq){ result.exactCount+=bin.item.count; }
            }
            return result;
        }
        QuoteRecord publish(ShopQuote value,const Json &context,const std::string &panel,uint64_t revision)
        {
            auto record=quotes.publish(std::move(value),context,panel,revision,SDL_GetTicks(),wallMs());
            if(records.size()==QuoteStore::capacity){ records.pop_front(); }
            records.push_back(record); return record;
        }
        QuoteRecord resolve(const Json &params,const Json &context)
        {
            const auto handle=params.at("quote_handle").get<std::string>();
            const auto found=std::find_if(records.begin(),records.end(),[&](const auto &q){ return q.handle==handle; });
            if(found==records.end()){ throw ProtocolError("stale_target", "报价已过期"); }
            const auto selling=found->value.selling;
            if(!(selling ? invopSource : shopSource).current(context)){ throw ProtocolError("stale_target", "面板来源属于旧场景"); }
            const auto panel=runtime.npc()->resolvePanel(selling ? NpcPanelKind::InvOp : NpcPanelKind::Shop,found->panelHandle,context);
            checkNpc(panel.npcUid,context);
            const auto revision=selling ? inventory().quoteRevision() : shop().detailsRevision();
            const auto record=quotes.resolve(handle,context,found->panelHandle,revision,SDL_GetTicks());
            if(selling){
                const auto &gui=inventory(); const auto &key=gui.quoteQuery().key();
                if(gui.aiInvOp().invOp!=INVOP_TRADE || !gui.quoteQuery().accepted() || !gui.quotedCost()
                    || *gui.quotedCost()!=record.value.gold || record.value.unitCount!=1
                    || key.itemId!=record.value.itemId || key.seqId!=record.value.seqId || !fresh(gui.quoteReceivedAt())){
                    throw ProtocolError("stale_target", "出售报价已改变");
                }
            }
            else{
                const auto cached=shop().detailCache().find(record.value.itemId);
                if(cached==shop().detailCache().end() || !fresh(cached->second.receivedAt)){
                    throw ProtocolError("stale_target", "购买报价需要重新查询");
                }
            }
            return record;
        }
        Json list(const Json &params,const Json &context)
        {
            run(); sync();
            const auto panelHandle=params.at("shop_handle").get<std::string>();
            if(!shopSource.current(context)){ throw ProtocolError("stale_target", "商店面板来源属于旧场景"); }
            const auto panel=runtime.npc()->resolvePanel(NpcPanelKind::Shop,panelHandle,context);
            checkNpc(panel.npcUid,context);
            auto &gui=shop();
            if(gui.npcUid()!=panel.npcUid || gui.catalog().size()>256){ throw ProtocolError("unavailable", "商品目录超出观察上限"); }
            const auto requested=params.value("item_id",uint32_t(0));
            if(params.contains("item_id") && std::find(gui.catalog().begin(),gui.catalog().end(),requested)==gui.catalog().end()){
                throw ProtocolError("stale_target", "商品不在当前商店目录");
            }
            bool missing=false;
            for(const auto id:gui.catalog()){
                if(requested && requested!=id){ continue; }
                const auto cached=gui.detailCache().find(id);
                if(cached==gui.detailCache().end() || !fresh(cached->second.receivedAt)){
                    missing=true;
                    if(!gui.detailQuery().pending()){ gui.queryDetails(id); }
                    break;
                }
            }
            auto result=snapshotMetadata({++sequence,wallMs()},"current_panel");
            result["items"]=Json::array(); result["next_cursor"]=nullptr;
            const auto &query=gui.detailQuery();
            result["refresh_pending"]=missing && !query.timedOut(SDL_GetTicks());
            result["last_query_error"]=query.timedOut(SDL_GetTicks()) ? error("timeout").json() : Json(nullptr);
            if(missing){ result["unavailable_fields"].push_back("unrefreshed_shop_items"); }
            for(const auto &[id,cached]:gui.detailCache()){
                if((requested && requested!=id) || !fresh(cached.receivedAt)){ continue; }
                if(cached.value.list.size()+result["items"].size()>100){ throw ProtocolError("unavailable", "商品详情超过单次观察上限，请按 item_id 缩小范围"); }
                for(const auto &row:cached.value.list){ result["items"].push_back(quoteJson(publish(purchaseQuote(row),context,panelHandle,gui.detailsRevision()))); }
            }
            validateDefinition(result,"shopList"); return result;
        }
        ValidationResult validate(std::string_view method,const Json &params,const Json &context)
        {
            auto &scene=run(); sync();
            if(context!=controller().context()){ throw ProtocolError("stale_context", "角色上下文已变化"); }
            if(!scene.aiState().alive){ throw ProtocolError("paused", "角色当前不能交易"); }
            if(!scene.getMyHero()->aiCanStartMove() || scene.getFocusUID(FOCUS_ATTACK) || scene.getFocusUID(FOCUS_FOLLOW)
                || scene.getFocusUID(FOCUS_MAGIC) || board<InputStringBoard>("InputStringBoard").showModal()
                || board<DirectTradeBoard>("DirectTradeBoard").peerUID() || scene.getMyHero()->getInvPack().getGrabbedItem()
                || board<AuctionBoard>("AuctionBoard").pending() || board<AuctionBoard>("AuctionBoard").uncertain()
                || board<AuctionRegisterBoard>("AuctionRegisterBoard").pending() || board<AuctionRegisterBoard>("AuctionRegisterBoard").uncertain()){
                throw ProtocolError("busy", "已有动作、输入或物品预留尚未处理");
            }
            ValidationResult result; result.budget.economic=true;
            if(method=="game_shop_quote_sell"){
                if(!invopSource.current(context)){ throw ProtocolError("stale_target", "出售面板来源属于旧场景"); }
                const auto panel=runtime.npc()->resolvePanel(NpcPanelKind::InvOp,params.at("invop_handle").get<std::string>(),context);
                checkNpc(panel.npcUid,context);
                const auto &gui=inventory(); const auto &invop=gui.aiInvOp();
                const uint32_t id=params.at("item_id"),seq=params.at("seq_id");
                const auto state=observation(id,seq);
                if(invop.invOp!=INVOP_TRADE || !state.exactCount || state.exactCount>UINT32_MAX || !invop.hasType(DBCOM_ITEMRECORD(id).type)){
                    throw ProtocolError("unsupported_item_type", "当前物品不能按此出售入口报价");
                }
                if(invop.queryTag.empty() || invop.queryTag.size()>=100 || invop.queryTag.find('\0')!=std::string::npos){ throw ProtocolError("unavailable", "出售报价入口不可用"); }
                const ShopQueryKey key{invop.uid,gui.invOpRevision(),id,seq,INVOP_TRADE};
                if(gui.quoteQuery().pending() && (gui.quoteQuery().key()!=key || gui.quoteQuery().timedOut(SDL_GetTicks()))){ throw ProtocolError("busy", "上一报价仍在等待回包"); }
                result.budget.itemIds={id};
            }
            else{
                const auto record=resolve(params,context); const auto &q=record.value;
                const bool selling=method=="game_shop_sell";
                if(selling!=q.selling){ throw ProtocolError("stale_target", "报价方向不匹配"); }
                const auto state=observation(q.itemId,q.seqId);
                result.budget.itemIds={q.itemId};
                if(selling){
                    if(q.gold<std::stoull(params.at("min_total_gold").get<std::string>())){ throw ProtocolError("budget_exceeded", "出售价格低于要求"); }
                    if(state.exactCount!=inventory().quotedCount() || q.gold>INT_MAX){ throw ProtocolError("stale_target", "出售单位已改变或超出原界面上限"); }
                    result.budget.itemCount=q.unitCount;
                }
                else{
                    const uint32_t count=params.at("count");
                    if(!count || count>INT_MAX || (!DBCOM_ITEMRECORD(q.itemId).packable() && count!=1)){ throw ProtocolError("invalid_argument", "购买数量不符合原游戏语义"); }
                    if(q.gold>state.gold || q.gold>std::stoull(params.at("max_total_gold").get<std::string>())){ throw ProtocolError("budget_exceeded", "购买成本超过余额或调用上限"); }
                    uint64_t items=count;
                    for(const auto &material:q.materials){
                        const auto p=state.items.find(material.itemId);
                        if(p==state.items.end() || p->second<material.count){ throw ProtocolError("budget_exceeded", "可用材料不足"); }
                        items+=material.count; result.budget.itemIds.push_back(material.itemId);
                    }
                    if(items>UINT32_MAX){ throw ProtocolError("budget_exceeded", "本次物品总量超出上限"); }
                    result.budget.itemCount=uint32_t(items); result.budget.gold=q.gold;
                }
            }
            return result;
        }
        void ensureCurrent(const OperationView &view) const
        {
            if(const auto e=validateEconomicSend(view,controller().context(),controller().controlState()==ControlState::Active
                && controller().activeOperation()==view.operationId,steadyMs())){ throw ProtocolError(e->code,e->message); }
        }
        OperationUpdate begin(const OperationView &view)
        {
            validate(view.method,view.params,view.context);
            if(watches.size()>=256){ throw ProtocolError("unavailable", "商店结果观察已满，需要人工核对"); }
            Watch w; w.view=view; w.scope=economyScope(view.context);
            if(view.method=="game_shop_quote_sell"){
                w.panel=view.params.at("invop_handle").get<std::string>();
                w.npc=inventory().aiInvOp().uid;
                w.quote.itemId=view.params.at("item_id"); w.quote.seqId=view.params.at("seq_id");
                ensureCurrent(view);
                if(inventory().queryInvOp(w.quote.itemId,w.quote.seqId)==ShopQuery::Start::Busy){ return {.outcome=OperationOutcome::Failed,.error=error("busy")}; }
                w.querySerial=inventory().quoteQuery().serial();
            }
            else{
                const auto record=resolve(view.params,view.context); w.quote=record.value; w.panel=record.panelHandle;
                w.npc=runtime.npc()->resolvePanel(w.quote.selling ? NpcPanelKind::InvOp : NpcPanelKind::Shop,w.panel,view.context).npcUid;
                w.before=observation(w.quote.itemId,w.quote.seqId);
                w.count=w.quote.selling ? w.quote.unitCount : view.params.at("count").get<uint32_t>();
                bool sent=false;
                ensureCurrent(view);
                if(w.quote.selling){ sent=inventory().submitInvOp(w.quote.itemId,w.quote.seqId); }
                else{
                    w.receipt.arm(shop().buyAttempts(w.npc,w.quote.itemId,w.quote.seqId));
                    sent=shop().submitBuy(w.npc,w.quote.itemId,w.quote.seqId,w.count);
                }
                if(!sent){ return {.outcome=OperationOutcome::Failed,.error=error("stale_target")}; }
            }
            w.sent=true; watches.push_back(std::move(w));
            return {.submitted=true};
        }
        OperationUpdate succeeded(std::string scope,std::string kind,std::string description,Json result=Json::object(),Json serverCode=nullptr)
        {
            return {.outcome=OperationOutcome::Succeeded,.submitted=true,.completionScope=scope,
                .evidence=Json::array({{{"source",scope=="server_ack" ? "server" : "client"},{"kind",kind},
                    {"observed_at",std::to_string(wallMs())},{"snapshot_seq",std::to_string(++sequence)},
                    {"server_code",serverCode},{"description",description}}}),.result=std::move(result)};
        }
        OperationUpdate receipt(const Watch &w)
        {
            if(*w.receipt.result()==0){ return succeeded("server_ack","server_ack","收到首次精确商品购买的明确成功回执",Json::object(),"SM_BUYSUCCEED"); }
            return {.outcome=OperationOutcome::Failed,.submitted=true,
                .error=OperationError{"server_rejected","服务器拒绝首次精确商品购买",{{"server_code",std::to_string(*w.receipt.result())}}}};
        }
        std::optional<OperationUpdate> tick(const OperationView &view)
        {
            auto p=std::find_if(watches.begin(),watches.end(),[&](const auto &w){ return w.view.operationId==view.operationId; });
            if(p==watches.end()){ return std::nullopt; }
            auto &w=*p;
            if(w.receipt.result()){ const auto update=receipt(w); watches.erase(p); return update; }
            if(view.method=="game_shop_quote_sell"){
                const auto &gui=inventory();
                if(gui.quoteQuery().serial()!=w.querySerial || gui.aiInvOp().uid!=w.npc
                    || gui.quoteQuery().key().panelRevision!=gui.invOpRevision()){
                    return OperationUpdate{.outcome=OperationOutcome::Unknown,.submitted=true,.error=error("stale_target")};
                }
                if(gui.quoteQuery().accepted() && gui.quotedCost()){
                    w.panel=runtime.npc()->panelHandle(NpcPanelKind::InvOp);
                    runtime.npc()->resolvePanel(NpcPanelKind::InvOp,w.panel,view.context);
                    // invop.postCommitTrade calls uidRemove without count, whose existing default is one.
                    w.quote.selling=true; w.quote.unitCount=1; w.quote.gold=*gui.quotedCost();
                    const auto record=publish(w.quote,view.context,w.panel,gui.quoteRevision());
                    auto update=succeeded("observed_postcondition","dialogue","观察到当前唯一在途出售单位的真实报价",
                        {{"quote_handle",record.handle},{"quote",quoteJson(record)}});
                    watches.erase(p); return update;
                }
            }
            else{
                const auto after=observation(w.quote.itemId,w.quote.seqId);
                if(shopPostcondition(w.quote,w.count,w.before,after)){
                    auto update=succeeded("observed_postcondition","inventory_delta","观察到约定物品与金币的完整变化；不声明服务端相关回执",
                        {{"gold_before",std::to_string(w.before.gold)},{"gold_after",std::to_string(after.gold)}});
                    watches.erase(p); return update;
                }
            }
            return OperationUpdate{.submitted=true};
        }
        void observe()
        {
            sync(); if(currentScope.is_null()){ return; }
            for(auto p=watches.begin();p!=watches.end();){
                if(p->receipt.result()){ controller().report(p->view.operationId,receipt(*p)); p=watches.erase(p); }
                else{ ++p; }
            }
            const auto next=std::to_string(shop().detailsRevision())+":"+std::to_string(inventory().quoteRevision())+":"
                +std::to_string(shop().detailQuery().timedOut(SDL_GetTicks()))+":"+std::to_string(inventory().quoteQuery().timedOut(SDL_GetTicks()));
            if(next!=fingerprint && lastFrame!=client.renderSequence()){
                fingerprint=next; lastFrame=client.renderSequence(); controller().notifySnapshot();
            }
        }
    };
    AiShopAdapter::AiShopAdapter(Client &c,AiBridgeRuntime &r):m_impl(std::make_unique<Impl>(c,r)){}
    AiShopAdapter::~AiShopAdapter()=default;
    void AiShopAdapter::observe(){ m_impl->observe(); }
    void AiShopAdapter::buyResult(uint64_t npc,uint32_t item,uint32_t seq,int code)
    {
        auto &s=*m_impl;
        if(s.controller().context().is_null()){ return; }
        const auto scope=economyScope(s.controller().context());
        for(auto &w:s.watches){
            if(w.view.method=="game_shop_buy" && w.sent && w.scope==scope
                && w.npc==npc && w.quote.itemId==item && w.quote.seqId==seq){
                w.receipt.receive(s.shop().buyAttempts(npc,item,seq),code); break;
            }
        }
    }
    void AiShopAdapter::panelSource(NpcPanelKind kind)
    {
        auto &s=*m_impl;
        const auto context=s.controller().context();
        if(context.is_null()){ return; }
        if(kind==NpcPanelKind::Shop){
            if(s.shopSource.observe(context)){ s.shop().invalidateDetails(); }
        }
        else if(kind==NpcPanelKind::InvOp){
            if(s.invopSource.observe(context)){ s.inventory().invalidateQuote(); }
        }
    }
    void AiShopAdapter::registerTools()
    {
        auto &s=*m_impl;
        s.controller().registerQuery("game_shop_list",[&s](const Json &p,const Json &c){ return s.list(p,c); });
        for(const std::string method:{"game_shop_buy","game_shop_quote_sell","game_shop_sell"}){
            ActionAdapter action;
            action.validate=[&s,method](const Json &p,const Json &c){
                try{ return s.validate(method,p,c); } catch(const ProtocolError &e){ return ValidationResult{.error=error(e.code)}; }
            };
            action.begin=[&s](const OperationView &v){
                try{ return s.begin(v); }
                catch(const ProtocolError &e){ return OperationUpdate{.outcome=OperationOutcome::Failed,.error=error(e.code)}; }
            };
            action.tick=[&s](const OperationView &v) -> std::optional<OperationUpdate>{
                try{ return s.tick(v); }
                catch(const ProtocolError &e){ return OperationUpdate{.outcome=v.submitted ? OperationOutcome::Unknown : OperationOutcome::Failed,.submitted=v.submitted,.error=error(e.code)}; }
            };
            action.cancel=[](const OperationView &v){ return OperationUpdate{.outcome=v.submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled,.submitted=v.submitted}; };
            s.controller().registerAction(method,std::move(action));
        }
    }
}
