#include "aibridge/economyrequest.hpp"
#include <algorithm>
#include <utility>
#include "strf.hpp"
#include "xmlf.hpp"
#include "colorf.hpp"
#include "client.hpp"
#include "dbcomid.hpp"
#include "pngtexdb.hpp"
#include "sdldevice.hpp"
#include "processrun.hpp"
#include "inputstringboard.hpp"
#include "gui/friendchatboard/friendchatboard.hpp"
#include "auctionregisterboard.hpp"
#include "auctionboard.hpp"
#include "utf8f.hpp"
#include "aibridge/runtime.hpp"

extern PNGTexDB *g_progUseDB;
extern SDLDevice *g_sdlDevice;
extern Client *g_client;

AuctionBoard::AuctionBoard(ProcessRun *argProc, Widget *argParent, bool argAutoDelete, std::optional<Access> access)
    : Widget
      {{
          .dir = DIR_NONE,
          .x = []{ return g_sdlDevice->getWindowLogicalWidth () / 2; },
          .y = []{ return g_sdlDevice->getWindowLogicalHeight() / 2; },
          .w = std::nullopt,
          .h = std::nullopt,
          .parent{argParent, argAutoDelete},
      }}

    , m_runProc(argProc)
    , m_access(access ? std::move(*access) : Access{
        .dbid=[argProc]{ return fflcheck(argProc)->getMyHeroDBID(); },
        .gold=[argProc]{ return fflcheck(argProc)->getMyHero()->getGold(); },
        .query=[](int category){ g_client->send({CM_QUERYAUCTIONITEMLIST,CMQueryAuctionItemList{.category=to_u8(category)}}); },
        .submit=[](uint64_t id,bool cancel){
            return cancel ? aibridge::sendEconomic<SMAuctionUnregisterError>(*g_client,
                {CM_UNREGISTERAUCTIONITEM,CMUnregisterAuctionItem{.auctionID=id}},SM_AUCTIONUNREGISTERERROR)
                : aibridge::sendEconomic<SMAuctionBuyError>(*g_client,
                {CM_BUYAUCTIONITEM,CMBuyAuctionItem{.auctionID=id}},SM_AUCTIONBUYERROR);
        },
        .log=[argProc](const char8_t *message){ if(argProc){argProc->addCBLog(CBLOG_SYS,u8"%s",to_cstr(message));} },
        .clock=[]{ return SDL_GetTicks(); },
        .scope=[]{
            const auto runtime=g_client ? g_client->aiRuntime() : nullptr;
            const auto controller=runtime ? runtime->controller() : nullptr;
            return controller ? controller->context() : aibridge::Json(nullptr);
        },
    })
    , m_background
      {{
          .texLoadFunc = []{ return g_progUseDB->retrieve(0X00001400); },
          .blendMode = SDL_BLENDMODE_NONE,
          .parent{this},
      }}

    , m_itemList
      {{
          .x = 10,
          .y = 82,
          .parent{this},
      }}

    , m_category
      {{
          .dir = DIR_NONE,
          .x = 61,
          .y = 24,
          .textFunc = [this]() -> std::string
          {
              switch(m_itemList.getItemList().category){
                  case AUCTIONCAT_ALL     : return to_cstr(u8"所有物品");
                  case AUCTIONCAT_DRESS   : return to_cstr(u8"衣服");
                  case AUCTIONCAT_WEAPON  : return to_cstr(u8"武器");
                  case AUCTIONCAT_NECKLACE: return to_cstr(u8"项链");
                  case AUCTIONCAT_HELMET  : return to_cstr(u8"头盔");
                  case AUCTIONCAT_RING    : return to_cstr(u8"戒指");
                  case AUCTIONCAT_ARMRING : return to_cstr(u8"手镯");
                  case AUCTIONCAT_SHOES   : return to_cstr(u8"鞋类");
                  case AUCTIONCAT_POTION  : return to_cstr(u8"药品");
                  case AUCTIONCAT_BOOK    : return to_cstr(u8"图书");
                  case AUCTIONCAT_OTHER   : return to_cstr(u8"其他物品");
                  default                 : return to_cstr(u8"寄售");
              }
          },
          .font
          {
              .color = colorf::YELLOW_A255,
          },
          .parent{this},
      }}

    , m_itemCount
      {{
          .dir = DIR_NONE,
          .x = 240,
          .y = 24,
          .textFunc = [this]() -> std::string
          {
              if(const auto firstIndex = m_itemList.firstIndex(); firstIndex.has_value()){
                  const auto lastIndex = std::min<size_t>(firstIndex.value() + AuctionItemList::m_rowCount, m_itemList.size());
                  return to_cstr(str_printf(u8"第%zu-%zu件物品（共%zu件）", firstIndex.value() + 1, lastIndex, m_itemList.size()));
              }
              return to_cstr(str_printf(u8"共%zu件", m_itemList.size()));
          },
          .font
          {
              .color = colorf::YELLOW_A255,
          },
          .parent{this},
      }}

    , m_wmdAniBoard
      {
          DIR_NONE,
          419,
          24,
          this,
      }

    , m_columnItem
      {{
          .dir = DIR_NONE,
          .x = 64,
          .y = 62,
          .textFunc = to_cstr(u8"物品"),
          .font
          {
              .color = colorf::YELLOW_A255,
          },
          .parent{this},
      }}

    , m_columnSeller
      {{
          .dir = DIR_NONE,
          .x = 188,
          .y = 62,
          .textFunc = to_cstr(u8"寄售人"),
          .font
          {
              .color = colorf::YELLOW_A255,
          },
          .parent{this},
      }}

    , m_columnTime
      {{
          .dir = DIR_NONE,
          .x = 293,
          .y = 62,
          .textFunc = to_cstr(u8"剩余"),
          .font
          {
              .color = colorf::YELLOW_A255,
          },
          .parent{this},
      }}

    , m_columnPrice
      {{
          .dir = DIR_NONE,
          .x = 399,
          .y = 62,
          .textFunc = to_cstr(u8"价格"),
          .font
          {
              .color = colorf::YELLOW_A255,
          },
          .parent{this},
      }}

    , m_itemDetailUpper
      {{
          .x = 486,
          .y = 23,

          .onContactSeller = [this](const SDChatPeer &seller)
          {
              auto chatBoard = dynamic_cast<FriendChatBoard *>(m_runProc->getWidget("FriendChatBoard"));
              fflassert(chatBoard);

              setFocus(false);
              chatBoard->openChat(seller);
          },

          .parent{this},
      }}

    , m_itemDetailLower
      {{
          .x = 486,
          .y = 168,
          .parent{this},
      }}

    , m_buttonPrevious
      {{
          .x = 21,
          .y = 360,
          .texIDList
          {
              .off  = 0X00000311,
              .on   = 0X00000310,
              .down = 0X00000312,
          },
          .onTrigger = [this](Widget *, int)
          {
              m_itemList.movePrev();
          },

          .attrs
          {
              .active = [this]{ return m_itemList.canMovePrev(); },
          },
          .parent{this},
      }}

    , m_buttonNext
      {{
          .x = 72,
          .y = 360,
          .texIDList
          {
              .off  = 0X00000301,
              .on   = 0X00000300,
              .down = 0X00000302,
          },
          .onTrigger = [this](Widget *, int)
          {
              m_itemList.moveNext();
          },

          .attrs
          {
              .active = [this]{ return m_itemList.canMoveNext(); },
          },
          .parent{this},
      }}

    , m_buttonRefresh
      {{
          .x = 122,
          .y = 361,
          .texIDList
          {
              .off  = 0X00000331,
              .on   = 0X00000330,
              .down = 0X00000332,
          },
          .onTrigger = [this](Widget *, int)
          {
              refreshItemList();
          },
          .parent{this},
      }}

    , m_buttonItemSearch
      {{
          .x = 412,
          .y = 360,
          .texIDList
          {
              .on   = 0X00001420,
              .down = 0X00001421,
          },
          .onTrigger = [](Widget *, int)
          {
          },
          .parent{this},
      }}

    , m_buttonSellerSearch
      {{
          .x = 412,
          .y = 390,
          .texIDList
          {
              .on   = 0X00001420,
              .down = 0X00001421,
          },
          .onTrigger = [](Widget *, int)
          {
          },
          .parent{this},
      }}

    , m_buttonRegister
      {{
          .x = 513,
          .y = 370,
          .texIDList
          {
              // looks there are two version of register button, in GameInter
              //
              // TMP001228.PNG vs TMP001904.PNG
              // they are identical expect 1 pixel offset
              //
              // zsdb uses 1228, but auction board frame image uses 1904
              // here use zsdb and off texture to redraw the button off state

              .off  = 0X000000B3,
              .on   = 0X000000B3,
              .down = 0X000000B4,
          },
          .onTrigger = [this](Widget *, int)
          {
              closePanel();
              if(auto registerBoardPtr = dynamic_cast<AuctionRegisterBoard *>(m_runProc->getWidget("AuctionRegisterBoard"))){
                  registerBoardPtr->beginRegister();
              }
          },
          .parent{this},
      }}

    , m_buttonBuy
      {{
          .x = 563,
          .y = 370,
          .texIDList
          {
              .on   = 0X000000B7,
              .down = 0X000000B8,
          },
          .onTrigger = [this](Widget *, int)
          {
              confirmBuy();
          },

          .attrs
          {
              .active = [this] -> bool
              {
                  if(m_pending){
                      return false;
                  }

                  if(const auto item = selectedItem()){
                      return item->seller.id != m_access.dbid();
                  }

                  return false;
              },
          },
          .parent{this},
      }}

    , m_buttonCancel
      {{
          .x = 643,
          .y = 370,
          .texIDList
          {
              .off  = 0X00000850, // same as buttonRegister
              .on   = 0X00000850,
              .down = 0X00000851,
          },
          .onTrigger = [this](Widget *, int)
          {
              confirmUnregister();
          },

          .attrs
          {
              .active = [this]
              {
                  if(m_pending){
                      return false;
                  }

                  if(const auto item = selectedItem()){
                      return item->seller.id == m_access.dbid();
                  }

                  return false;
              },
          },
          .parent{this},
      }}

    , m_buttonClose
      {{
          .x = 680,
          .y = 396,
          .texIDList
          {
              .on   = 0X0000001C,
              .down = 0X0000001D,
          },
          .onTrigger = [this](Widget *, int)
          {
              closePanel();
          },
          .parent{this},
      }}
{
    closePanel();
}

