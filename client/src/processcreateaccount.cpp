#include "utf8f.hpp"
#include "idstrf.hpp"
#include "mathf.hpp"
#include "client.hpp"
#include "progusedb.hpp"
#include "sdldevice.hpp"
#include "processcreateaccount.hpp"

extern Client *g_client;
extern ProgUseDB *g_progUseDB;
extern SDLDevice *g_sdlDevice;

namespace
{
    constexpr int fieldX = 310;
    constexpr int fieldW = 240;
    constexpr int fieldH = 30;
    constexpr auto formText = colorf::RGBA(231, 224, 204, 255);
    constexpr auto formBorder = colorf::RGBA(130, 112, 73, 255);
    constexpr auto formAccent = colorf::RGBA(188, 160, 103, 255);
    constexpr auto formError = colorf::RGBA(255, 128, 112, 255);
}

ProcessCreateAccount::ProcessCreateAccount()
    : Process()
    , m_title{{.dir = DIR_NONE, .x = 400, .y = 178, .label = u8"创建账号", .font{.id = 1, .size = 18, .color = formText}}}
    , m_LBID{{.label = u8"账号", .font{.id = 1, .size = 15, .color = formText}}}
    , m_LBPwd{{.label = u8"密码", .font{.id = 1, .size = 15, .color = formText}}}
    , m_LBPwdConfirm{{.label = u8"确认密码", .font{.id = 1, .size = 15, .color = formText}}}
    , m_boxID
      {{
          .dir = DIR_LEFT,
          .x = fieldX + 8,
          .y = m_y + 85,
          .w = fieldW - 16,
          .h = 22,
          .font{.id = 2, .size = 15, .color = formText},
          .cursor{.align = DIR_LEFT},
          .onChange = [this](std::string){ onInputChange(0); },
      }}
    , m_boxPwd
      {{
          .dir = DIR_LEFT,
          .x = fieldX + 8,
          .y = m_y + 143,
          .w = fieldW - 16,
          .h = 22,
          .font{.id = 2, .size = 15, .color = formText},
          .cursor{.align = DIR_LEFT},
          .onChange = [this](std::string){ onInputChange(1); },
          .codeXfer = [maskCode = utf8f::str2code("*")](uint32_t){ return maskCode; },
      }}
    , m_boxPwdConfirm
      {{
          .dir = DIR_LEFT,
          .x = fieldX + 8,
          .y = m_y + 198,
          .w = fieldW - 16,
          .h = 22,
          .font{.id = 2, .size = 15, .color = formText},
          .cursor{.align = DIR_LEFT},
          .onChange = [this](std::string){ onInputChange(2); },
          .codeXfer = [maskCode = utf8f::str2code("*")](uint32_t){ return maskCode; },
      }}
    , m_LBCheckID{{.font{.id = 1, .size = 12, .color = formError}}}
    , m_LBCheckPwd{{.font{.id = 1, .size = 12, .color = formError}}}
    , m_LBCheckPwdConfirm{{.font{.id = 1, .size = 12, .color = formError}}}
    , m_submitLabel{{.label = u8"创建账号", .font{.id = 1, .size = 15, .color = formText}}}
    , m_submitArt
      {{
          .w = 120,
          .h = 30,
          .drawFunc = [this](const Widget *self, int drawX, int drawY)
          {
              const auto fill = !m_submit.active() ? colorf::RGBA(40, 40, 35, 255)
                  : m_submit.getState() == BEVENT_DOWN ? colorf::RGBA(38, 31, 20, 255)
                  : m_submit.getState() == BEVENT_ON ? colorf::RGBA(70, 56, 33, 255) : colorf::RGBA(49, 41, 27, 255);
              g_sdlDevice->fillRectangle(fill, drawX, drawY, self->w(), self->h(), 3);
              g_sdlDevice->drawRectangle(formAccent, drawX, drawY, self->w(), self->h(), 3);
              m_submitLabel.draw({.dir = DIR_NONE, .x = drawX + self->w() / 2, .y = drawY + self->h() / 2});
          },
      }}
    , m_submit
      {{
          .dir = DIR_NONE,
          .x = 400,
          .y = 395,
          .gfxFunc = [this]{ return &m_submitArt; },
          .onTrigger = [this](Widget *, int){ doPostAccount(); },
      }}
    , m_quit
      {{
          .x = m_x + 400,
          .y = m_y + 267,
          .texAniOpt = 0X0000001C,
          .onTrigger = [this](Widget *, int){ doExit(); },
      }}
    , m_infoStr
      {{
          .dir = DIR_NONE,
          .x = 400,
          .y = 428,
          .font{.id = 1, .size = 13, .color = colorf::YELLOW_A255},
      }}
{
    m_submit.setActive([this]{ return !m_submitting && !m_registered; });
    focusInput(m_boxID);
    localCheck();
}

