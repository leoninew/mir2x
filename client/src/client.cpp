#include <set>
#include <future>
#include <thread>
#include <cstring>
#include <cinttypes>

#include "log.hpp"
#include "client.hpp"
#include "initview.hpp"
#include "sysconst.hpp"
#include "pngtexdb.hpp"
#include "fontexdb.hpp"
#include "sdldevice.hpp"
#include "servermsg.hpp"
#include "processrun.hpp"
#include "processlogo.hpp"
#include "processsync.hpp"
#include "processselectchar.hpp"
#include "processcreatechar.hpp"
#include "processcreateaccount.hpp"
#include "processchangepassword.hpp"
#include "processconnectionerror.hpp"
#include "pngtexoffdb.hpp"
#include "messagestackboard.hpp"
#include "buildconfig.hpp"
#include "processlogin.hpp"
#include "clientargparser.hpp"
#include "aibridge/runtime.hpp"

extern Log *g_mir2xLog;
extern SDLDevice *g_sdlDevice;
extern MessageStackBoard *g_notifyBoard;
extern ClientArgParser *g_clientArgParser;

Client::Client()
    : m_clientMonitor()
    , m_serverDelay( 0.00)
    , m_netPackTick(-1.00)
{
    InitView(10);
    g_sdlDevice->createMainWindow();
}

Client::~Client()
{
    shutdown();
}

void Client::shutdown() noexcept
{
    if(m_shutdown){ return; }
    m_shutdown = true;
    m_quitRequested = true;
    if(m_aiRuntime){ m_aiRuntime->shutdown(); }
    cancelResponses(ResponseCallbacks::CancelReason::Shutdown);
}

void Client::observeAi(const aibridge::RuntimeState &state)
{
    if(m_aiRuntime){
        try{ m_aiRuntime->observe(state); }
        catch(...){
            m_aiRuntime->shutdown();
            g_mir2xLog->addLog(LOGTYPE_WARNING, "AI bridge stopped after context failure");
        }
    }
}

void Client::observeAi()
{
    if(!m_aiRuntime || m_shutdown){ return; }
    aibridge::RuntimeState state;
    if(auto run = getProcess<ProcessRun>()){
        state = run->aiState();
    }
    else if(!m_currentProcess || m_quitRequested){ state.phase = "exiting"; }
    else{
        switch(m_currentProcess->id()){
            case PROCESSID_SELECTCHAR: case PROCESSID_CREATECHAR: state.phase = "select_character"; break;
            case PROCESSID_CONNECTIONERROR: state.phase = "disconnected"; break;
            case PROCESSID_LOGO: case PROCESSID_SYRC: state.phase = "loading"; break;
            default: state.phase = "login"; break;
        }
    }
    observeAi(state);
}

void Client::drawAiPanel() const
{
    if(m_aiRuntime){
        try{ m_aiRuntime->drawPanel(); }
        catch(...){ m_aiRuntime->shutdown(); }
    }
}

void Client::logoutAi()
{
    observeAi({.phase="select_character"});
}

void Client::initializeAi()
{
    if(!g_clientArgParser->enableAiBridge || m_shutdown){ return; }
    try{
        m_aiRuntime = std::make_unique<aibridge::AiBridgeRuntime>(*this);
        m_aiRuntime->start({g_clientArgParser->aiBridgeDir, g_clientArgParser->aiPolicy, std::string(getBuildSignature())});
        observeAi();
    }
    catch(...){ g_mir2xLog->addLog(LOGTYPE_WARNING, "AI bridge initialization failed; game connection remains enabled"); }
}

void Client::send(const ClientMsgBuf &msg, ResponseCallbacks::Handler handler)
{
    if(handler){
        sendWithResponse(msg, std::move(handler), {}, {});
    }
    else{
        m_netIO.send(msg.headCode, msg.data, msg.size, 0);
        sendCMsgLog(msg.headCode);
    }
}

uint64_t Client::sendScoped(const ClientMsgBuf &msg, uint64_t timeoutMS, ResponseCallbacks::Handler handler, ResponseCallbacks::OnCancel onCancel)
{
    const uint64_t now = SDL_GetTicks();
    if(!timeoutMS || timeoutMS > std::numeric_limits<uint64_t>::max() - now){
        throw std::invalid_argument("Invalid response timeout");
    }
    return sendWithResponse(msg, std::move(handler), now + timeoutMS, std::move(onCancel));
}

