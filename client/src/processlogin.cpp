#include <cstring>
#include <iostream>
#include <algorithm>
#include <stdexcept>

#include "log.hpp"
#include "utf8f.hpp"
#include "client.hpp"
#include "message.hpp"
#include "progusedb.hpp"
#include "bgmusicdb.hpp"
#include "sdldevice.hpp"
#include "buildconfig.hpp"
#include "messagestackboard.hpp"
#include "processlogin.hpp"
#include "clientargparser.hpp"

extern Log *g_mir2xLog;
extern Client *g_client;
extern ProgUseDB *g_progUseDB;
extern SDLDevice *g_sdlDevice;
extern BGMusicDB *g_bgmDB;
extern ClientArgParser *g_clientArgParser;

namespace
{
    constexpr int idFieldX = 104;
    constexpr int idFieldW = 196;
    constexpr int passwordFieldX = 356;
    constexpr int passwordFieldW = 164;
    constexpr int fieldY = 528;
    constexpr int fieldH = 30;
    constexpr int switchW = 22;
    constexpr auto formText = colorf::RGBA(215, 207, 185, 255);
    constexpr auto formAccent = colorf::RGBA(188, 160, 103, 255);
    constexpr auto formBorder = colorf::RGBA(91, 88, 69, 255);
}