bool AuctionBoard::processEventDefault(const SDL_Event &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(const auto oldSelected = m_itemList.selectedIndex(); m_itemList.processEventParent(event, valid, m)){
        if(const auto newSelected = m_itemList.selectedIndex(); oldSelected != newSelected){
            m_itemDetailUpper.setItem(newSelected.has_value() ? m_itemList.indexItem(newSelected.value()) : nullptr);
            m_itemDetailLower.setItem(newSelected.has_value() ? m_itemList.indexItem(newSelected.value()) : nullptr);
        }
        return true;
    }

    if(!valid){
        return consumeFocus(false);
    }

    if(m_itemDetailUpper.show() && m_itemDetailUpper   .processEventParent(event, valid, m)){ return true; }
    if(m_itemDetailLower.show() && m_itemDetailLower   .processEventParent(event, valid, m)){ return true; }
    if(                            m_buttonPrevious    .processEventParent(event, valid, m)){ return true; }
    if(                            m_buttonNext        .processEventParent(event, valid, m)){ return true; }
    if(                            m_buttonRefresh     .processEventParent(event, valid, m)){ return true; }
    if(                            m_buttonItemSearch  .processEventParent(event, valid, m)){ return true; }
    if(                            m_buttonSellerSearch.processEventParent(event, valid, m)){ return true; }
    if(                            m_buttonRegister    .processEventParent(event, valid, m)){ return true; }
    if(                            m_buttonBuy         .processEventParent(event, valid, m)){ return true; }
    if(                            m_buttonCancel      .processEventParent(event, valid, m)){ return true; }
    if(                            m_buttonClose       .processEventParent(event, valid, m)){ return true; }

    switch(event.type){
        case SDL_EVENT_KEY_DOWN:
            {
                if(event.key.key == SDLK_ESCAPE){
                    closePanel();
                    setFocus(false);
                    return true;
                }
                return consumeFocus(false);
            }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                return consumeFocus(m.in(to_d(event.button.x), to_d(event.button.y)));
            }
        case SDL_EVENT_MOUSE_MOTION:
            {
                if((event.motion.state & SDL_BUTTON_LMASK) && (m.in(to_d(event.motion.x), to_d(event.motion.y)) || focus())){
                    const int remapX = m.x - m.ro->x;
                    const int remapY = m.y - m.ro->y;
                    const auto [winLogicalW, winLogicalH] = g_sdlDevice->getWindowLogicalSize();

                    const int newX = std::clamp(remapX + to_d(event.motion.xrel), 0, winLogicalW - w());
                    const int newY = std::clamp(remapY + to_d(event.motion.yrel), 0, winLogicalH - h());
                    moveBy(newX - remapX, newY - remapY);
                    return consumeFocus(true);
                }
                return consumeFocus(false);
            }
        default:
            {
                return consumeFocus(false);
            }
    }
}

