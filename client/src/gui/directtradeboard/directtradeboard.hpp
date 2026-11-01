#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "widget.hpp"
#include "imageboard.hpp"
#include "inputline.hpp"
#include "textboard.hpp"
#include "tritexbutton.hpp"
#include "directtradeitemlist.hpp"

class ProcessRun;
class InvPack;
class DirectTradeBoard final: public Widget
{
    public:
        struct Access
        {
            std::function<InvPack *()> inventory;
            std::function<std::optional<uint64_t>()> gold;
            std::function<bool(uint64_t, uint32_t, bool, const std::vector<SDItem> &)> offer;
            std::function<void(uint64_t)> commit, cancel;
            std::function<std::string()> localName;
            std::function<void()> itemTooLarge;
        };
        struct InitArgs final
        {
            Widget::VarDir dir = DIR_UPLEFT;

            Widget::VarInt x = 0;
            Widget::VarInt y = 0;

            ProcessRun *runProc = nullptr;
            Widget::WADPair parent {};
            std::optional<Access> access;
        };

    private:
        ProcessRun *m_runProc;
        Access m_access;

    private:
        bool m_dragging = false;

    private:
        bool m_lockPending = false;
        bool m_locked      = false;
        bool m_confirmed   = false;

    private:
        bool m_peerLocked    = false;
        bool m_peerConfirmed = false;
        bool m_offerPending = false;
        bool m_closePending = false;
        bool m_editing = false;
        uint64_t m_generation = 0;
        uint64_t m_localRevision = 0;
        uint64_t m_peerRevision = 0;
        SDDirectTradeOffer m_localAck;

    private:
        size_t      m_peerGold = 0;
        uint64_t    m_peerUID  = 0;
        std::string m_peerNameStr;

    private:
        ImageBoard m_background;

    private:
        TextBoard m_peerName;
        TextBoard m_peerState;
        TextBoard m_peerGoldBoard;
        DirectTradeItemList m_peerItemList;

    private:
        TextBoard m_name;
        TextBoard m_state;
        InputLine m_goldInput;
        DirectTradeItemList m_itemList;

    private:
        TritexButton m_buttonTrade;
        TritexButton m_buttonClose;

    public:
        explicit DirectTradeBoard(DirectTradeBoard::InitArgs);

    public:
        void drawDefault(Widget::ROIMap) const override;
        bool processEventDefault(const SDL_Event &, bool, Widget::ROIMap) override;

    public:
        void    beginTrade(uint64_t, std::string);
        void    closeTrade(bool = true);
        void completeTrade();

    public:
        uint64_t peerUID() const
        {
            return m_peerUID;
        }

    public:
        uint32_t localGold() const;

        std::vector<SDItem> localItemList() const
        {
            return m_itemList.itemList();
        }

        bool localLocked() const
        {
            return m_locked;
        }

        bool peerLocked() const
        {
            return m_peerLocked;
        }

        void setLocalLocked(bool);
        void setPeerOffer(SDDirectTradeOffer);
        void applyLocalOfferAck(const SDDirectTradeOffer &);
        void rejectLocalOffer();
        using SendGuard = std::function<bool()>;
        bool replaceOffer(uint32_t gold, const std::vector<SDDirectTradeItem> &, const SendGuard & = {});
        bool lockOffer(const SendGuard & = {});
        bool confirmTrade(const SendGuard & = {});
        bool requestClose(const SendGuard & = {});
        uint64_t generation() const noexcept { return m_generation; }
        uint64_t localRevision() const noexcept { return m_localRevision; }
        uint64_t peerRevision() const noexcept { return m_peerRevision; }
        bool offerPending() const noexcept { return m_offerPending || m_lockPending; }
        bool closePending() const noexcept { return m_closePending; }
        bool localConfirmed() const noexcept { return m_confirmed; }
        bool peerConfirmed() const noexcept { return m_peerConfirmed; }
        const SDDirectTradeOffer &localAck() const noexcept { return m_localAck; }
        bool localOfferAcknowledged() const { return !offerPending() && localOfferMatches(m_localAck); }
        uint64_t peerGold() const noexcept { return m_peerGold; }
        std::vector<SDItem> peerItems() const { return m_peerItemList.itemList(); }

    private:
        std::optional<size_t> parsedLocalGold(bool = true) const;
        bool localOfferMatches(const SDDirectTradeOffer &) const;
        bool syncLocalOffer(bool, const SendGuard & = {});

        void onLocalItemClick(DirectTradeItemList::ClickEvent);
        void restoreLocalItems();
        void drawItemHoverText(const SDItem &) const;
};
