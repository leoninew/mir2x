#include <algorithm>
#include <charconv>
#include <limits>
#include <utility>
#include <map>
#include "colorf.hpp"
#include "invpack.hpp"
#include "layoutboard.hpp"
#include "pngtexdb.hpp"
#include "sdldevice.hpp"
#include "processrun.hpp"
#include "directtradeboard.hpp"

extern PNGTexDB *g_progUseDB;
extern SDLDevice *g_sdlDevice;

DirectTradeBoard::DirectTradeBoard(DirectTradeBoard::InitArgs args)
    : Widget
      {{
          .dir = std::move(args.dir),

          .x = std::move(args.x),
          .y = std::move(args.y),

          .w = std::nullopt,
          .h = std::nullopt,
          .parent = std::move(args.parent),
      }}

    , m_runProc(args.runProc)
    , m_access(args.access ? std::move(*args.access) : Access{
          .inventory=[run=fflcheck(args.runProc)]() -> InvPack * { auto hero=run->getMyHero(); return hero ? &hero->getInvPack() : nullptr; },
          .gold=[run=args.runProc]() -> std::optional<uint64_t> { if(auto hero=run->getMyHero()){ return hero->getGold(); } return {}; },
          .offer=[run=args.runProc](uint64_t peer,uint32_t gold,bool locked,const std::vector<SDItem> &items){ return run->updateDirectTradeOffer(peer,gold,locked,items); },
          .commit=[run=args.runProc](uint64_t peer){ run->commitDirectTrade(peer); },
          .cancel=[run=args.runProc](uint64_t peer){ run->cancelDirectTrade(peer); },
          .localName=[run=args.runProc]{ return run->getMyHeroChatPeer().name; },
          .itemTooLarge=[run=args.runProc]{ run->addCBLog(CBLOG_ERR,u8"该物品过大，无法放入交易栏"); },
      })

    , m_background
      {{
          .texLoadFunc = []{ return g_progUseDB->retrieve(0X00002010); },
          .blendMode = SDL_BLENDMODE_NONE,
          .parent{this},
      }}

    , m_peerName
      {{
          .dir = DIR_NONE,
          .x = 178,
          .y =  20,

          .textFunc = [this]{ return m_peerNameStr; },
          .font
          {
              .color = colorf::YELLOW_A255,
          },

          .parent{this},
      }}

    , m_peerState
      {{
          .dir = DIR_NONE,
          .x = 80,
          .y = 20,

          .textFunc = [this]
          {
              if(m_peerConfirmed){
                  return to_cstr(u8"已确认");
              }

              if(m_peerLocked){
                  return to_cstr(u8"已锁定");
              }

              return to_cstr(u8"编辑中");
          },

          .font
          {
              .color = [this]
              {
                  if(m_peerConfirmed){
                      return colorf::RED_A255;
                  }

                  if(m_peerLocked){
                      return colorf::GREEN_A255;
                  }

                  return colorf::GREY_A255;
              },
          },

          .parent{this},
      }}

    , m_peerGoldBoard
      {{
          .dir = DIR_NONE,
          .x = 124,
          .y = 300,

          .textFunc = [this]{ return str_ksep(m_peerGold, ','); },
          .font
          {
              .color = colorf::YELLOW_A255,
          },

          .parent{this},
      }}

    , m_peerItemList
      {{
          .x = 13,
          .y = 47,
          .parent{this},
      }}

    , m_name
      {{
          .dir = DIR_NONE,
          .x = 320,
          .y =  20,

          .textFunc = [this]
          {
              return m_access.localName();
          },

          .font
          {
              .color = colorf::YELLOW_A255,
          },

          .parent{this},
      }}

    , m_state
      {{
          .dir = DIR_NONE,
          .x = 415,
          .y =  20,

          .textFunc = [this] -> const char *
          {
              if(m_confirmed){
                  return to_cstr(u8"已确认");
              }

              if(m_locked){
                  return to_cstr(u8"已锁定");
              }

              if(m_lockPending){
                  return to_cstr(u8"锁定中");
              }

              return to_cstr(u8"编辑中");
          },

          .font
          {
              .color = [this]
              {
                  if(m_confirmed){
                      return colorf::RED_A255;
                  }

                  if(m_locked){
                      return colorf::GREEN_A255;
                  }

                  if(m_lockPending){
                      return colorf::YELLOW_A255;
                  }

                  return colorf::GREY_A255;
              },
          },

          .parent{this},
      }}

    , m_goldInput
      {{
          .dir = DIR_NONE,

          .x = 375,
          .y = 300,

          .w = 96,
          .h = 20,

          .align = DIR_NONE,
          .font
          {
              .color = colorf::YELLOW_A255,
          },

          .onChange = [this](std::string)
          {
              if(m_editing){ return; }
              m_confirmed = false;
              syncLocalOffer(false);
          },

          .validate = [](const std::string &currentInput, const std::string &newInput)
          {
              if(newInput.find_first_not_of("0123456789") != std::string::npos){
                  return false;
              }

              try{
                  if(const auto val = std::stoll(currentInput + newInput); val >= 0 && val <= INT_MAX){
                      return true;
                  }
              }
              catch(...){}
              return false;
          },

          .parent{this},
      }}

    , m_itemList
      {{
          .x = 264,
          .y =  47,

          .onClick = [this](DirectTradeItemList::ClickEvent event)
          {
              onLocalItemClick(event);
          },
          .parent{this},
      }}

    , m_buttonTrade
      {{
          .x = 227,
          .y = 278,

          .texIDFunc = [this](int state) -> std::optional<uint32_t>
          {
              if(m_locked) return std::array{0X00000190, 0X00000190, 0X00000191}[state];
              else         return std::array{0X000000B5, 0X000000B5, 0X000000B6}[state];
          },

          .onTrigger = [this](Widget *, int)
          {
              if(!m_locked){
                  lockOffer();
                  return;
              }
              confirmTrade();
          },

          .attrs
          {
              .active = [this]
              {
                  if(m_closePending || m_offerPending){ return false; }
                  if(m_locked){
                      return m_peerLocked && !m_confirmed;
                  }
                  else{
                      return !m_lockPending && parsedLocalGold().has_value();
                  }
              },
          },

          .parent{this},
      }}

    , m_buttonClose
      {{
          .x = 454,
          .y = 282,

          .texIDList
          {
              .off  = 0X0000001C,
              .on   = 0X0000001C,
              .down = 0X0000001D,
          },

          .onTrigger = [this](Widget *, int){ closeTrade(); },
          .parent{this},
      }}
{
    fflassert(m_access.inventory && m_access.gold && m_access.offer && m_access.commit
        && m_access.cancel && m_access.localName && m_access.itemTooLarge);
    m_goldInput.setInput("0");
    m_goldInput.setActive([this]{ return !m_locked && !m_lockPending && !m_offerPending && !m_closePending; });

    setShow(false);
    setSize([this]{ return m_background.w(); },
            [this]{ return m_background.h(); });
}