ProcessLogin::ProcessLogin()
	: Process()
        , m_canvas
          {{
              .w = 800,
              .h = 600,
          }}

	, m_button1{{.dir = DIR_NONE, .x = 200, .y = 462, .texAniOpt = 0X00000005, .onTrigger = [this](Widget *, int){ doCreateAccount (); }, .parent{&m_canvas}}}
	, m_button2{{.dir = DIR_NONE, .x = 400, .y = 462, .texAniOpt = 0X00000008, .onTrigger = [this](Widget *, int){ doChangePassword(); }, .parent{&m_canvas}}}
	, m_button3{{.dir = DIR_NONE, .x = 600, .y = 462, .texAniOpt = 0X0000000B, .onTrigger = [this](Widget *, int){ doExit ();          }, .parent{&m_canvas}}}

    , m_loginButtonArt
      {{
          .w = 108,
          .h = fieldH,
          .drawFunc = [this](const Widget *self, int x, int y)
          {
              const auto state = m_button4.getState();
              const auto fill = state == BEVENT_DOWN ? colorf::RGBA(38, 31, 20, 255)
                  : state == BEVENT_ON ? colorf::RGBA(70, 56, 33, 255) : colorf::RGBA(49, 41, 27, 255);
              g_sdlDevice->fillRectangle(fill, x, y, self->w(), self->h(), 3);
              g_sdlDevice->drawRectangle(formAccent, x, y, self->w(), self->h(), 3);
              if(const auto texture = g_progUseDB->retrieve(0X0000000E, state - BEVENT_BEGIN)){
                  const auto [textureW, textureH] = SDLDeviceHelper::getTextureSize(texture);
                  const auto scale = std::min({1.0f, (self->w() - 16.0f) / textureW, (self->h() - 8.0f) / textureH});
                  const auto labelW = to_dround(textureW * scale);
                  const auto labelH = to_dround(textureH * scale);
                  g_sdlDevice->drawTexture(texture, x + (self->w() - labelW) / 2, y + (self->h() - labelH) / 2,
                          labelW, labelH, 0, 0, textureW, textureH);
              }
          },
      }}

    , m_button4
      {{
          .dir = DIR_LEFT,
          .x = 628,
          .y = fieldY + fieldH / 2,
          .gfxFunc = [this]{ return &m_loginButtonArt; },
          .onTrigger = [this](Widget *, int){ doLogin(); },
          .parent{&m_canvas},
      }}

    , m_idLabel
      {{
          .dir = DIR_LEFT,
          .x = 64,
          .y = fieldY + fieldH / 2,
          .label = u8"账号",
          .font{.size = 14, .color = formText},
          .parent{&m_canvas},
      }}

    , m_passwordLabel
      {{
          .dir = DIR_LEFT,
          .x = 316,
          .y = fieldY + fieldH / 2,
          .label = u8"密码",
          .font{.size = 14, .color = formText},
          .parent{&m_canvas},
      }}

	, m_idBox
          {{
              .x = idFieldX + 8,
              .y = fieldY + 6,

              .w = idFieldW - switchW - 16,
              .h = 18,

              .font
              {
                  .id = 2,
                  .size = 16,
                  .color = formText,
              },

              .cursor
              {
                  .align = DIR_LEFT,
              },

              .onTab = [this]
              {
                  m_passwordBox.setFocus(true);
                  m_idBox      .setFocus(false);
              },

              .onCR = [this]
              {
                  doLogin();
              },

              .parent{&m_canvas},
          }}

	, m_passwordBox
          {{
              .x = passwordFieldX + 8,
              .y = fieldY + 6,

              .w = passwordFieldW - 16,
              .h = 18,

              .font
              {
                  .id = 2,
                  .size = 16,
                  .color = formText,
              },

              .cursor
              {
                  .align = DIR_LEFT,
              },

              .onTab = [this]
              {
                  m_idBox      .setFocus(true);
                  m_passwordBox.setFocus(false);
              },

              .onCR = [this]
              {
                  doLogin();
              },

              .codeXfer = [maskCode = utf8f::str2code("*")](uint32_t)
              {
                  return maskCode;
              },

              .parent{&m_canvas},
          }}

    , m_previousAccount
      {{
          .x = idFieldX + idFieldW - switchW,
          .y = fieldY + 1,
          .w = switchW - 1,
          .h = fieldH / 2 - 1,
          .triangle
          {
              .dir = DIR_UP,
              .w = 7,
              .h = 4,
              .color = [this]{ return m_previousAccount.active() ? formAccent : formBorder; },
          },
          .frame{.show = false},
          .onTrigger = [this](Widget *, int){ switchAccount(true); },
          .parent{&m_canvas},
      }}

    , m_nextAccount
      {{
          .x = idFieldX + idFieldW - switchW,
          .y = fieldY + fieldH / 2,
          .w = switchW - 1,
          .h = fieldH / 2 - 1,
          .triangle
          {
              .dir = DIR_DOWN,
              .w = 7,
              .h = 4,
              .color = [this]{ return m_nextAccount.active() ? formAccent : formBorder; },
          },
          .frame{.show = false},
          .onTrigger = [this](Widget *, int){ switchAccount(false); },
          .parent{&m_canvas},
      }}

    , m_rememberAccount
      {{
          .dir = DIR_LEFT,
          .x = 536,
          .y = fieldY + fieldH / 2,
          .gap = 6,
          .box{.w = 14, .h = 14, .color = formAccent},
          .label{.text = u8"记住账号", .font{.size = 14, .color = formText}},
          .parent{&m_canvas},
      }}

    , m_buildSignature
      {{
          .textFunc = [](const Widget *)
          {
              return str_printf("编译版本号:%s", getBuildSignature());
          },

          .font
          {
              .id = 1,
              .size = 14,
              .color = colorf::YELLOW_A255,
          },

          .parent{&m_canvas},
      }}

    , m_notifyBoardBg
      {{
          .drawFunc = [](const Widget *self, int drawDstX, int drawDstY)
          {
              g_sdlDevice->fillRectangle(colorf::RGBA(0, 0,   0, 128), drawDstX, drawDstY, self->w(), self->h(), 8);
              g_sdlDevice->drawRectangle(colorf::RGBA(0, 0, 255, 128), drawDstX, drawDstY, self->w(), self->h(), 8);
          },

          .parent{&m_canvas},
      }}

    , m_notifyBoard
      {{
          .dir = DIR_NONE,
          .x = [this]{ return m_canvas.w() / 2; },
          .y = [this]{ return m_canvas.h() / 2; },
          .width = 0,
          .font
          {
              .id = 1,
              .size = 15,
              .color = colorf::YELLOW_A255,
          },
          .showTime = 5000,
          .entryLimit = 10,
          .align = ItemAlign::CENTER,
          .parent{&m_canvas},
      }}
{
    m_notifyBoard  .setShow([this]{ return !m_notifyBoard.empty(); });
    m_notifyBoardBg.setShow([this]{ return !m_notifyBoard.empty(); });

    m_notifyBoardBg.moveAt(DIR_UPLEFT, [this]{ return m_notifyBoard.dx() - 10; }, [this]{ return m_notifyBoard.dy() - 10; });
    m_notifyBoardBg.setSize(           [this]{ return m_notifyBoard. w() + 20; }, [this]{ return m_notifyBoard. h() + 20; });

    m_previousAccount.setActive([this]{ return m_accountStore && m_accountStore->accounts().size() > 1; });
    m_nextAccount    .setActive([this]{ return m_accountStore && m_accountStore->accounts().size() > 1; });

    try{
        const auto basePath = SDL_GetBasePath();
        if(!basePath){
            throw std::runtime_error("Unable to locate client directory");
        }
        m_accountStore.emplace(std::filesystem::path(to_u8rawstr(basePath)) / "accounts.json");
        m_accountStore->load();
        if(!m_accountStore->accounts().empty()){
            const auto &account = m_accountStore->accounts().front();
            m_idBox.setInput(account.id.c_str());
            m_passwordBox.setInput(account.password.c_str());
            m_rememberAccount.setter(true);
        }
    }
    catch(const std::exception &){
        m_accountStore.reset();
        m_notifyBoard.addMessage(u8"无法读取账号记录，请手工登录");
        g_mir2xLog->addLog(LOGTYPE_WARNING, "Unable to load accounts.json; saved accounts disabled for this login page");
    }

    g_sdlDevice->playBGM(g_bgmDB->retrieve(0X00040007));
    if(g_clientArgParser->autoLogin.has_value()){
        sendLogin(g_clientArgParser->autoLogin.value().first, g_clientArgParser->autoLogin.value().second);
    }
}

