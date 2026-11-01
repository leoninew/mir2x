// inventory class
// server won't store the item place
// it only stores the item weight in total
//
// client can use different strategy to store them
// player has max weight to hold only

#pragma once
#include <string>
#include <optional>
#include "widget.hpp"
#include "texslider.hpp"
#include "labelboard.hpp"
#include "textboard.hpp"
#include "wmdaniboard.hpp"
#include "tritexbutton.hpp"
#include "aibridge/shopmodel.hpp"
#include "serdesmsg.hpp"

class ProcessRun;
class InvPack;
struct PackBin;
class InventoryBoard: public Widget
{
    public:
        struct ShopAccess
        {
            std::function<const InvPack *()> inventory;
            std::function<void(uint64_t, const std::string &, const std::string &)> event;
            std::function<uint64_t()> now;
        };
        struct InitArgs final
        {
            int x = 0;
            int y = 0;

            ProcessRun *runProc {};
            Widget::WADPair parent{};
            std::optional<ShopAccess> shopAccess;
        };

    private:
        const int m_invGridX0 = 18;
        const int m_invGridY0 = 59;

    private:
        const int m_invOpButtonX = 295;
        const int m_invOpButtonY = 475;

    private:
        int m_invOpCost = -1;
        int m_selectedIndex = -1;

    private:
        ProcessRun *m_processRun;
        ShopAccess m_shopAccess;

    private:
        SDStartInvOp m_sdInvOp;
        uint64_t m_invOpRevision = 0, m_quoteRevision = 0, m_quoteReceivedAt = 0;
        uint32_t m_quoteCount = 0;
        SDItem m_quoteItem;
        std::optional<uint32_t> m_quoteCost;
        aibridge::ShopQuery m_quoteQuery;

    private:
        WMDAniBoard m_wmdAniBoard;

    private:
        TexSlider m_slider;

    private:
        TritexButton m_sortButton;
        TritexButton m_closeButton;

    private:
        TritexButton m_invOpButton;
        TextBoard    m_invOpTitle;

    private:
        TextBoard m_goldStr;
        TextBoard m_invOpCostStr;

    public:
        InventoryBoard(InventoryBoard::InitArgs);

    private:
        void drawGold() const;
        void drawInvOpTitle() const;
        void drawItem(int, int, size_t, const PackBin &, uint32_t) const;

    public:
        void updateDefault(double) override;

    public:
        void drawDefault(Widget::ROIMap) const override;

    public:
        bool processEventDefault(const SDL_Event &, bool, Widget::ROIMap) override;

    private:
        size_t getStartRow() const;
        size_t getRowCount() const;

    private:
        int getPackBinIndex(int, int) const;
        std::tuple<int, int> getInvGrid(int, int) const;

    private:
        void drawItemHoverText(const PackBin &) const;

    private:
        void packBinConsume(const PackBin &);

    public:
        void clearInvOp();
        void invalidateQuote();
        const SDStartInvOp &aiInvOp() const noexcept { return m_sdInvOp; }
        uint64_t invOpRevision() const noexcept { return m_invOpRevision; }
        uint64_t quoteRevision() const noexcept { return m_quoteRevision; }
        const auto &quoteQuery() const noexcept { return m_quoteQuery; }
        std::optional<uint32_t> quotedCost() const noexcept { return m_quoteCost; }
        uint32_t quotedCount() const noexcept { return m_quoteCount; }
        uint64_t quoteReceivedAt() const noexcept { return m_quoteReceivedAt; }
        aibridge::ShopQuery::Start queryInvOp(uint32_t itemId, uint32_t seqId);
        bool submitInvOp(uint32_t itemId, uint32_t seqId);
        void startInvOp(SDStartInvOp);
        void setInvOpCost(int, uint32_t, uint32_t, size_t);

    private:
        void commitInvOp();

    public:
        void removeItem(uint32_t, uint32_t, size_t);

    private:
        static std::u8string typeListString(const std::vector<std::u8string> &typeList);
};