void DirectTradeBoard::drawDefault(Widget::ROIMap m) const
{
    if(!m.calibrate(this)){
        return;
    }

    Widget::drawDefault(m);

    if(const auto item = m_itemList.hoveredItem()){
        drawItemHoverText(*item);
    }

    else if(const auto item = m_peerItemList.hoveredItem()){
        drawItemHoverText(*item);
    }
}

bool DirectTradeBoard::processEventDefault(const SDL_Event &event, bool valid, Widget::ROIMap m)
{
    if(!m.calibrate(this)){
        return false;
    }

    if(!valid){
        return consumeFocus(false);
    }

    if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE){
        closeTrade();
        return true;
    }

    if(Widget::processEventDefault(event, valid, m)){
        return true;
    }

    switch(event.type){
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                m_dragging = event.button.button == SDL_BUTTON_LEFT && m.in(to_d(event.button.x), to_d(event.button.y));
                return consumeFocus(m_dragging);
            }
        case SDL_EVENT_MOUSE_BUTTON_UP:
            {
                return std::exchange(m_dragging, false) ? consumeFocus(m.in(to_d(event.button.x), to_d(event.button.y))) : false;
            }
        case SDL_EVENT_MOUSE_MOTION:
            {
                if(m_dragging && (event.motion.state & SDL_BUTTON_LMASK)){
                    const int remapX = m.x - m.ro->x;
                    const int remapY = m.y - m.ro->y;
                    const auto [winLogicalW, winLogicalH] = g_sdlDevice->getWindowLogicalSize();

                    const int newX = std::clamp(remapX + to_d(event.motion.xrel), 0, winLogicalW - w());
                    const int newY = std::clamp(remapY + to_d(event.motion.yrel), 0, winLogicalH - h());
                    moveBy(newX - remapX, newY - remapY);
                    return consumeFocus(true);
                }
                return false;
            }
        default:
            {
                return false;
            }
    }
}