void ProcessCreateAccount::update(double updateTime)
{
    m_boxID.update(updateTime);
    m_boxPwd.update(updateTime);
    m_boxPwdConfirm.update(updateTime);

    const auto previousTime = m_stateTime;
    m_stateTime += updateTime;
    if(m_registered && m_stateTime >= 1000.0){
        doExit();
    }
    else if(m_submitting && previousTime < 10000.0 && m_stateTime >= 10000.0){
        setInfoStr(u8"暂未收到注册结果，请等待或返回登录");
    }
}

void ProcessCreateAccount::draw() const
{
    const SDLDeviceHelper::RenderNewFrame newFrame;
    g_sdlDevice->drawTexture(g_progUseDB->retrieve(0X00000003), 0, 75);
    g_sdlDevice->drawTexture(g_progUseDB->retrieve(0X00000004), 0, 75, 0, 0, 800, 450);
    if(const auto frame = g_progUseDB->retrieve(0X0A000000)){
        g_sdlDevice->drawTexture(frame, m_x, m_y);
        g_sdlDevice->drawTexture(frame, m_x + 188, m_y + 232, 264, 232, 72, 28);
    }
    m_title.drawRoot({});

    const auto drawInput = [this](const InputLine &input, const LabelBoard &title, const LabelBoard &feedback, size_t index)
    {
        const auto centerY = input.dy() + input.h() / 2;
        const auto border = m_invalidFields[index] ? formError : input.focus() ? formAccent : formBorder;
        g_sdlDevice->fillRectangle(colorf::RGBA(20, 25, 22, 255), fieldX, centerY - fieldH / 2, fieldW, fieldH, 2);
        g_sdlDevice->drawRectangle(border, fieldX, centerY - fieldH / 2, fieldW, fieldH, 2);
        title.draw({.dir = DIR_RIGHT, .x = fieldX - 12, .y = centerY});
        input.drawRoot({});
        feedback.draw({.x = fieldX + 2, .y = centerY + fieldH / 2 + 3});
    };
    drawInput(m_boxID, m_LBID, m_LBCheckID, 0);
    drawInput(m_boxPwd, m_LBPwd, m_LBCheckPwd, 1);
    drawInput(m_boxPwdConfirm, m_LBPwdConfirm, m_LBCheckPwdConfirm, 2);

    m_submit.drawRoot({});
    m_quit.drawRoot({});
    m_infoStr.drawRoot({});
}

void ProcessCreateAccount::processEvent(const SDL_Event &event)
{
    if(m_quit.processEventRoot(event, true, {})){
        return;
    }
    if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE){
        doExit();
        return;
    }
    if(m_submitting || m_registered){
        return;
    }

    if(event.type == SDL_EVENT_KEY_DOWN){
        switch(event.key.key){
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                {
                    if(!event.key.repeat){
                        doPostAccount();
                    }
                    return;
                }
            case SDLK_TAB:
                {
                    std::array<InputLine *, 3> inputs{&m_boxID, &m_boxPwd, &m_boxPwdConfirm};
                    const size_t step = (event.key.mod & SDL_KMOD_SHIFT) ? inputs.size() - 1 : 1;
                    for(size_t index = 0; index < inputs.size(); ++index){
                        if(inputs[index]->focus()){
                            focusInput(*inputs[(index + step) % inputs.size()]);
                            return;
                        }
                    }
                    focusInput(*(step == 1 ? inputs.front() : inputs.back()));
                    return;
                }
            default:
                {
                    break;
                }
        }
    }

    if(m_submit.processEventRoot(event, true, {})){
        return;
    }

    if(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP){
        for(auto input: {&m_boxID, &m_boxPwd, &m_boxPwdConfirm}){
            const auto centerY = input->dy() + input->h() / 2;
            if(mathf::pointInRectangle(to_d(event.button.x), to_d(event.button.y), fieldX, centerY - fieldH / 2, fieldW, fieldH)
                    && !mathf::pointInRectangle(to_d(event.button.x), to_d(event.button.y), input->dx(), input->dy(), input->w(), input->h())){
                focusInput(*input);
                return;
            }
        }
    }

    bool tookEvent = false;
    for(auto input: {&m_boxID, &m_boxPwd, &m_boxPwdConfirm}){
        tookEvent |= input->processEventRoot(event, !tookEvent, {});
    }
}

