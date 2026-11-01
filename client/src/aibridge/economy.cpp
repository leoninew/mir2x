#include "economy.hpp"
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <chrono>
#include <tinyxml2.h>
#include <charconv>

namespace aibridge
{
    Json economyScope(const Json &context)
    {
        validateDefinition(context, "context");
        auto result = context;
        result.erase("control_epoch");
        return result;
    }
    std::string economyNonce()
    {
        std::random_device random;
        std::ostringstream text;
        text << std::hex << std::setfill('0');
        for(int i = 0; i < 4; ++i){ text << std::setw(8) << uint32_t(random()); }
        return text.str();
    }
    QuoteStore::QuoteStore(std::string nonce): m_nonce(nonce.empty() ? economyNonce() : std::move(nonce))
    {
        if(m_nonce.size() > 32){ throw std::invalid_argument("Quote nonce too long"); }
    }
    QuoteRecord QuoteStore::publish(ShopQuote value, const Json &context, std::string panel,
        uint64_t revision, uint64_t mono, uint64_t unixTime)
    {
        if(m_sequence == UINT64_MAX || unixTime > UINT64_MAX - economyHandleLifetimeMs){
            throw ProtocolError("unavailable", "Quote identity exhausted");
        }
        QuoteRecord record{"quote-" + m_nonce + "-" + std::to_string(++m_sequence), std::move(panel),
            economyScope(context), revision, mono, unixTime + economyHandleLifetimeMs, std::move(value)};
        quoteJson(record);
        if(m_records.size() == capacity){ m_records.pop_front(); }
        m_records.push_back(record);
        return record;
    }
    const QuoteRecord &QuoteStore::resolve(std::string_view handle, const Json &context,
        std::string_view panel, uint64_t revision, uint64_t now) const
    {
        const auto current = economyScope(context);
        for(const auto &record: m_records){
            if(record.handle != handle){ continue; }
            if(record.contextScope != current || record.panelHandle != panel || record.sourceRevision != revision
                || now < record.createdMonotonic || now - record.createdMonotonic >= economyHandleLifetimeMs){ break; }
            return record;
        }
        throw ProtocolError("stale_target", "Quote is no longer current");
    }
    void QuoteStore::clear(){ m_records.clear(); }
    Json quoteJson(const QuoteRecord &record)
    {
        Json materials = Json::array();
        for(const auto &m: record.value.materials){ materials.push_back({{"item_id",m.itemId},{"count",m.count}}); }
        Json result = {{"quote_handle",record.handle},{"item_id",record.value.itemId},{"seq_id",record.value.seqId},
            {"unit_count",record.value.unitCount},{"gold",std::to_string(record.value.gold)},
            {"materials",std::move(materials)},{"expires_at",std::to_string(record.expiresAt)},
            {"direction",record.value.selling ? "sell" : "buy"}};
        validateDefinition(result,"quote");
        return result;
    }
    OperationUpdate economicUpdate(const EconomicRequest &request, bool deliveryPending)
    {
        OperationUpdate result;
        result.submitted = request.submitted;
        switch(request.receipt){
            case EconomicReceipt::Pending: break;
            case EconomicReceipt::Ok:
                result.outcome = OperationOutcome::Succeeded;
                result.completionScope = "server_ack";
                result.evidence.push_back({{"source","server"},{"kind","server_ack"},
                    {"observed_at",std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count())},
                    {"snapshot_seq","0"},{"server_code","SM_OK"},{"description","服务器明确返回 SM_OK"}});
                result.result = {{"delivery_pending",deliveryPending}};
                break;
            case EconomicReceipt::Rejected:
                result.outcome = OperationOutcome::Failed;
                result.error = OperationError{"server_rejected","服务器明确拒绝本次请求",{{"server_code",std::to_string(request.error)}}};
                break;
            case EconomicReceipt::Unknown:
                result.outcome = request.submitted ? OperationOutcome::Unknown : OperationOutcome::Failed;
                result.error = OperationError{request.submitted ? "timeout" : "unavailable",
                    request.submitted ? "结果未知，请人工核对；不会自动重发或返还暂存物品" : "请求未发送",Json::object()};
                break;
        }
        return result;
    }
    std::optional<OperationError> validateEconomicSend(const OperationView &view,const Json &currentContext,bool active,uint64_t now)
    {
        if(!active || currentContext!=view.context){ return OperationError{"stale_context","发送前操作授权或角色上下文已失效",Json::object()}; }
        if(now>=view.deadlineMs){ return OperationError{"timeout","发送前已达到操作期限",Json::object()}; }
        return {};
    }
    bool EconomyRefresh::begin(uint64_t now)
    {
        expire(now);
        if(pending || quarantined){ return false; }
        pending = true; started = now; error.reset();
        return true;
    }
    OperationUpdate tradeUpdate(std::string_view method,TradeReceipt receipt,bool submitted,uint32_t error)
    {
        OperationUpdate result{.submitted=submitted};
        const bool success=(receipt==TradeReceipt::Started && (method=="game_trade_request" || method=="game_trade_respond"))
            || (receipt==TradeReceipt::OfferAccepted && method=="game_trade_offer")
            || (receipt==TradeReceipt::Locked && method=="game_trade_lock")
            || (receipt==TradeReceipt::Complete && method=="game_trade_commit")
            || (receipt==TradeReceipt::Closed && method=="game_trade_cancel");
        if(success){ result.outcome=OperationOutcome::Succeeded; result.completionScope="observed_postcondition"; }
        else if(receipt==TradeReceipt::Closed){
            result.outcome=OperationOutcome::Failed;
            result.error=OperationError{"target_lost","交易已关闭，未观察到完整提交结果",Json::object()};
        }
        if(success || receipt==TradeReceipt::Closed || receipt==TradeReceipt::UnrelatedError){
            result.evidence.push_back({{"source","server"},{"kind","action"},{"observed_at","0"},{"snapshot_seq","0"},
                {"server_code",error ? Json(std::to_string(error)) : Json(nullptr)},
                {"description",receipt==TradeReceipt::UnrelatedError ? "收到未关联的交易错误，不能判定本次请求失败" : "观察到当前交易对应阶段的服务器状态；不代表请求关联回执"}});
        }
        return result;
    }
    void EconomyRefresh::expire(uint64_t now)
    {
        if(pending && (now < started || now - started >= 10000)){
            pending = false; quarantined = true;
            error = OperationError{"timeout","列表刷新超时；为避免误用迟到结果，本分类暂不可再次刷新",Json::object()};
        }
    }
    bool EconomyRefresh::receive(uint64_t now)
    {
        expire(now);
        if(!pending || quarantined){ return false; }
        pending = false; known = true; ++revision; error.reset();
        return true;
    }
    std::string EconomicHandles::publish(Json value, const Json &context, uint64_t revision, uint64_t now)
    {
        if(m_sequence == UINT64_MAX){ throw ProtocolError("unavailable","Handle identity exhausted"); }
        if(m_entries.size() == 16384){ m_entries.pop_front(); }
        auto id="economy-"+m_nonce+"-"+std::to_string(++m_sequence);
        m_entries.push_back({id,economyScope(context),std::move(value),revision,now});
        return id;
    }
    const EconomicHandle &EconomicHandles::resolve(std::string_view handle,const Json &context,uint64_t now) const
    {
        const auto scope=economyScope(context);
        for(const auto &e:m_entries){
            if(e.handle==handle){
                if(e.scope==scope && now>=e.created && now-e.created<economyHandleLifetimeMs){ return e; }
                break;
            }
        }
        throw ProtocolError("stale_target","Economic handle expired");
    }
    Json EconomicPages::page(const Page &p,size_t offset) const
    {
        Json result=p.metadata; result["items"]=Json::array();
        const auto end=std::min(offset+100,p.rows.size());
        for(auto i=offset;i<end;++i){ result["items"].push_back(p.rows[i]); }
        result["next_cursor"]=end<p.rows.size() ? Json(p.id+":"+std::to_string(end)) : Json(nullptr);
        return result;
    }
    Json EconomicPages::first(std::string kind,const Json &context,const Json &filters,Json metadata,std::vector<Json> rows)
    {
        if(rows.size()>8192 || m_sequence==UINT64_MAX){ throw ProtocolError("unavailable","Economic snapshot limit"); }
        size_t count=0;
        for(const auto &p:m_pages){ if(p.kind==kind){ ++count; } }
        if(count>=2){
            const auto found=std::find_if(m_pages.begin(),m_pages.end(),[&](const auto &p){return p.kind==kind;});
            m_pages.erase(found);
        }
        m_pages.push_back({std::move(kind),"ep-"+m_nonce+"-"+std::to_string(++m_sequence),economyScope(context),filters,std::move(metadata),std::move(rows)});
        return page(m_pages.back(),0);
    }
    Json EconomicPages::next(std::string_view kind,const Json &context,const Json &filters,std::string_view cursor) const
    {
        const auto colon=cursor.rfind(':');
        if(colon!=std::string_view::npos){
            size_t offset=0;
            const auto [end,ec]=std::from_chars(cursor.data()+colon+1,cursor.data()+cursor.size(),offset);
            if(ec==std::errc() && end==cursor.data()+cursor.size() && offset && offset%100==0){
                for(const auto &p:m_pages){
                    if(p.kind==kind && p.id==cursor.substr(0,colon) && p.scope==economyScope(context)
                        && p.filters==filters && offset<p.rows.size()){ return page(p,offset); }
                }
            }
        }
        throw ProtocolError("stale_cursor","Economic cursor expired or filters changed");
    }
    std::vector<DeliveryText> systemDeliveries(bool trusted,std::string_view xml)
    {
        std::vector<DeliveryText> result;
        if(!trusted){ return result; }
        if(xml.size()>65536){ throw ProtocolError("unavailable","System message exceeds observation limit"); }
        tinyxml2::XMLDocument doc;
        if(doc.Parse(xml.data(),xml.size())!=tinyxml2::XML_SUCCESS){ return result; }
        std::vector<const tinyxml2::XMLNode *> pending;
        if(doc.RootElement()){ pending.push_back(doc.RootElement()); }
        std::string text;
        size_t visited=0;
        while(!pending.empty()){
            const auto node=pending.back(); pending.pop_back();
            if(++visited>8192){ throw ProtocolError("unavailable","System message node limit"); }
            if(const auto t=node->ToText()){
                if(text.size()+std::char_traits<char>::length(t->Value())>2048){ throw ProtocolError("unavailable","Delivery text exceeds contract"); }
                text+=t->Value();
            }
            if(const auto e=node->ToElement(); e && std::string_view(e->Name())=="event"){
                const auto id=e->Attribute("id"),record=e->Attribute("record");
                if(id && record && std::string_view(id)=="_RSVD_NAME_DELIVERY_5639021748316" && std::string_view(record).size()==32){
                    if(result.size()==100){ throw ProtocolError("unavailable","Delivery record limit"); }
                    result.push_back({record,{}});
                }
            }
            for(auto child=node->LastChild();child;child=child->PreviousSibling()){ pending.push_back(child); }
        }
        for(auto &entry:result){ entry.text=text; }
        return result;
    }
}