void DirectTradeBoard::beginTrade(uint64_t peerUID, std::string peerName)
{
    if(m_confirmed){ m_itemList.clear(); }
    else { restoreLocalItems(); }
    m_peerItemList.clear();

    m_peerUID = peerUID;
    m_peerNameStr = std::move(peerName);
    m_peerGold = 0;
    m_locked = false;
    m_lockPending = false;
    m_peerLocked = false;
    m_confirmed = false;
    m_peerConfirmed = false;
    m_offerPending = m_closePending = false;
    ++m_generation;
    m_localRevision = m_peerRevision = 0;
    m_localAck = {};

    m_editing = true;
    m_goldInput.setInput("0");
    m_editing = false;

    flipShow(true);
    setFocus(true);
}

void DirectTradeBoard::closeTrade(bool notify)
{
    // Only the server's close notification proves that reserved items can return.
    if(notify && m_peerUID){ requestClose(); return; }

    restoreLocalItems();
    m_peerItemList.clear();

    m_dragging = false;
    m_locked = false;
    m_lockPending = false;
    m_peerLocked = false;
    m_confirmed = false;
    m_peerConfirmed = false;
    m_peerGold = 0;
    m_peerUID = 0;
    m_peerNameStr.clear();
    m_offerPending = m_closePending = false;
    ++m_generation;

    m_editing = true;
    m_goldInput.setInput("0");
    m_editing = false;
    m_goldInput.setFocus(false);

    flipShow(false);
    setFocus(false);
}

void DirectTradeBoard::completeTrade()
{
    if(auto pack = m_access.inventory()){
        pack->setGrabbedItem({});
    }

    m_itemList.clear();
    m_peerItemList.clear();

    m_dragging = false;
    m_locked = false;
    m_lockPending = false;
    m_peerLocked = false;
    m_confirmed = false;
    m_peerConfirmed = false;
    m_peerGold = 0;
    m_peerUID = 0;
    m_peerNameStr.clear();
    m_offerPending = m_closePending = false;
    ++m_generation;

    m_editing = true;
    m_goldInput.setInput("0");
    m_editing = false;
    m_goldInput.setFocus(false);

    flipShow(false);
    setFocus(false);
}

uint32_t DirectTradeBoard::localGold() const
{
    return parsedLocalGold().value_or(0);
}

void DirectTradeBoard::setLocalLocked(bool locked)
{
    m_locked = locked;
    m_lockPending = false;
    if(!locked){
        m_confirmed = false;
    }
    else{
        m_goldInput.setFocus(false);
    }
}

void DirectTradeBoard::setPeerOffer(SDDirectTradeOffer offer)
{
    ++m_peerRevision;
    m_peerGold = offer.gold;
    m_peerItemList.setItemList(std::move(offer.itemList));
    m_peerLocked = offer.locked;
    m_peerConfirmed = offer.confirmed;
}

void DirectTradeBoard::applyLocalOfferAck(const SDDirectTradeOffer &offer)
{
    // A delayed pre-commit snapshot must not reopen the confirm button.
    if(m_confirmed && !offer.confirmed){ return; }
    if(!localOfferMatches(offer)){
        return;
    }

    m_localAck = offer;
    ++m_localRevision;
    m_offerPending = false;

    if(offer.locked){
        setLocalLocked(true);
        m_confirmed = offer.confirmed;
    }
    else if(!m_lockPending){
        setLocalLocked(false);
    }
}

void DirectTradeBoard::rejectLocalOffer()
{
    // The error packet has no request identity; it cannot undo a submitted commit.
    if(m_confirmed){ return; }
    m_offerPending = false;
    m_lockPending = false;
}

