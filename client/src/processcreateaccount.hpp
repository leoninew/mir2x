#pragma once
#include <array>
#include <cstdint>
#include "process.hpp"
#include "labelboard.hpp"
#include "inputline.hpp"
#include "tritexbutton.hpp"
#include "trigfxbutton.hpp"
#include "gfxshapeboard.hpp"

class ProcessCreateAccount: public Process
{
    private:
        constexpr static int m_x = 180;
        constexpr static int m_y = 145;

    private:
        LabelBoard m_title;
        LabelBoard m_LBID;
        LabelBoard m_LBPwd;
        LabelBoard m_LBPwdConfirm;

    private:
        InputLine m_boxID;
        InputLine m_boxPwd;
        InputLine m_boxPwdConfirm;

    private:
        LabelBoard m_LBCheckID;
        LabelBoard m_LBCheckPwd;
        LabelBoard m_LBCheckPwdConfirm;

    private:
        LabelBoard m_submitLabel;
        GfxShapeBoard m_submitArt;
        TrigfxButton m_submit;
        TritexButton m_quit;

    private:
        LabelBoard m_infoStr;
        std::array<bool, 3> m_invalidFields {};
        bool m_validationRequested = false;
        bool m_submitting = false;
        bool m_registered = false;
        int m_serverError = 0;
        double m_stateTime = 0.0;

    public:
        ProcessCreateAccount();
        virtual ~ProcessCreateAccount() = default;

    public:
        int id() const override
        {
            return PROCESSID_CREATEACCOUNT;
        }

    public:
        void draw() const override;
        void update(double) override;
        void processEvent(const SDL_Event &) override;

    private:
        void doPostAccount();
        void doExit();

    private:
        bool localCheck();
        void focusInput(InputLine &);
        void onInputChange(size_t);

    private:
        void setInfoStr(const char8_t *);

    public:
        void on_SM_CREATEACCOUNTOK   (const uint8_t *, size_t);
        void on_SM_CREATEACCOUNTERROR(const uint8_t *, size_t);
};
