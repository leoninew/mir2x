#include "widget.hpp"
#include "colorf.hpp"
#include "utf8f.hpp"
#include "pngtexdb.hpp"
#include "sdldevice.hpp"
#include "gui/controlboard/controlboard.hpp"
#include "inputstringboard.hpp"

extern PNGTexDB *g_progUseDB;
extern SDLDevice *g_sdlDevice;

InputStringBoard::InputStringBoard(
        Widget::VarDir argDir,
        Widget::VarInt argX,
        Widget::VarInt argY,

        bool argSecurity,

        Widget *argParent,
        bool    argAutoDelete)

    : Widget
      {{
          .dir = std::move(argDir),

          .x = std::move(argX),
          .y = std::move(argY),

          .parent
          {
              .widget = argParent,
              .autoDelete = argAutoDelete,
          }
      }}

    , m_bg
      {{
          .texLoadFunc = [](const Widget *)
          {
              return g_progUseDB->retrieve(0X07000000);
          },

          .blendMode = SDL_BLENDMODE_BLEND,
          .parent{this},
      }}

    , m_textInfo
      {{
          .dir = DIR_NONE,

          .x = [this](){ return w() / 2; },
          .y = 120,

          .lineWidth = 250,

          .font
          {
              .id = 1,
              .size = 12,
          },

          .lineAlign = LALIGN_JUSTIFY,
          .parent{this},
      }}

    , m_inputBg
      {{
          .x = 22,
          .y = 225,

          .w = 315,
          .h = 23,

          .drawFunc = [](const Widget *self, int drawDstX, int drawDstY)
          {
              g_sdlDevice->fillRectangle(colorf::WHITE + colorf::A_SHF(32), drawDstX, drawDstY, self->w(), self->h());
          },

          .parent{this},
      }}

    , m_input
      {{
          .x = [this]{ return m_inputBg.dx(); },
          .y = [this]{ return m_inputBg.dy(); },

          .w = [this]{ return m_inputBg.w(); },
          .h = [this]{ return m_inputBg.h(); },

          .font
          {
              .id = 1,
              .size = 14,
          },

          .onCR = [this]
          {
              acceptInput();
          },

          .codeXfer = [argSecurity, codeMask = utf8f::str2code("*")](uint32_t codePoint)
          {
              return argSecurity ? codeMask : codePoint;
          },

          .parent{this},
      }}

    , m_yesButton
      {{
          .x = [this]
          {
              if(m_onAccept && m_onReject){
                  return 66;
              }
              else{
                  return (w() - m_yesButton.w()) / 2;
              }
          },
          .y = 190,

          .texIDList
          {
              .off  = 0X07000001,
              .on   = 0X07000002,
              .down = 0X07000003,
          },

          .onTrigger = [this](Widget *, int)
          {
              acceptInput();
          },

          .parent{this},
      }}

    , m_nopButton
      {{
          .x = [this]
          {
              if(m_onAccept && m_onReject){
                  return 212;
              }
              else{
                  return (w() - m_nopButton.w()) / 2;
              }
          },
          .y = 190,

          .texIDList
          {
              .off  = 0X07000004,
              .on   = 0X07000005,
              .down = 0X07000006,
          },

          .onTrigger = [this](Widget *, int)
          {
              rejectInput();
          },

          .parent{this},
      }}
{
    setShow(false);
    setSize([this]{ return m_bg.w(); },
            [this]{ return m_bg.h(); });

    m_yesButton.setShow([this]{ return static_cast<bool>(m_onAccept) || !m_onReject; });
    m_nopButton.setShow([this]{ return static_cast<bool>(m_onReject)               ; });

    m_inputBg.setShow([this]
    {
        return m_input.focus();
    });
}