uint64_t Client::sendWithResponse(const ClientMsgBuf &msg, ResponseCallbacks::Handler handler, std::optional<uint64_t> deadline, ResponseCallbacks::OnCancel onCancel)
{
    const auto id = m_respHandlers.add(std::move(handler), deadline, std::move(onCancel));
    try{
        m_netIO.send(msg.headCode, msg.data, msg.size, id);
    }
    catch(...){
        const auto error = std::current_exception();
        try{
            m_respHandlers.cancel(id, ResponseCallbacks::CancelReason::SendFailed);
        }
        catch(...){}
        std::rethrow_exception(error);
    }
    sendCMsgLog(msg.headCode);
    return id;
}

void Client::cancelResponses(ResponseCallbacks::CancelReason reason) noexcept
{
    try{
        m_respHandlers.cancelAll(reason);
    }
    catch(...){
        g_mir2xLog->addLog(LOGTYPE_WARNING, "Response cancellation callback failed");
    }
}

void Client::processEvent()
{
    if(m_currentProcess){
        SDL_Event event;
        while(m_currentProcess && !m_quitRequested && SDL_PollEvent(&event)){
            if(g_clientArgParser->debugSDLEvent){
                char buf[256];
                SDL_GetEventDescription(&event, buf, sizeof(buf));
                g_mir2xLog->addLog(LOGTYPE_INFO, "[%s] %s", str_localtime().c_str(), buf);
            }
            auto gameWindow = SDL_GetRenderWindow(g_sdlDevice->getRenderer());
            if(auto eventWindow = SDL_GetWindowFromEvent(&event); eventWindow && eventWindow != gameWindow){ continue; }
            const auto gameWindowID = SDL_GetWindowID(gameWindow);
            if(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID != gameWindowID){ continue; }
            if(aibridge::ControlGesture::manual(event) && !aibridge::ControlGesture::gameWindowEvent(event, gameWindowID)){ continue; }
            if(event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED){
                requestQuit();
                break;
            }
            const auto scaled = g_sdlDevice->scaleEvent(event);
            observeAi();
            if(m_aiRuntime){
                try{ if(m_aiRuntime->processEvent(scaled)){ continue; } }
                catch(...){
                    m_aiRuntime->shutdown();
                    g_mir2xLog->addLog(LOGTYPE_WARNING, "AI bridge stopped after input failure");
                }
            }
            m_currentProcess->processEvent(scaled);
            switchProcess();
        }
    }
}

void Client::mainLoop()
{
    struct ShutdownGuard { Client &client; ~ShutdownGuard(){ client.shutdown(); } } guard{*this};
    switchProcess(PROCESSID_LOGO);
    initASIO();
    initializeAi();

    const auto fDelayDraw   = (1000.0 / (1.0 * SYS_DEFFPS)) / 6.0;
    const auto fDelayUpdate = (1000.0 / (1.0 * SYS_DEFFPS)) / 7.0;
    const auto fDelayLoop   = (1000.0 / (1.0 * SYS_DEFFPS)) / 8.0;

    auto fLastDraw   = SDL_GetTicks() * 1.0;
    auto fLastUpdate = SDL_GetTicks() * 1.0;
    auto fLastLoop   = SDL_GetTicks() * 1.0;

    while(true){
        if(g_clientArgParser->enableClientMonitor){
            PrintMonitor();
        }

        if(m_quitRequested || !(m_currentProcess && (m_currentProcess->id() != PROCESSID_EXIT))){
            break;
        }

        // Drain network and player input before animation triggers or new AI work.
        if(m_aiRuntime){ eventDelay(0); }
        if(m_quitRequested || !m_currentProcess){ break; }

        if(m_netPackTick > 0.0){
            if(SDL_GetTicks() * 1.0 - m_netPackTick > 15.0 * 1000){
                // std::exit(0);
            }
        }

        if(auto fCurrUpdate = 1.0 * SDL_GetTicks(); fCurrUpdate - fLastUpdate > fDelayUpdate){
            auto fPastUpdate = fCurrUpdate - fLastUpdate;
            fLastUpdate = fCurrUpdate;
            update(fPastUpdate);
        }
        if(m_quitRequested || !m_currentProcess){ break; }

        if(auto fCurrDraw = 1.0 * SDL_GetTicks(); fCurrDraw - fLastDraw > fDelayDraw){
            fLastDraw = fCurrDraw;
            draw();
        }

        auto fCurrLoop = 1.0 * SDL_GetTicks();
        auto fPastLoop = fCurrLoop - fLastLoop;

        fLastLoop = fCurrLoop;
        eventDelay(fDelayLoop - fPastLoop);
    }
}

