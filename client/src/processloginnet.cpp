#include "log.hpp"
#include "client.hpp"
#include "fflerror.hpp"
#include "processlogin.hpp"

extern Client *g_client;
extern Log *g_mir2xLog;
void ProcessLogin::on_SM_LOGINOK(const uint8_t *, size_t)
{
    if(m_submittedLogin && m_submittedLogin->remember && m_accountStore){
        try{
            m_accountStore->remember(m_submittedLogin->id, m_submittedLogin->password);
        }
        catch(const std::exception &){
            g_mir2xLog->addLog(LOGTYPE_WARNING, "Unable to save accounts.json; login will continue");
        }
    }
    m_submittedLogin.reset();
    g_client->requestProcess(PROCESSID_SELECTCHAR);
}

void ProcessLogin::on_SM_LOGINERROR(const uint8_t *buf, size_t)
{
    m_submittedLogin.reset();
    const auto smLE = ServerMsg::conv<SMLoginError>(buf);
    switch(smLE.error){
        case LOGINERR_NOACCOUNT:
            {
                m_notifyBoard.addMessage(u8"无效的账号或密码");
                return;
            }
        case LOGINERR_MULTILOGIN:
            {
                m_notifyBoard.addMessage(u8"该账号已经登录");
                return;
            }
        default:
            {
                throw fflreach();
            }
    }
}