void AuctionBoard::setItemList(SDAuctionItemList list)
{
    if(list.category < AUCTIONCAT_BEGIN || list.category >= AUCTIONCAT_END){ return; }
    auto &cache=m_cachedLists[list.category];
    if(!cache.refresh.receive(m_access.clock())){ return; }
    if(list.itemList.size() > 8192){
        cache.refresh.known=false;
        cache.refresh.error=aibridge::OperationError{"unavailable","拍卖列表超过客户端有界处理上限",aibridge::Json::object()};
        return;
    }
    for(const auto &entry:list.itemList){
        if(!entry.auctionID || !entry.item || !DBCOM_ITEMRECORD(entry.item.itemID)
            || !entry.seller.player() || !entry.seller.id || entry.seller.id>SYS_MAXDBID
            || !entry.price || entry.price>uint64_t(INT64_MAX) || entry.item.count>UINT32_MAX
            || entry.note.size()>128 || !utf8f::valid(entry.note)){
            cache.refresh.known=false;
            cache.refresh.error=aibridge::OperationError{"unavailable","拍卖数据超出当前观察契约",aibridge::Json::object()};
            return;
        }
    }
    cache.list=list;
    if(m_requestedCategory != list.category){ return; }
    m_itemList.setItemList(std::move(list));
    m_itemDetailUpper.setItem(nullptr); m_itemDetailLower.setItem(nullptr);
    setShow(true);
}