void Client::eventDelay(double fDelayMS)
{
    const double fStartDelayMS = SDL_GetTicks() * 1.0;
    while(!m_quitRequested){
        m_netIO.poll();
        m_respHandlers.expire(SDL_GetTicks());
        switchProcess();
        processEvent();
        if(m_quitRequested || !m_currentProcess){ break; }
        observeAi();
        if(m_aiRuntime){ m_aiRuntime->poll(); }

        double fCurrentMS = SDL_GetTicks() * 1.0;
        double fDelayDone = fCurrentMS - fStartDelayMS;

        if(fDelayDone >= fDelayMS){
            break;
        }

        // here we check the delay time
        // since SDL_Delay(0) may run into problem

        const auto nDelayMSCount = (Uint32)(std::lround((fDelayMS - fDelayDone) * 0.80));
        if(nDelayMSCount <= 0){
            break;
        }

        SDL_Delay(nDelayMSCount);
    }
}

void Client::initASIO()
{
    const auto ipStr = g_clientArgParser->serverIP;
    const auto portStr = std::to_string(g_clientArgParser->serverPort.first);

    m_netIO.start(ipStr, portStr, [this](uint8_t headCode, const uint8_t *pData, size_t nDataLen, uint64_t respID)
    {
        // core should handle on fully recieved message from the serer
        // previously there are two steps (HC, Body) seperately handled, error-prone

        if(respID){
            if(!m_respHandlers.dispatch(respID, headCode, pData, nDataLen, SDL_GetTicks())){
                g_mir2xLog->addLog(LOGTYPE_WARNING, "Ignored expired or unknown response id: %llu", static_cast<unsigned long long>(respID));
            }
        }
        else{
            onServerMessage(headCode, pData, nDataLen);
        }
        switchProcess();
    },

    [this](std::error_code ec)
    {
        observeAi({.phase="disconnected"});
        g_mir2xLog->addLog(LOGTYPE_WARNING, "Connection error: %s (%s:%d)", ec.message().c_str(), ec.category().name(), ec.value());
        requestProcess(PROCESSID_CONNECTIONERROR);
        switchProcess();
    });
}