void ProcessLogin::update(double fUpdateTime)
{
    m_canvas.update(fUpdateTime);
}

void ProcessLogin::draw() const
{
    SDLDeviceHelper::RenderNewFrame newFrame;
    g_sdlDevice->drawTexture(g_progUseDB->retrieve(0X00000003),   0,  75);
    g_sdlDevice->drawTexture(g_progUseDB->retrieve(0X00000004),   0, 465);

    for(int row = 0; row < 16; ++row){
        g_sdlDevice->fillRectangle(colorf::RGBA(10, 14, 12, row * 15), 0, 432 + row, 800, 1);
    }
    g_sdlDevice->fillRectangle(colorf::RGBA(10, 14, 12, 240), 0, 448, 800, 152);
    g_sdlDevice->drawLine(colorf::RGBA(135, 117, 78, 120), 64, 486, 736, 486);

    const auto drawField = [](int x, int width, bool focused)
    {
        g_sdlDevice->fillRectangle(colorf::RGBA(5, 9, 7, 235), x, fieldY, width, fieldH, 3);
        g_sdlDevice->drawRectangle(focused ? formAccent : formBorder, x, fieldY, width, fieldH, 3);
    };
    drawField(idFieldX, idFieldW, m_idBox.focus());
    drawField(passwordFieldX, passwordFieldW, m_passwordBox.focus());

    const auto switchX = idFieldX + idFieldW - switchW - 1;
    g_sdlDevice->drawLine(formBorder, switchX, fieldY + 5, switchX, fieldY + fieldH - 6);

    m_canvas.drawRoot({});
}

void ProcessLogin::processEvent(const SDL_Event &event)
{
    switch(event.type){
        case SDL_EVENT_KEY_DOWN:
            {
                switch(event.key.key){
                    case SDLK_TAB:
                        {
                            if(true
                                    && !m_idBox      .focus()
                                    && !m_passwordBox.focus()){

                                m_idBox      .setFocus(true);
                                m_passwordBox.setFocus(false);
                                return;
                            }
                        }
                    default:
                        {
                            break;
                        }
                }
            }
        default:
            {
                break;
            }
    }

    m_canvas.processEventRoot(event, true, {});
}

void ProcessLogin::doLogin()
{
    const auto idStr  = m_idBox.getRawString();
    const auto pwdStr = m_passwordBox.getRawString();

    if(idStr.empty() || pwdStr.empty()){
        m_notifyBoard.addMessage(u8"无效的账号或密码");
    }
    else{
        // don't check id/password by idstf functions
        // this allows some test account like: (test, 123456)
        // but when creating account, changing password we need to be extremely careful

        sendLogin(idStr, pwdStr, m_rememberAccount.getter());
    }
}

void ProcessLogin::doCreateAccount()
{
    g_client->requestProcess(PROCESSID_CREATEACCOUNT);
}

void ProcessLogin::doChangePassword()
{
    g_client->requestProcess(PROCESSID_CHANGEPASSWORD);
}

void ProcessLogin::doExit()
{
    std::exit(0);
}

void ProcessLogin::switchAccount(bool previous)
{
    if(m_accountStore){
        if(const auto account = m_accountStore->adjacent(m_idBox.getRawString(), previous)){
            m_idBox.setInput(account->id.c_str());
            m_passwordBox.setInput(account->password.c_str());
        }
    }
}

void ProcessLogin::sendLogin(const std::string &id, const std::string &password, bool remember)
{
    if(m_submittedLogin){
        return;
    }

    CMLogin cmL;
    std::memset(&cmL, 0, sizeof(cmL));

    cmL.id.assign(id);
    cmL.password.assign(password);
    // The response belongs to this submission, even if the user edits the fields while waiting.
    m_submittedLogin = SubmittedLogin{id, password, remember};
    g_client->send({CM_LOGIN, cmL});
}