std::optional<size_t> DirectTradeBoard::parsedLocalGold(bool checkGold) const
{
    if(const auto input = m_goldInput.getRawString(); input.empty()){
        return 0;
    }

    else{
        long long val = 0;
        for(char c: input){
            if(c < '0' || c > '9'){
                return {};
            }

            val = val * 10 + (c - '0');
            if(val > INT_MAX){
                return {};
            }

            if(checkGold){
                if(const auto gold=m_access.gold(); !gold || uint64_t(val) > *gold){
                    return {};
                }
            }
        }
        return to_uz(val);
    }
}

bool DirectTradeBoard::localOfferMatches(const SDDirectTradeOffer &offer) const
{
    const auto gold = parsedLocalGold();
    const auto itemList = m_itemList.itemList();
    if(!gold || offer.gold != gold.value() || offer.itemList.size() != itemList.size()){
        return false;
    }

    for(size_t i = 0; i < itemList.size(); ++i){
        const auto &lhs = itemList.at(i);
        const auto &rhs = offer.itemList.at(i);
        if(lhs.itemID != rhs.itemID || lhs.seqID != rhs.seqID || lhs.count != rhs.count){
            return false;
        }
    }
    return true;
}

bool DirectTradeBoard::syncLocalOffer(bool locked, const SendGuard &canSend)
{
    if(!m_peerUID || m_offerPending || m_closePending || m_confirmed){
        return false;
    }

    const auto gold = parsedLocalGold();
    if(!gold){
        return false;
    }

    const auto items=m_itemList.itemList();
    if(canSend && !canSend()){ return false; }
    if(!m_access.offer(m_peerUID, gold.value(), locked, items)){ return false; }
    m_offerPending = true;
    return true;
}

void DirectTradeBoard::onLocalItemClick(DirectTradeItemList::ClickEvent event)
{
    if(m_locked || m_lockPending || m_offerPending || m_closePending){
        return;
    }

    auto &invPack = *fflcheck(m_access.inventory());
    const auto grabbedItem = invPack.getGrabbedItem();

    if(grabbedItem){
        if(event.packBinIndex >= 0){
            const auto selectedBin = m_itemList.takeItem(to_uz(event.packBinIndex));
            if(!m_itemList.addItem(grabbedItem, selectedBin.x, selectedBin.y)){
                fflassert(m_itemList.addItem(selectedBin.item, selectedBin.x, selectedBin.y));
                m_access.itemTooLarge();
                return;
            }
            invPack.setGrabbedItem(selectedBin.item);
        }
        else{
            const auto [gridW, gridH] = InvPack::getPackBinSize(grabbedItem.itemID);
            if(!m_itemList.addItem(grabbedItem, event.gridX - gridW / 2, event.gridY - gridH / 2)){
                m_access.itemTooLarge();
                return;
            }
            invPack.setGrabbedItem({});
        }

        m_confirmed = false;
        syncLocalOffer(false);
        return;
    }

    if(event.packBinIndex >= 0){
        invPack.setGrabbedItem(m_itemList.takeItem(to_uz(event.packBinIndex)).item);
        m_confirmed = false;
        syncLocalOffer(false);
    }
}

bool DirectTradeBoard::lockOffer(const SendGuard &canSend)
{
    if(m_locked || m_lockPending || m_offerPending || m_closePending || !localOfferAcknowledged()){ return false; }
    if(!syncLocalOffer(true,canSend)){ return false; }
    m_lockPending = true;
    m_goldInput.setFocus(false);
    return true;
}

bool DirectTradeBoard::confirmTrade(const SendGuard &canSend)
{
    if(!m_peerUID || !m_locked || !m_peerLocked || m_confirmed || m_offerPending || m_closePending || !localOfferAcknowledged()){ return false; }
    if(canSend && !canSend()){ return false; }
    m_confirmed = true;
    m_access.commit(m_peerUID);
    return true;
}

bool DirectTradeBoard::requestClose(const SendGuard &canSend)
{
    if(!m_peerUID || m_closePending){ return false; }
    if(canSend && !canSend()){ return false; }
    m_closePending = true;
    m_goldInput.setFocus(false);
    m_access.cancel(m_peerUID);
    return true;
}