void Client::onServerMessage(uint8_t headCode, const uint8_t *buf, size_t bufSize)
{
    m_netPackTick = SDL_GetTicks() * 1.0;
    if(headCode != SM_PING){
        sendSMsgLog(headCode);
    }

    m_clientMonitor.SMProcMonitorList[headCode].recvCount++;
    raii_timer stTimer(&(m_clientMonitor.SMProcMonitorList[headCode].procTick));

    switch(headCode){
        case SM_BUILDVERSION:
            {
                if(!g_clientArgParser->disableVersionCheck){
                    if(const auto smBV = ServerMsg::conv<SMBuildVersion>(buf); smBV.version.as_sv() != getBuildSignature()){
                        throw fflpanic("client/server version mismatches, client: {}, server: {}", getBuildSignature(), smBV.version.as_sv().data());
                    }
                }
                break;
            }

#define _INSTALL_SM_HANDLER(ProcessType, smType) case smType: {if(auto proc = getProcess<ProcessType>()){ proc->on_##smType(buf, bufSize); } break; }

        _INSTALL_SM_HANDLER(ProcessLogin, SM_LOGINERROR)
        _INSTALL_SM_HANDLER(ProcessLogin, SM_LOGINOK)

        _INSTALL_SM_HANDLER(ProcessCreateChar, SM_CREATECHARERROR)
        _INSTALL_SM_HANDLER(ProcessCreateChar, SM_CREATECHAROK)

        _INSTALL_SM_HANDLER(ProcessCreateAccount, SM_CREATEACCOUNTOK)
        _INSTALL_SM_HANDLER(ProcessCreateAccount, SM_CREATEACCOUNTERROR)

        _INSTALL_SM_HANDLER(ProcessChangePassword, SM_CHANGEPASSWORDERROR)
        _INSTALL_SM_HANDLER(ProcessChangePassword, SM_CHANGEPASSWORDOK)

        _INSTALL_SM_HANDLER(ProcessSelectChar, SM_DELETECHARERROR)
        _INSTALL_SM_HANDLER(ProcessSelectChar, SM_DELETECHAROK)
        _INSTALL_SM_HANDLER(ProcessSelectChar, SM_ONLINEERROR)
        _INSTALL_SM_HANDLER(ProcessSelectChar, SM_ONLINEOK)
        _INSTALL_SM_HANDLER(ProcessSelectChar, SM_QUERYCHARERROR)
        _INSTALL_SM_HANDLER(ProcessSelectChar, SM_QUERYCHAROK)

        _INSTALL_SM_HANDLER(ProcessRun, SM_ACTION)
        _INSTALL_SM_HANDLER(ProcessRun, SM_BELT)
        _INSTALL_SM_HANDLER(ProcessRun, SM_BUFF)
        _INSTALL_SM_HANDLER(ProcessRun, SM_BUFFIDLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_BUYERROR)
        _INSTALL_SM_HANDLER(ProcessRun, SM_BUYSUCCEED)
        _INSTALL_SM_HANDLER(ProcessRun, SM_CASTMAGIC)
        _INSTALL_SM_HANDLER(ProcessRun, SM_CORECORD)
        _INSTALL_SM_HANDLER(ProcessRun, SM_DEADFADEOUT)
        _INSTALL_SM_HANDLER(ProcessRun, SM_EQUIPBELT)
        _INSTALL_SM_HANDLER(ProcessRun, SM_EQUIPBELTERROR)
        _INSTALL_SM_HANDLER(ProcessRun, SM_EQUIPWEAR)
        _INSTALL_SM_HANDLER(ProcessRun, SM_EQUIPWEARERROR)
        _INSTALL_SM_HANDLER(ProcessRun, SM_EXP)
        _INSTALL_SM_HANDLER(ProcessRun, SM_FRIENDLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_CHATMESSAGELIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_GOLD)
        _INSTALL_SM_HANDLER(ProcessRun, SM_GRABBELT)
        _INSTALL_SM_HANDLER(ProcessRun, SM_GRABBELTERROR)
        _INSTALL_SM_HANDLER(ProcessRun, SM_GRABWEAR)
        _INSTALL_SM_HANDLER(ProcessRun, SM_GRABWEARERROR)
        _INSTALL_SM_HANDLER(ProcessRun, SM_GROUNDFIREWALLLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_GROUNDITEMIDLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_HEALTH)
        _INSTALL_SM_HANDLER(ProcessRun, SM_INVENTORY)
        _INSTALL_SM_HANDLER(ProcessRun, SM_INVOPCOST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_LEARNEDMAGICLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_MISS)
        _INSTALL_SM_HANDLER(ProcessRun, SM_NEXTSTRIKE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_NOTIFYDEAD)
        _INSTALL_SM_HANDLER(ProcessRun, SM_NPCNAME)
        _INSTALL_SM_HANDLER(ProcessRun, SM_NPCSELL)
        _INSTALL_SM_HANDLER(ProcessRun, SM_NPCXMLLAYOUT)
        _INSTALL_SM_HANDLER(ProcessRun, SM_NPCERROR)
        _INSTALL_SM_HANDLER(ProcessRun, SM_OFFLINE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_LOGOUTOK)
        _INSTALL_SM_HANDLER(ProcessRun, SM_CODISAPPEAR)
        _INSTALL_SM_HANDLER(ProcessRun, SM_REVIVE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_VIEWRANGE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_PICKUPERROR)
        _INSTALL_SM_HANDLER(ProcessRun, SM_PING)
        _INSTALL_SM_HANDLER(ProcessRun, SM_PLAYERCONFIG)
        _INSTALL_SM_HANDLER(ProcessRun, SM_PLAYERNAME)
        _INSTALL_SM_HANDLER(ProcessRun, SM_PLAYERSAY)
        _INSTALL_SM_HANDLER(ProcessRun, SM_PLAYERBROADCAST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_PLAYERWLDESP)
        _INSTALL_SM_HANDLER(ProcessRun, SM_QUESTDESPLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_QUESTDESPUPDATE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_REMOVEITEM)
        _INSTALL_SM_HANDLER(ProcessRun, SM_REMOVESECUREDITEM)
        _INSTALL_SM_HANDLER(ProcessRun, SM_SELLITEMLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_SHOWSECUREDITEMLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_STARTGAMESCENE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_STARTINPUT)
        _INSTALL_SM_HANDLER(ProcessRun, SM_STARTINVOP)
        _INSTALL_SM_HANDLER(ProcessRun, SM_STRIKEGRID)
        _INSTALL_SM_HANDLER(ProcessRun, SM_TEAMCANDIDATE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_TEAMMEMBERLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_TEXT)
        _INSTALL_SM_HANDLER(ProcessRun, SM_PARTEXT)
        _INSTALL_SM_HANDLER(ProcessRun, SM_UPDATEITEM)
        _INSTALL_SM_HANDLER(ProcessRun, SM_WEARITEMDURATION)
        _INSTALL_SM_HANDLER(ProcessRun, SM_CREATECHATGROUP)
        _INSTALL_SM_HANDLER(ProcessRun, SM_ADDFRIENDACCEPTED)
        _INSTALL_SM_HANDLER(ProcessRun, SM_ADDFRIENDREJECTED)
        _INSTALL_SM_HANDLER(ProcessRun, SM_RANKINGLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_AUCTIONITEMLIST)
        _INSTALL_SM_HANDLER(ProcessRun, SM_REQUESTDIRECTTRADE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_STARTDIRECTTRADE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_UPDATEDIRECTTRADE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_COMPLETEDIRECTTRADE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_CLOSEDIRECTTRADE)
        _INSTALL_SM_HANDLER(ProcessRun, SM_DIRECTTRADEERROR)

#undef _INSTALL_SM_HANDLER

        default:
            {
                throw fflpanic("no handler registered for server message {}", headCode);
            }
    }
}

