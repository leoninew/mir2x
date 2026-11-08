#pragma once
#include <cstdint>
#include <optional>
#include <SDL3/SDL.h>

#include "strf.hpp"
#include "totype.hpp"
#include "process.hpp"
#include "message.hpp"
#include "inputline.hpp"
#include "textboard.hpp"
#include "messagestackboard.hpp"
#include "tritexbutton.hpp"
#include "gfxshapeboard.hpp"
#include "labelboard.hpp"
#include "checklabel.hpp"
#include "gfxdirbutton.hpp"
#include "trigfxbutton.hpp"
#include "loginaccountstore.hpp"

class ProcessLogin: public Process
{
    private:
        struct SubmittedLogin
        {
            std::string id;
            std::string password;
            bool remember;
        };

        std::optional<LoginAccountStore> m_accountStore;
        std::optional<SubmittedLogin> m_submittedLogin;

    private:
        Widget m_canvas;

    private:
        TritexButton m_button1;
        TritexButton m_button2;
        TritexButton m_button3;
        GfxShapeBoard m_loginButtonArt;
        TrigfxButton m_button4;

    private:
        LabelBoard m_idLabel;
        LabelBoard m_passwordLabel;
        InputLine m_idBox;
        InputLine m_passwordBox;

    private:
        GfxDirButton m_previousAccount;
        GfxDirButton m_nextAccount;
        CheckLabel m_rememberAccount;

    private:
        TextBoard m_buildSignature;

    private:
        GfxShapeBoard m_notifyBoardBg;
        MessageStackBoard m_notifyBoard;

    public:
        ProcessLogin();
        virtual ~ProcessLogin() = default;

    public:
        int id() const override
        {
            return PROCESSID_LOGIN;
        }

    public:
        void draw() const override;
        void update(double) override;
        void processEvent(const SDL_Event &) override;

    private:
        void doExit();
        void doLogin();
        void doCreateAccount();
        void doChangePassword();

    private:
        void switchAccount(bool previous);
        void sendLogin(const std::string &, const std::string &, bool remember = false);

    public:
        void on_SM_LOGINOK   (const uint8_t *, size_t);
        void on_SM_LOGINERROR(const uint8_t *, size_t);
};