void ProcessCreateAccount::doExit()
{
    g_client->requestProcess(PROCESSID_LOGIN);
}

void ProcessCreateAccount::doPostAccount()
{
    if(m_submitting || m_registered){
        return;
    }

    m_validationRequested = true;
    if(!localCheck()){
        if(m_invalidFields[0]){
            focusInput(m_boxID);
        }
        else if(m_invalidFields[1]){
            focusInput(m_boxPwd);
        }
        else{
            focusInput(m_boxPwdConfirm);
        }
        return;
    }

    CMCreateAccount request {};
    request.id.assign(m_boxID.getRawString());
    request.password.assign(m_boxPwd.getRawString());

    m_submitting = true;
    m_stateTime = 0.0;
    m_submitLabel.setText(u8"提交中…");
    setInfoStr(u8"正在创建账号，请稍候");
    g_client->send({CM_CREATEACCOUNT, request});
}

bool ProcessCreateAccount::localCheck()
{
    const auto account = m_boxID.getRawString();
    const auto password = m_boxPwd.getRawString();
    const auto confirmation = m_boxPwdConfirm.getRawString();
    const bool accountValid = idstrf::isEmail(account.c_str()) && account.size() <= SYS_IDSIZE
        && m_serverError != CRTACCERR_ACCOUNTEXIST && m_serverError != CRTACCERR_BADACCOUNT;
    const bool passwordValid = idstrf::isPassword(password.c_str()) && password.size() <= SYS_PWDSIZE
        && m_serverError != CRTACCERR_BADPASSWORD;
    const bool confirmationValid = !confirmation.empty() && password == confirmation;

    m_invalidFields = {
        !accountValid && (m_validationRequested || !account.empty()),
        !passwordValid && (m_validationRequested || !password.empty()),
        !confirmationValid && (m_validationRequested || !confirmation.empty()),
    };

    m_LBCheckID.setText(!m_invalidFields[0] ? u8"" : m_serverError == CRTACCERR_ACCOUNTEXIST ? u8"账号已存在，请更换邮箱或返回登录"
        : account.size() > SYS_IDSIZE ? u8"账号过长" : u8"请输入有效的邮箱账号");
    m_LBCheckPwd.setText(passwordValid ? u8"" : password.size() > SYS_PWDSIZE ? u8"密码过长"
        : u8"至少8位，含大小写、数字和~!@#$%^&*()");
    m_LBCheckPwd.setFontColor(m_invalidFields[1] ? formError : formText);
    m_LBCheckPwdConfirm.setText(!m_invalidFields[2] ? u8"" : confirmation.empty() ? u8"请再次输入密码" : u8"两次密码输入不一致");
    return accountValid && passwordValid && confirmationValid;
}

void ProcessCreateAccount::focusInput(InputLine &input)
{
    m_boxID.setFocus(&input == &m_boxID);
    m_boxPwd.setFocus(&input == &m_boxPwd);
    m_boxPwdConfirm.setFocus(&input == &m_boxPwdConfirm);
}

void ProcessCreateAccount::onInputChange(size_t index)
{
    if((index == 0 && (m_serverError == CRTACCERR_ACCOUNTEXIST || m_serverError == CRTACCERR_BADACCOUNT))
            || (index == 1 && m_serverError == CRTACCERR_BADPASSWORD)){
        m_serverError = CRTACCERR_NONE;
    }
    m_infoStr.clear();
    localCheck();
}

void ProcessCreateAccount::setInfoStr(const char8_t *text)
{
    m_infoStr.setText(text);
}