const SDAuctionItem *AuctionBoard::selectedItem() const
{
    const auto selectedIndex = m_itemList.selectedIndex();
    return selectedIndex.has_value() ? m_itemList.indexItem(selectedIndex.value()) : nullptr;
}

void AuctionBoard::refreshItemList()
{
    const auto category=m_itemList.getItemList().category;
    requestList(category >= AUCTIONCAT_BEGIN && category < AUCTIONCAT_END ? category : AUCTIONCAT_ALL);
}

void AuctionBoard::confirmBuy()
{
    if(m_pending){
        return;
    }

    const auto item = selectedItem();
    if(!item){
        m_runProc->addCBLog(CBLOG_ERR, u8"请先选择要购买的寄售物品");
        return;
    }

    if(item->seller.id == m_access.dbid()){
        m_runProc->addCBLog(CBLOG_ERR, u8"不能购买自己寄售的物品");
        return;
    }

    if(item->price > m_access.gold()){
        m_runProc->addCBLog(CBLOG_ERR, u8"金币不够");
        return;
    }

    const auto &ir = DBCOM_ITEMRECORD(item->item.itemID);
    fflassert(ir);

    auto inputBoard = dynamic_cast<InputStringBoard *>(m_runProc->getWidget("InputStringBoard"));
    fflassert(inputBoard);

    std::string prompt = "<layout>";
    prompt += xmlf::toParString("你确定花费%s金币购买%s吗？", str_ksep(item->price).c_str(), to_cstr(ir.name));
    prompt += xmlf::toParString("%s", to_cstr(u8"购买成功后，物品会通过系统投递发送。"));
    prompt += "</layout>";

    inputBoard->waitChoice(to_u8rawstr(prompt), false, [auctionID = item->auctionID, itemName = std::string(to_cstr(ir.name)), this]
    {
        buyItem(auctionID, itemName);
    });
}