void Client::switchProcess()
{
    if(m_quitRequested){
        shutdown();
        return;
    }
    if(m_requestProcess >= PROCESSID_BEGIN && m_requestProcess < PROCESSID_END){
        switchProcess(m_currentProcess ? m_currentProcess->id() : PROCESSID_NONE, m_requestProcess);
    }
    m_requestProcess = PROCESSID_NONE;
}

void Client::switchProcess(int newID)
{
    switchProcess((m_currentProcess ? m_currentProcess->id() : PROCESSID_NONE), newID);
}

void Client::switchProcess(int oldID, int newID)
{
    if(newID == PROCESSID_EXIT){ requestQuit(); shutdown(); return; }
    if(m_currentProcess){ observeAi({.phase=newID == PROCESSID_CONNECTIONERROR ? "disconnected" : "loading"}); }
    // Notify while the old process still exists, then drop all of its handlers.
    cancelResponses(ResponseCallbacks::CancelReason::ProcessChanged);

    m_currentProcess.reset();
    switch(oldID){
        case PROCESSID_NONE:
            {
                switch(newID)
                {
                    case PROCESSID_LOGO:
                        {
                            // on initialization
                            m_currentProcess = std::make_unique<ProcessLogo>();
                            SDL_HideCursor();
                            break;
                        }
                    case PROCESSID_CONNECTIONERROR:
                        {
                            m_currentProcess = std::make_unique<ProcessConnectionError>();
                            SDL_ShowCursor();
                            break;
                        }
                    case PROCESSID_EXIT:
                        {
                            // exit immediately when logo shows
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }
        case PROCESSID_LOGO:
            {
                switch(newID)
                {
                    case PROCESSID_SYRC:
                        {
                            // on initialization
                            m_currentProcess = std::make_unique<ProcessSync>();
                            SDL_ShowCursor();
                            break;
                        }
                    case PROCESSID_CONNECTIONERROR:
                        {
                            m_currentProcess = std::make_unique<ProcessConnectionError>();
                            SDL_ShowCursor();
                            break;
                        }
                    case PROCESSID_EXIT:
                        {
                            // exit immediately when logo shows
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }
        case PROCESSID_SYRC:
            {
                switch(newID)
                {
                    case PROCESSID_LOGIN:
                        {
                            m_currentProcess = std::make_unique<ProcessLogin>();
                            SDL_ShowCursor();
                            break;
                        }
                    case PROCESSID_CONNECTIONERROR:
                        {
                            m_currentProcess = std::make_unique<ProcessConnectionError>();
                            SDL_ShowCursor();
                            break;
                        }
                    default:
                        {
                            break;
                        }

                }
                break;
            }
        case PROCESSID_LOGIN:
            {
                switch(newID){
                    case PROCESSID_SELECTCHAR:
                        {
                            m_currentProcess = std::make_unique<ProcessSelectChar>();
                            break;
                        }
                    case PROCESSID_CREATEACCOUNT:
                        {
                            m_currentProcess = std::make_unique<ProcessCreateAccount>();
                            break;
                        }
                    case PROCESSID_CHANGEPASSWORD:
                        {
                            m_currentProcess = std::make_unique<ProcessChangePassword>();
                            break;
                        }
                    case PROCESSID_CONNECTIONERROR:
                        {
                            m_currentProcess = std::make_unique<ProcessConnectionError>();
                            SDL_ShowCursor();
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }
        case PROCESSID_CREATEACCOUNT:
            {
                switch(newID){
                    case PROCESSID_LOGIN:
                        {
                            m_currentProcess = std::make_unique<ProcessLogin>();
                            break;
                        }
                    case PROCESSID_CONNECTIONERROR:
                        {
                            m_currentProcess = std::make_unique<ProcessConnectionError>();
                            SDL_ShowCursor();
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }
        case PROCESSID_CHANGEPASSWORD:
            {
                switch(newID){
                    case PROCESSID_LOGIN:
                        {
                            m_currentProcess = std::make_unique<ProcessLogin>();
                            break;
                        }
                    case PROCESSID_CONNECTIONERROR:
                        {
                            m_currentProcess = std::make_unique<ProcessConnectionError>();
                            SDL_ShowCursor();
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }
        case PROCESSID_SELECTCHAR:
            {
                switch(newID){
                    case PROCESSID_RUN:
                        {
                            fflassert(m_smOOK.has_value());
                            m_currentProcess = std::make_unique<ProcessRun>(m_smOOK.value());
                            break;
                        }
                    case PROCESSID_CREATECHAR:
                        {
                            m_currentProcess = std::make_unique<ProcessCreateChar>();
                            break;
                        }
                    case PROCESSID_CONNECTIONERROR:
                        {
                            m_currentProcess = std::make_unique<ProcessConnectionError>();
                            SDL_ShowCursor();
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }
        case PROCESSID_CREATECHAR:
            {
                switch(newID){
                    case PROCESSID_SELECTCHAR:
                        {
                            m_currentProcess = std::make_unique<ProcessSelectChar>();
                            break;
                        }
                    case PROCESSID_CONNECTIONERROR:
                        {
                            m_currentProcess = std::make_unique<ProcessConnectionError>();
                            SDL_ShowCursor();
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }
        case PROCESSID_RUN:
            {
                switch(newID){
                    case PROCESSID_SELECTCHAR:
                        {
                            m_currentProcess = std::make_unique<ProcessSelectChar>();
                            break;
                        }
                    case PROCESSID_CONNECTIONERROR:
                        {
                            m_currentProcess = std::make_unique<ProcessConnectionError>();
                            SDL_ShowCursor();
                            break;
                        }
                    default:
                        {
                            break;
                        }
                }
                break;
            }
        default:
            {
                break;
            }
    }
    observeAi();
}

void Client::sendCMsgLog(uint8_t headCode)
{
    g_notifyBoard->addMessage(str_printf(u8"[%08.3f] ← %s", (float)(SDL_GetTicks()) / 1000.0f, ClientMsg(headCode).name().c_str()));
}

void Client::sendSMsgLog(uint8_t headCode)
{
    g_notifyBoard->addMessage(str_printf(u8"[%08.3f] → %s", (float)(SDL_GetTicks()) / 1000.0f, ServerMsg(headCode).name().c_str()));
}

void Client::PrintMonitor() const
{
    g_mir2xLog->addLog(LOGTYPE_INFO, "Client runs %llu msec", to_llu(m_clientTimer.diff_msec()));
    for(size_t nIndex = 0; nIndex < SM_END; ++nIndex){
        uint64_t nProcTick  = m_clientMonitor.SMProcMonitorList[nIndex].procTick / 1000000;
        uint64_t nRecvCount = m_clientMonitor.SMProcMonitorList[nIndex].recvCount;
        if(nRecvCount > 0){
            g_mir2xLog->addLog(LOGTYPE_INFO, "%s: recvCount = %llu, procTick = %llumsec", ServerMsg(nIndex).name().c_str(), to_llu(nRecvCount), to_llu(nProcTick));
        }
    }
}
