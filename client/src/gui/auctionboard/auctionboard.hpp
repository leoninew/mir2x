#pragma once
#include <string>
#include "widget.hpp"
#include "imageboard.hpp"
#include "textboard.hpp"
#include "wmdaniboard.hpp"
#include "tritexbutton.hpp"
#include "auctionitemlist.hpp"
#include "auctionitemdetailupper.hpp"
#include "auctionitemdetaillower.hpp"
#include "aibridge/economy.hpp"
#include <map>

class ProcessRun;
class AuctionBoard final: public Widget
{
    public:
        struct Access
        {
            std::function<uint32_t()> dbid;
            std::function<uint64_t()> gold;
            std::function<void(int)> query;
            std::function<std::shared_ptr<aibridge::EconomicRequest>(uint64_t,bool)> submit;
            std::function<void(const char8_t *)> log;
            std::function<uint64_t()> clock;
            std::function<aibridge::Json()> scope;
        };
    private:
        ProcessRun *m_runProc;
        Access m_access;
        bool m_pending = false;
        std::shared_ptr<aibridge::EconomicRequest> m_economicRequest;
        bool m_economicSettled = false;
        struct CachedList { SDAuctionItemList list; aibridge::EconomyRefresh refresh; aibridge::Json scope=nullptr; };
        std::map<int, CachedList> m_cachedLists;
        int m_requestedCategory = 0;
        uint64_t m_panelRevision = 0;

    private:
        ImageBoard m_background;
        AuctionItemList m_itemList;

        TextBoard m_category;
        TextBoard m_itemCount;
        WMDAniBoard m_wmdAniBoard;

        TextBoard m_columnItem;
        TextBoard m_columnSeller;
        TextBoard m_columnTime;
        TextBoard m_columnPrice;

        AuctionItemDetailUpper m_itemDetailUpper;
        AuctionItemDetailLower m_itemDetailLower;

    private:
        TritexButton m_buttonPrevious;
        TritexButton m_buttonNext;
        TritexButton m_buttonRefresh;
        TritexButton m_buttonItemSearch;
        TritexButton m_buttonSellerSearch;
        TritexButton m_buttonRegister;
        TritexButton m_buttonBuy;
        TritexButton m_buttonCancel;
        TritexButton m_buttonClose;

    public:
        AuctionBoard(ProcessRun *, Widget * = nullptr, bool = false, std::optional<Access> = {});

    public:
        bool processEventDefault(const SDL_Event &, bool, Widget::ROIMap) override;

    public:
        void setItemList(SDAuctionItemList);
        bool pending() const noexcept { return m_pending; }
        bool uncertain() const noexcept;
        bool requestList(int category);
        const SDAuctionItemList *cachedList(int category) const;
        int currentCategory() const { return m_itemList.getItemList().category; }
        void showCategory(int category);
        uint64_t panelRevision() const noexcept { return m_panelRevision; }
        void closePanel();
        void invalidateScope(const aibridge::Json &context = nullptr);
        const aibridge::EconomyRefresh &refreshState(int category);
        std::shared_ptr<aibridge::EconomicRequest> submitAuction(uint64_t auctionID, bool cancel);
        void updateDefault(double) override;

    private:
        const SDAuctionItem *selectedItem() const;

    private:
        void refreshItemList();

    private:
        void confirmBuy();
        void buyItem(uint64_t, std::string);

    private:
        void confirmUnregister();
        void unregisterItem(uint64_t, std::string);
};