void AuctionBoard::buyItem(uint64_t auctionID, std::string)
{
    try{ submitAuction(auctionID, false); }
    catch(const aibridge::ProtocolError &){ m_runProc->addCBLog(CBLOG_ERR, u8"当前物品不可购买，请刷新或核对交易状态"); }
}

void AuctionBoard::confirmUnregister()
{
    if(m_pending){
        return;
    }

    const auto item = selectedItem();
    if(!item){
        m_runProc->addCBLog(CBLOG_ERR, u8"请先选择要下架的寄售物品");
        return;
    }

    if(item->seller.id != m_access.dbid()){
        m_runProc->addCBLog(CBLOG_ERR, u8"只能下架自己寄售的物品");
        return;
    }

    const auto &ir = DBCOM_ITEMRECORD(item->item.itemID);
    fflassert(ir);

    auto inputBoard = dynamic_cast<InputStringBoard *>(m_runProc->getWidget("InputStringBoard"));
    fflassert(inputBoard);

    std::string prompt = "<layout>";
    prompt += xmlf::toParString("你确定下架%s吗？", to_cstr(ir.name));
    prompt += xmlf::toParString("%s", to_cstr(u8"下架后，物品会通过系统投递退回。"));
    prompt += "</layout>";

    inputBoard->waitChoice(to_u8rawstr(prompt), false, [auctionID = item->auctionID, itemName = std::string(to_cstr(ir.name)), this]
    {
        unregisterItem(auctionID, itemName);
    });
}

void AuctionBoard::unregisterItem(uint64_t auctionID, std::string)
{
    try{ submitAuction(auctionID, true); }
    catch(const aibridge::ProtocolError &){ m_runProc->addCBLog(CBLOG_ERR, u8"当前物品不可下架，请刷新或核对交易状态"); }
}

bool AuctionBoard::uncertain() const noexcept
{
    return m_economicRequest && m_economicRequest->submitted
        && m_economicRequest->receipt == aibridge::EconomicReceipt::Unknown;
}

std::shared_ptr<aibridge::EconomicRequest> AuctionBoard::submitAuction(uint64_t auctionID, bool cancel)
{
    updateDefault(0);
    if(m_pending || uncertain()){ throw aibridge::ProtocolError("busy", "Auction outcome is unresolved"); }
    const auto &items = m_itemList.getItemList().itemList;
    const auto found = std::find_if(items.begin(), items.end(), [auctionID](const auto &i){ return i.auctionID == auctionID; });
    if(found == items.end()){ throw aibridge::ProtocolError("stale_target", "Auction is not in current list"); }
    if((found->seller.id == m_access.dbid()) != cancel){
        throw aibridge::ProtocolError("policy_denied", "Auction ownership mismatch");
    }
    if(!cancel && found->price > m_access.gold()){
        throw aibridge::ProtocolError("budget_exceeded", "Not enough gold");
    }
    const auto index = size_t(found - items.begin());
    m_itemList.setFirstIndex(index);
    m_itemList.setSelectedRow(0,true);
    m_itemDetailUpper.setItem(&*found); m_itemDetailLower.setItem(&*found);
    setShow(true);
    m_pending = true; m_economicSettled = false;
    m_economicRequest=m_access.submit(auctionID,cancel);
    return m_economicRequest;
}

