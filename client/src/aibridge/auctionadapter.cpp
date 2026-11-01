#include "auctionadapter.hpp"
#include "economy.hpp"
#include "snapshot.hpp"
#include "runtime.hpp"
#include "gameadapter.hpp"
#include "client.hpp"
#include "processrun.hpp"
#include "inputstringboard.hpp"
#include "gui/auctionboard/auctionboard.hpp"
#include "gui/auctionboard/auctionregisterboard.hpp"
#include "gui/friendchatboard/friendchatboard.hpp"
#include "dbcomid.hpp"
#include "uidf.hpp"
#include <chrono>
#include <map>

namespace aibridge
{
    namespace
    {
        uint64_t mono(){ return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
        uint64_t wall(){ return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
        OperationError error(std::string code){ return {std::move(code),"当前经济操作条件不满足，请检查面板、物品和交易结果",Json::object()}; }
        Json itemJson(const SDItem &item)
        {
            const auto &record=DBCOM_ITEMRECORD(item.itemID);
            Json name=nullptr;
            if(record && record.name && std::char_traits<char8_t>::length(record.name)<=256){ name=to_cstr(record.name); }
            if(item.count>UINT32_MAX){ throw ProtocolError("unavailable","Item count exceeds contract"); }
            Json result={{"item_id",item.itemID},{"seq_id",item.seqID},{"count",uint32_t(item.count)},{"name",name}};
            validateDefinition(result,"itemDetails"); return result;
        }
        std::string claimState(const std::shared_ptr<EconomicRequest> &request)
        {
            if(!request){ return "unclaimed"; }
            switch(request->receipt){
                case EconomicReceipt::Ok:return "claimed";
                case EconomicReceipt::Pending:return "pending";
                default:return "unknown";
            }
        }
    }
    struct AiAuctionAdapter::Impl
    {
        Client &client;
        AiBridgeRuntime &runtime;
        EconomicHandles handles;
        EconomicPages pages;
        Json scope=nullptr;
        uint64_t sequence=0,panelRevision=0,guiRevision=0;
        bool shown=false;
        int category=0;
        uint64_t lastRender=0;
        std::string fingerprint;
        using Counts=std::map<std::pair<uint32_t,uint32_t>,uint32_t>;
        struct Watch
        {
            std::string id;
            std::shared_ptr<EconomicRequest> request;
            bool delivery;
            Json scope;
            std::optional<uint64_t> gold;
            std::optional<Counts> inventory;
        };
        std::deque<Watch> watches;
        Impl(Client &c,AiBridgeRuntime &r):client(c),runtime(r){}
        AiBridgeController &controller() const { return *runtime.controller(); }
        ProcessRun &run() const
        {
            const auto scene=client.getProcess<ProcessRun>();
            if(!scene || scene->aiState().phase!="run" || controller().context().is_null()){ throw ProtocolError("not_in_game","No game scene"); }
            return *scene;
        }
        template<typename T> T &board(const char *name) const
        {
            const auto widget=dynamic_cast<T *>(run().getWidget(name));
            if(!widget){ throw ProtocolError("unavailable","Missing game panel"); }
            return *widget;
        }
        AuctionBoard &auction() const { return board<AuctionBoard>("AuctionBoard"); }
        AuctionRegisterBoard &registration() const { return board<AuctionRegisterBoard>("AuctionRegisterBoard"); }
        FriendChatBoard &inbox() const { return board<FriendChatBoard>("FriendChatBoard"); }
        std::optional<Counts> counts() const
        {
            const auto hero=run().getMyHero();
            const auto &bins=hero->getInvPack().getPackBinList();
            if(!hero->inventoryKnown() || bins.size()>8192){ return {}; }
            Counts result;
            for(const auto &b:bins){
                if(b.item.count>UINT32_MAX){ return {}; }
                if(b.item){ result[{b.item.itemID,b.item.seqID}]=uint32_t(b.item.count); }
            }
            return result;
        }
        OperationUpdate update(const Watch &watch)
        {
            auto result=economicUpdate(*watch.request,watch.delivery);
            if(watch.request->receipt!=EconomicReceipt::Ok || controller().context().is_null()
                || watch.scope!=economyScope(controller().context()) || !client.getProcess<ProcessRun>()){ return result; }
            const auto hero=run().getMyHero();
            if(watch.gold && hero->goldKnown()){
                result.result["gold_before"]=std::to_string(*watch.gold);
                result.result["gold_after"]=std::to_string(hero->getGold());
            }
            if(watch.inventory){
                if(const auto after=counts()){
                    Counts merged=*watch.inventory;
                    for(const auto &[key,count]:*after){ merged.try_emplace(key,count); }
                    Json changes=Json::array();
                    for(const auto &[key,unused]:merged){
                        const auto beforeIt=watch.inventory->find(key),afterIt=after->find(key);
                        const auto before=beforeIt==watch.inventory->end() ? 0 : beforeIt->second;
                        const auto current=afterIt==after->end() ? 0 : afterIt->second;
                        if(before!=current){ changes.push_back({{"item_id",key.first},{"seq_id",key.second},{"before",before},{"after",current}}); }
                        if(changes.size()>100){ changes=nullptr; break; }
                    }
                    if(changes.is_array()){ result.result["item_changes"]=std::move(changes); }
                }
            }
            result.evidence.push_back({{"source","client"},{"kind","delivery"},{"observed_at",std::to_string(wall())},
                {"snapshot_seq",std::to_string(++sequence)},{"server_code",nullptr},
                {"description","附带当前客户端金币与背包差异，仅为同时观察，不把其他奖励归因于本次请求；到账可能晚于确认"}});
            return result;
        }
        void sync()
        {
            const auto current=economyScope(controller().context());
            if(scope!=current){
                if(!scope.is_null()){ auction().invalidateScope(controller().context()); }
                scope=current; handles.clear(); pages.clear(); ++panelRevision;
            }
            const auto visible=auction().show(); const auto cat=auction().currentCategory();
            if(shown!=visible || category!=cat || guiRevision!=auction().panelRevision()){
                ++panelRevision; shown=visible; category=cat; guiRevision=auction().panelRevision();
            }
        }
        Json auctionList(const Json &params,const Json &context)
        {
            run(); sync();
            const int requested=params.at("category").get<int>();
            const Json filters={{"category",requested}};
            if(params.contains("cursor")){ return pages.next("auction",context,filters,params.at("cursor").get<std::string>()); }
            auto &gui=auction();
            if(params.value("refresh",false) || !gui.cachedList(requested)){ gui.requestList(requested); }
            gui.showCategory(requested); sync();
            const auto &refresh=gui.refreshState(requested);
            auto meta=snapshotMetadata({++sequence,wall()},"current_panel");
            meta["refresh_pending"]=refresh.pending;
            meta["last_query_error"]=refresh.error ? refresh.error->json() : Json(nullptr);
            std::vector<Json> rows;
            if(const auto list=gui.cachedList(requested)){
                rows.reserve(list->itemList.size());
                for(const auto &entry:list->itemList){
                    if(!entry.seller.player() || !entry.seller.id || entry.seller.id>SYS_MAXDBID){ throw ProtocolError("unavailable","Auction seller identity unavailable"); }
                    auto item=itemJson(entry.item);
                    if(item["name"].is_null()){ meta["unavailable_fields"]=Json::array({"items[].item.name"}); }
                    const auto now=mono();
                    const auto handle=handles.publish({{"kind","auction"},{"category",requested},{"id",entry.auctionID},
                        {"panel",panelRevision},{"price",entry.price},{"item",item},{"seller",entry.seller.id}},context,refresh.revision,now);
                    Json row={{"auction_handle",handle},{"auction_id",std::to_string(entry.auctionID)},
                        {"seller_uid",std::to_string(uidf::getPlayerUID(entry.seller.id))},{"item",std::move(item)},
                        {"price",std::to_string(entry.price)},{"note",entry.note},{"expires_at",std::to_string(wall()+economyHandleLifetimeMs)},
                        {"owned",entry.seller.id==run().getMyHeroDBID()}};
                    try{ validateDefinition(row,"auction"); }
                    catch(const ProtocolError &){ throw ProtocolError("unavailable","Auction data exceeds contract"); }
                    rows.push_back(std::move(row));
                }
            }
            return pages.first("auction",context,filters,std::move(meta),std::move(rows));
        }
        Json deliveryList(const Json &params,const Json &context)
        {
            run(); sync();
            if(params.contains("cursor")){ return pages.next("delivery",context,Json::object(),params.at("cursor").get<std::string>()); }
            auto &gui=inbox();
            if(gui.deliveryOverflow()){ throw ProtocolError("unavailable","Received delivery observation limit exceeded"); }
            std::vector<Json> rows;
            for(const auto &[record,entry]:gui.deliveries()){
                const auto handle=handles.publish({{"kind","delivery"},{"record",record}},context,0,mono());
                rows.push_back({{"delivery_handle",handle},{"source","system"},{"auction_id",nullptr},{"text",entry.text},
                    {"claim_state",claimState(entry.request)},{"items",nullptr},{"gold",nullptr}});
            }
            return pages.first("delivery",context,Json::object(),snapshotMetadata({++sequence,wall()},"client_known",
                {"items[].auction_id","items[].items","items[].gold","unreceived_mailbox"}),std::move(rows));
        }
        const SDItem &availableItem(uint32_t id,uint32_t seq) const
        {
            const auto hero=run().getMyHero();
            if(!hero->inventoryKnown()){ throw ProtocolError("unavailable","Inventory unknown"); }
            const auto &bins=hero->getInvPack().getPackBinList();
            if(bins.size()>8192){ throw ProtocolError("unavailable","Inventory scan limit"); }
            for(const auto &b:bins){ if(b.item.itemID==id && b.item.seqID==seq && b.item){ return b.item; } }
            throw ProtocolError("stale_target","Item is not in available inventory");
        }
        SDAuctionItem resolveAuction(const Json &params,const Json &context)
        {
            sync();
            const auto &saved=handles.resolve(params.at("auction_handle").get<std::string>(),context,mono());
            const auto &value=saved.value;
            if(value.value("kind","")!="auction" || !shown || value.at("panel")!=panelRevision || value.at("category")!=category){
                throw ProtocolError("stale_target","Auction panel changed");
            }
            const auto &refresh=auction().refreshState(category);
            if(refresh.revision!=saved.revision || refresh.pending || refresh.quarantined){ throw ProtocolError("stale_target","Auction list changed or refresh unresolved"); }
            const auto list=auction().cachedList(category);
            if(list){
                for(const auto &item:list->itemList){
                    if(item.auctionID==value.at("id").get<uint64_t>() && item.price==value.at("price").get<uint64_t>()
                        && item.seller.id==value.at("seller").get<uint32_t>() && itemJson(item.item)==value.at("item")){ return item; }
                }
            }
            throw ProtocolError("stale_target","Auction entry changed");
        }
        ValidationResult validate(std::string_view method,const Json &params,const Json &context)
        {
            auto &scene=run(); sync();
            if(controller().context()!=context){ throw ProtocolError("stale_context","Context changed"); }
            if(!scene.aiState().alive){ throw ProtocolError("paused","Character dead"); }
            if(board<InputStringBoard>("InputStringBoard").showModal() || board<DirectTradeBoard>("DirectTradeBoard").peerUID()){
                throw ProtocolError("busy","A modal interaction or trade is active");
            }
            if(auction().pending() || auction().uncertain() || registration().pending() || registration().uncertain()){
                throw ProtocolError("economic_uncertain","Auction submission unresolved");
            }
            ValidationResult result; result.budget.economic=true;
            if(method=="game_auction_register"){
                if(registration().reservedItem() || scene.getMyHero()->getInvPack().getGrabbedItem()){ throw ProtocolError("busy","Inventory item is reserved"); }
                const auto &item=availableItem(params.at("item_id"),params.at("seq_id"));
                if(item.isGold() || item.count>UINT32_MAX || !item.seqID){ throw ProtocolError("unsupported_item_type","Invalid auction item"); }
                result.budget.itemIds={item.itemID}; result.budget.itemCount=uint32_t(item.count);
            }
            else if(method=="game_delivery_claim"){
                const auto &handle=handles.resolve(params.at("delivery_handle").get<std::string>(),context,mono());
                if(handle.value.value("kind","")!="delivery"){ throw ProtocolError("stale_target","Wrong handle kind"); }
                const auto found=inbox().deliveries().find(handle.value.at("record").get<std::string>());
                if(found==inbox().deliveries().end() || found->second.request){ throw ProtocolError("stale_target","Delivery already submitted or unavailable"); }
            }
            else{
                const auto entry=resolveAuction(params,context);
                const bool cancel=method=="game_auction_cancel";
                if((entry.seller.id==scene.getMyHeroDBID())!=cancel){ throw ProtocolError("policy_denied","Auction ownership mismatch"); }
                result.budget.itemIds={entry.item.itemID};
                if(entry.item.count>UINT32_MAX){ throw ProtocolError("unavailable","Auction quantity exceeds contract"); }
                result.budget.itemCount=uint32_t(entry.item.count);
                if(!cancel){
                    result.budget.peerUid=uidf::getPlayerUID(entry.seller.id);
                    if(!scene.getMyHero()->goldKnown()){ throw ProtocolError("unavailable","Gold unknown"); }
                    if(entry.price>std::stoull(params.at("max_total_gold").get<std::string>()) || entry.price>scene.getMyHero()->getGold()){
                        throw ProtocolError("budget_exceeded","Auction cost exceeds available funds or caller limit");
                    }
                    result.budget.gold=entry.price;
                }
            }
            return result;
        }
        void ensureCurrent(const OperationView &view) const
        {
            if(const auto e=validateEconomicSend(view,controller().context(),controller().controlState()==ControlState::Active
                && controller().activeOperation()==view.operationId,mono())){ throw ProtocolError(e->code,e->message); }
        }
        OperationUpdate begin(const OperationView &v)
        {
            bool prepared=false;
            try{
                validate(v.method,v.params,v.context);
                if(watches.size()>=256){ throw ProtocolError("unavailable","Receipt observation capacity reached"); }
                std::shared_ptr<EconomicRequest> request;
                Watch watch{v.operationId,{},false,economyScope(v.context),{},counts()};
                if(run().getMyHero()->goldKnown()){ watch.gold=run().getMyHero()->getGold(); }
                ensureCurrent(v);
                if(v.method=="game_auction_register"){
                    registration().prepareItem(v.params.at("item_id"),v.params.at("seq_id"),
                        std::stoull(v.params.at("price").get<std::string>()),v.params.at("note").get<std::string>());
                    prepared=true;
                    ensureCurrent(v);
                    request=registration().submitRegister();
                }
                else if(v.method=="game_delivery_claim"){
                    const auto record=handles.resolve(v.params.at("delivery_handle").get<std::string>(),v.context,mono()).value.at("record").get<std::string>();
                    ensureCurrent(v);
                    request=inbox().submitDelivery(record);
                }
                else{
                    const auto entry=resolveAuction(v.params,v.context);
                    ensureCurrent(v);
                    request=auction().submitAuction(entry.auctionID,v.method=="game_auction_cancel");
                }
                const bool delivery=v.method=="game_auction_buy" || v.method=="game_auction_cancel";
                watch.request=request; watch.delivery=delivery;
                watches.push_back(std::move(watch));
                if(runtime.game()){ runtime.game()->markChanged(); }
                return economicUpdate(*request,delivery);
            }
            catch(const ProtocolError &e){
                if(prepared){ registration().cancelUnsubmitted(); }
                return {.outcome=OperationOutcome::Failed,.error=error(e.code)};
            }
        }
        std::optional<OperationUpdate> tick(const OperationView &v)
        {
            for(const auto &w:watches){ if(w.id==v.operationId){ return update(w); } }
            return std::nullopt;
        }
        void observe()
        {
            for(auto it=watches.begin();it!=watches.end();){
                if(it->request->receipt==EconomicReceipt::Pending){ ++it; continue; }
                controller().report(it->id,update(*it));
                it=watches.erase(it);
            }
            if(!client.getProcess<ProcessRun>() || controller().context().is_null()){
                if(client.getProcess<ProcessRun>() && !scope.is_null()){
                    const auto gui=dynamic_cast<AuctionBoard *>(client.getProcess<ProcessRun>()->getWidget("AuctionBoard"));
                    if(gui){ gui->invalidateScope(); }
                }
                scope=nullptr; handles.clear(); pages.clear(); return;
            }
            sync();
            const auto &refresh=category>=AUCTIONCAT_BEGIN && category<AUCTIONCAT_END ? auction().refreshState(category) : auction().refreshState(AUCTIONCAT_ALL);
            const auto current=std::to_string(panelRevision)+":"+std::to_string(refresh.revision)+":"+std::to_string(refresh.pending)
                +":"+std::to_string(inbox().deliveryRevision())+":"+std::to_string(watches.size());
            if(current!=fingerprint && lastRender!=client.renderSequence()){
                fingerprint=current; lastRender=client.renderSequence(); controller().notifySnapshot();
            }
        }
    };
    AiAuctionAdapter::AiAuctionAdapter(Client &c,AiBridgeRuntime &r):m_impl(std::make_unique<Impl>(c,r)){}
    AiAuctionAdapter::~AiAuctionAdapter()=default;
    void AiAuctionAdapter::observe(){ m_impl->observe(); }
    void AiAuctionAdapter::registerTools()
    {
        auto &s=*m_impl;
        s.controller().registerQuery("game_auction_list",[&s](const Json &p,const Json &c){ return s.auctionList(p,c); });
        s.controller().registerQuery("game_delivery_list",[&s](const Json &p,const Json &c){ return s.deliveryList(p,c); });
        for(const std::string method:{"game_auction_register","game_auction_buy","game_auction_cancel","game_delivery_claim"}){
            ActionAdapter adapter;
            adapter.validate=[&s,method](const Json &p,const Json &c){
                try{ return s.validate(method,p,c); } catch(const ProtocolError &e){ return ValidationResult{.error=error(e.code)}; }
            };
            adapter.begin=[&s](const OperationView &v){ return s.begin(v); };
            adapter.tick=[&s](const OperationView &v){ return s.tick(v); };
            adapter.cancel=[&s](const OperationView &v){
                for(const auto &w:s.watches){
                    if(w.id==v.operationId){
                        if(w.request->receipt!=EconomicReceipt::Pending){ return s.update(w); }
                        return OperationUpdate{.outcome=w.request->submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled,.submitted=w.request->submitted};
                    }
                }
                return OperationUpdate{.outcome=v.submitted ? OperationOutcome::Unknown : OperationOutcome::Cancelled,.submitted=v.submitted};
            };
            s.controller().registerAction(method,std::move(adapter));
        }
    }
}