void InputStringBoard::acceptInput()
{
    const std::string fullInput = m_input.getRawString();
    const auto inputPos = fullInput.find_first_not_of(" \n\r\t");
    const std::string realInput = (inputPos == std::string::npos) ? "" : fullInput.substr(inputPos);

    if(m_npcUid && !aibridge::npcInputFits(fullInput)){ return; }
    setShow(false);

    m_input.clear();
    m_input.setFocus(false);

    m_modal = false;
    m_npcUid = 0;
    m_tradePeerUid = 0;
    ++m_revision;
    m_onReject = {};
    m_input.setValidateFunc({});
    if(auto onAccept = std::move(m_onAccept)){
        onAccept(to_u8rawstr(realInput));
    }
}

void InputStringBoard::waitInput(InputStringBoard::WaitInputArgs args)
{
    m_tradePeerUid = 0;
    m_npcUid = 0;
    m_onAccept = {};
    m_onReject = {};
    ++m_revision;
    m_textInfo.loadXML(to_cstr(args.layoutString), 0);
    m_npcUid = args.npcUid;
    m_sensitive = args.security;
    m_onAccept = std::move(args.onAccept);
    m_onReject = std::move(args.onReject);
    m_modal = args.modal;

    m_input.setShow(true);
    m_input.setActive(true);
    m_input.setValidateFunc([npcUid=args.npcUid, validate=std::move(args.validate)](std::string current, std::string added){
        return (!npcUid || aibridge::npcInputFits(current + added)) && (!validate || validate(std::move(current), std::move(added)));
    });
    if(args.security){
        m_input.setCodeXferFunc([maskCode = utf8f::str2code("*")](uint32_t){ return maskCode; });
    } else {
        m_input.setCodeXferFunc(nullptr);
    }

    clear();
    setShow(true);
    setFocus(true);
}

void InputStringBoard::waitChoice(std::u8string layoutString, bool modal, std::function<void()> onAccept, std::function<void()> onReject)
{
    m_tradePeerUid = 0;
    ++m_revision;
    m_npcUid = 0;
    m_onAccept = {};
    m_onReject = {};
    m_textInfo.loadXML(to_cstr(layoutString), 0);
    if(onAccept){
        m_onAccept = [onAccept = std::move(onAccept)](std::u8string){ onAccept(); };
    }
    else{
        m_onAccept = nullptr;
    }
    m_onReject = std::move(onReject);
    m_modal = modal;

    clear();
    m_input.setFocus(false);
    m_input.setActive(false);
    m_input.setShow(false);
    m_input.setValidateFunc({});

    setShow(true);
    setFocus(true);
}

std::optional<aibridge::NpcInput> InputStringBoard::npcInput() const
{
    if(!show() || !m_npcUid || !m_onAccept){ return {}; }
    return aibridge::NpcInput{m_revision, m_npcUid, m_textInfo.getTextBounded(2048), 199, m_sensitive};
}

bool InputStringBoard::submitNpcInput(uint64_t revision, const std::string &text)
{
    if(!show() || !m_npcUid || !m_onAccept || revision != m_revision || !aibridge::npcInputFits(text)){
        return false;
    }
    if(!m_input.setValidatedInput(text)){ return false; }
    acceptInput();
    return true;
}

void InputStringBoard::rejectInput()
{
    setShow(false);
    m_input.clear();
    m_input.setFocus(false);
    m_modal = false;
    m_npcUid = 0;
    m_tradePeerUid = 0;
    ++m_revision;
    m_onAccept = {};
    m_input.setValidateFunc({});
    if(auto callback = std::move(m_onReject)){ callback(); }
}

void InputStringBoard::waitTradeChoice(uint64_t peerUid, std::u8string text,
    std::function<void()> accept, std::function<void()> reject)
{
    waitChoice(std::move(text), false, std::move(accept), std::move(reject));
    m_tradePeerUid = peerUid;
}

bool InputStringBoard::respondTradeChoice(uint64_t expectedRevision, uint64_t peerUid, bool accept, const std::function<bool()> &canSend)
{
    if(!show() || !peerUid || m_tradePeerUid != peerUid || m_revision != expectedRevision
        || (accept ? !m_onAccept : !m_onReject)){ return false; }
    if(canSend && !canSend()){ return false; }
    if(accept){ acceptInput(); }
    else { rejectInput(); }
    return true;
}