void AuctionBoard::updateDefault(double ms)
{
    Widget::updateDefault(ms);
    for(auto &[category, cache]: m_cachedLists){ cache.refresh.expire(m_access.clock()); }
    if(!m_economicRequest || m_economicSettled || m_economicRequest->receipt == aibridge::EconomicReceipt::Pending){ return; }
    m_economicSettled = true; m_pending = false;
    if(m_economicRequest->receipt == aibridge::EconomicReceipt::Ok){
        m_access.log(u8"拍卖操作已确认，物品等待系统投递");
        const auto cat=m_itemList.getItemList().category;
        auto &cache=m_cachedLists[cat];
        cache.refresh.known=false; ++cache.refresh.revision;
        m_itemDetailUpper.setItem(nullptr); m_itemDetailLower.setItem(nullptr);
        m_itemList.setItemList(SDAuctionItemList{.category=cat});
    }
    else if(uncertain()){ m_access.log(u8"拍卖结果未知，请人工核对；不会自动重发"); }
    else{ m_access.log(u8"服务器拒绝拍卖操作"); }
}

bool AuctionBoard::requestList(int category)
{
    if(category < AUCTIONCAT_BEGIN || category >= AUCTIONCAT_END){
        throw aibridge::ProtocolError("invalid_argument", "Invalid auction category");
    }
    auto &cache=m_cachedLists[category];
    m_requestedCategory=category;
    if(!cache.refresh.begin(m_access.clock())){ return false; }
    const auto context=m_access.scope ? m_access.scope() : aibridge::Json(nullptr);
    cache.scope=context.is_null() ? context : aibridge::economyScope(context);
    try{ m_access.query(category); }
    catch(...){ cache.refresh.pending=false; cache.refresh.error=aibridge::OperationError{"unavailable","列表请求发送失败",aibridge::Json::object()}; }
    return cache.refresh.pending;
}
const SDAuctionItemList *AuctionBoard::cachedList(int category) const
{
    const auto found=m_cachedLists.find(category);
    return found != m_cachedLists.end() && found->second.refresh.known ? &found->second.list : nullptr;
}
void AuctionBoard::showCategory(int category)
{
    if(category<AUCTIONCAT_BEGIN || category>=AUCTIONCAT_END){ throw aibridge::ProtocolError("invalid_argument","Invalid auction category"); }
    m_requestedCategory=category;
    if(const auto list=cachedList(category)){
        m_itemList.setItemList(*list);
        m_itemDetailUpper.setItem(nullptr); m_itemDetailLower.setItem(nullptr);
    }
    else{
        m_itemList.setItemList(SDAuctionItemList{.category=category});
        m_itemDetailUpper.setItem(nullptr); m_itemDetailLower.setItem(nullptr);
    }
    setShow(true);
}
void AuctionBoard::closePanel()
{
    ++m_panelRevision;
    setShow(false);
}
void AuctionBoard::invalidateScope(const aibridge::Json &context)
{
    const auto next=context.is_null() ? context : aibridge::economyScope(context);
    const auto current=m_cachedLists.find(m_requestedCategory);
    const bool preserve=!next.is_null() && current!=m_cachedLists.end() && current->second.scope==next;
    if(!preserve){ closePanel(); m_requestedCategory=0; }
    for(auto &[cat,cache]:m_cachedLists){
        if(!next.is_null() && cache.scope==next){ continue; }
        if(cache.refresh.pending){ cache.refresh.quarantined=true; cache.refresh.pending=false; }
        cache.refresh.known=false; ++cache.refresh.revision; cache.list.itemList.clear();
    }
    if(!preserve){
        m_itemList.setItemList(SDAuctionItemList{.category=AUCTIONCAT_ALL});
        m_itemDetailUpper.setItem(nullptr); m_itemDetailLower.setItem(nullptr);
    }
}
const aibridge::EconomyRefresh &AuctionBoard::refreshState(int category)
{
    if(category < AUCTIONCAT_BEGIN || category >= AUCTIONCAT_END){ throw aibridge::ProtocolError("invalid_argument","Invalid auction category"); }
    auto &state=m_cachedLists[category].refresh; state.expire(m_access.clock()); return state;
}
