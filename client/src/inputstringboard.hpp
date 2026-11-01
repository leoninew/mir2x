#pragma once
#include <vector>
#include <cstdint>
#include "widget.hpp"
#include "imageboard.hpp"
#include "inputline.hpp"
#include "layoutboard.hpp"
#include "tritexbutton.hpp"
#include "gfxshapeboard.hpp"
#include "aibridge/npcmodel.hpp"

class InputStringBoard: public Widget
{
    public:
        struct WaitInputArgs final
        {
            std::u8string layoutString {};
            bool security = false;
            bool modal    = false;

            std::function<void(std::u8string)>            onAccept = nullptr;
            std::function<void()>                         onReject = nullptr;
            std::function<bool(std::string, std::string)> validate = nullptr;
            uint64_t npcUid = 0; // Only SM_STARTINPUT supplies an NPC source.
        };

    private:
        std::function<void(std::u8string)> m_onAccept;
        std::function<void()> m_onReject;

    private:
        bool m_modal = false;
        uint64_t m_revision = 0;
        uint64_t m_npcUid = 0;
        uint64_t m_tradePeerUid = 0;
        bool m_sensitive = false;

    private:
        ImageBoard m_bg;

    private:
        LayoutBoard m_textInfo;

    private:
        GfxShapeBoard m_inputBg;
        InputLine m_input;

    private:
        TritexButton m_yesButton;
        TritexButton m_nopButton;

    public:
        InputStringBoard(
                Widget::VarDir,
                Widget::VarInt,
                Widget::VarInt,

                bool,

                Widget * = nullptr,
                bool     = false);

    private:
        void acceptInput();
        void rejectInput();

    public:
        void clear()
        {
            m_input.clear();
        }

    public:
        bool showModal() const
        {
            return show() && m_modal;
        }

    public:
        void waitInput(InputStringBoard::WaitInputArgs);
        void waitChoice(std::u8string, bool, std::function<void()>, std::function<void()> = {});
        std::optional<aibridge::NpcInput> npcInput() const;
        bool submitNpcInput(uint64_t revision, const std::string &text);
        uint64_t revision() const noexcept { return m_revision; }
        void waitTradeChoice(uint64_t peerUid, std::u8string, std::function<void()>, std::function<void()>);
        uint64_t tradePeerUid() const noexcept { return show() ? m_tradePeerUid : 0; }
        bool respondTradeChoice(uint64_t revision, uint64_t peerUid, bool accept, const std::function<bool()> &canSend = {});
};