bool DirectTradeBoard::replaceOffer(uint32_t gold, const std::vector<SDDirectTradeItem> &requested, const SendGuard &canSend)
{
    if(!m_peerUID || m_locked || m_lockPending || m_offerPending || m_closePending || m_confirmed
        || requested.size() > 100 || gold > INT_MAX){ return false; }
    const auto availableGold=m_access.gold();
    auto packPtr=m_access.inventory();
    if(!packPtr || !availableGold || gold > *availableGold){ return false; }
    auto &pack = *packPtr;
    if(pack.getGrabbedItem() || pack.getPackBinList().size() > 8192){ return false; }

    using Key = std::pair<uint32_t, uint32_t>;
    std::map<Key, SDItem> available;
    const auto include = [&available](const SDItem &item){
        const Key key{item.itemID, item.seqID};
        auto [it, inserted] = available.emplace(key, item);
        if(!inserted){ it->second.count += item.count; }
    };
    for(const auto &bin: pack.getPackBinList()){ include(bin.item); }
    for(const auto &item: m_itemList.itemList()){ include(item); }
    std::vector<SDItem> next;
    std::map<Key, bool> seen;
    for(const auto &entry: requested){
        const Key key{entry.itemID, entry.seqID};
        const auto found = available.find(key);
        if(!entry.count || !seen.emplace(key, true).second || found == available.end()
            || entry.count > found->second.count){ return false; }
        auto item = found->second;
        item.count = entry.count;
        if(std::get<0>(InvPack::getPackBinSize(item.itemID)) > 5){ return false; }
        next.push_back(std::move(item));
    }
    if(canSend && !canSend()){ return false; }
    const auto oldItems=m_itemList.itemList();
    const auto oldGold=m_goldInput.getRawString();
    // Validate the entire replacement before changing either reservation container.
    restoreLocalItems();
    for(const auto &item: next){ fflassert(pack.remove(item) == item.count); }
    m_itemList.setItemList(std::move(next));
    m_editing = true;
    m_goldInput.setInput(std::to_string(gold).c_str());
    m_editing = false;
    m_confirmed = false;
    if(syncLocalOffer(false,canSend)){ return true; }
    // No request was sent: restore only this preparation, never a submitted offer.
    restoreLocalItems();
    for(const auto &item:oldItems){ fflassert(pack.remove(item)==item.count); }
    m_itemList.setItemList(oldItems);
    m_editing=true;
    m_goldInput.setInput(oldGold.c_str());
    m_editing=false;
    return false;
}

void DirectTradeBoard::restoreLocalItems()
{
    auto itemList = m_itemList.takeItemList();
    if(itemList.empty()){
        return;
    }

    if(auto pack = m_access.inventory()){
        for(auto &item: itemList){
            pack->add(std::move(item), false);
        }
    }
}

void DirectTradeBoard::drawItemHoverText(const SDItem &item) const
{
    const LayoutBoard hoverTextBoard
    {{
        .lineWidth = 200,
        .initXML = to_cstr(item.getXMLLayout().c_str()),
        .lineAlign = LALIGN_JUSTIFY,
    }};

    const auto [mousePX, mousePY] = g_sdlDevice->getMousePLoc();
    const int textBoxW = std::max<int>(hoverTextBoard.w(), 200) + 20;
    const int textBoxH = hoverTextBoard.h() + 20;

    const int drawBoardPX = mathf::bound<int>(mousePX, 0, g_sdlDevice->getWindowLogicalWidth () - textBoxW);
    const int drawBoardPY = mathf::bound<int>(mousePY, 0, g_sdlDevice->getWindowLogicalHeight() - textBoxH);

    g_sdlDevice->fillRectangle(colorf::RGBA(0, 0, 0, 200), drawBoardPX, drawBoardPY, textBoxW, textBoxH, 5);
    g_sdlDevice->drawRectangle(colorf::RGBA(231, 231, 189, 200), drawBoardPX, drawBoardPY, textBoxW, textBoxH, 5);
    hoverTextBoard.draw({.x = drawBoardPX + 10, .y = drawBoardPY + 10});
}
