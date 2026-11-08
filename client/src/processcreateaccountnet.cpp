#include <cstdint>
#include "servermsg.hpp"
#include "processcreateaccount.hpp"

void ProcessCreateAccount::on_SM_CREATEACCOUNTOK(const uint8_t *, size_t)
{
    m_submitting = false;
    m_registered = true;
    m_stateTime = 0.0;
    m_submitLabel.setText(u8"注册成功");
    setInfoStr(u8"注册成功，即将返回登录");
    m_boxID.setFocus(false);
    m_boxPwd.setFocus(false);
    m_boxPwdConfirm.setFocus(false);
}

void ProcessCreateAccount::on_SM_CREATEACCOUNTERROR(const uint8_t *buf, size_t)
{
    const auto smCAE = ServerMsg::conv<SMCreateAccountError>(buf);
    m_submitting = false;
    m_submitLabel.setText(u8"创建账号");
    m_serverError = smCAE.error;
    setInfoStr(u8"");
    localCheck();
    switch(smCAE.error){
        case CRTACCERR_ACCOUNTEXIST:
            {
                focusInput(m_boxID);
                return;
            }
        case CRTACCERR_BADACCOUNT:
            {
                focusInput(m_boxID);
                return;
            }
        case CRTACCERR_BADPASSWORD:
            {
                focusInput(m_boxPwd);
                return;
            }
        default:
            {
                throw fflreach();
            }
    }
}
