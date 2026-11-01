#pragma once
#include "widget.hpp"
#include "serdesmsg.hpp"
#include "imageboard.hpp"
#include "auctionregisternote.hpp"
#include "auctionregisterbox.hpp"
#include "auctionregisterprice.hpp"
#include "tritexbutton.hpp"
#include "aibridge/economy.hpp"

class ProcessRun;
class InvPack;
class AuctionRegisterBoard final: public Widget
{
    public:
        struct Access
        {
            std::function<InvPack &()> inventory;
            std::function<void(SDItem)> restore;
            std::function<std::shared_ptr<aibridge::EconomicRequest>(const SDItem &,uint64_t,const std::string &)> submit;
            std::function<void(const char8_t *)> log;
        };
    private:
        ProcessRun *m_runProc;
        Access m_access;

    private:
        bool m_pending = false;
        bool m_dragging = false;
        std::shared_ptr<aibridge::EconomicRequest> m_economicRequest;
        bool m_economicSettled = false;

    private:
        ImageBoard m_background;
        AuctionRegisterNote m_note;
        AuctionRegisterBox m_box;
        AuctionRegisterPrice m_price;

        TritexButton m_buttonRegister;
        TritexButton m_buttonCancel;
        TritexButton m_buttonClose;

    public:
        AuctionRegisterBoard(ProcessRun *, Widget * = nullptr, bool = false, std::optional<Access> = {});

    public:
        void beginRegister();
        const SDItem &reservedItem() const { return m_box.item(); }
        bool pending() const noexcept { return m_pending; }
        bool uncertain() const noexcept;
        void prepareItem(uint32_t itemId, uint32_t seqId, uint64_t price, const std::string &note);
        std::shared_ptr<aibridge::EconomicRequest> submitRegister();
        void updateDefault(double) override;
        bool cancelUnsubmitted();

    public:
        bool processEventDefault(const SDL_Event &, bool, Widget::ROIMap) override;

    private:
        void setPending(bool);
        void setPrice();
        void confirmRegister();
        void registerItem();
        void confirmCancel();

    private:
        void restoreItemInBox();
        void restoreItemInGrab();

    private:
        void closeRegister(bool);
};
