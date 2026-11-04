#include "runtime.hpp"
#include "instancefile.hpp"
#include "gameadapter.hpp"
#include "combatadapter.hpp"
#include "npcadapter.hpp"
#include "tradeadapter.hpp"
#include "shopadapter.hpp"
#include "auctionadapter.hpp"
#include "controlpanel.hpp"
#include "clientargparser.hpp"
#include "client.hpp"
#include "processrun.hpp"
#include "myhero.hpp"
#include "textbutton.hpp"
#include "textboard.hpp"
#include "sdldevice.hpp"
#include "log.hpp"
#include <utility>
#include <algorithm>
#include <deque>
#include <map>

extern SDLDevice *g_sdlDevice;
extern Log *g_mir2xLog;
extern ClientArgParser *g_clientArgParser;

namespace aibridge
{
    struct AiBridgeRuntime::Impl
    {
        Client &client;
        AiBridgeTransport transport;
        std::unique_ptr<AiBridgeController> controller;
        std::unique_ptr<AiGameAdapter> game;
        std::unique_ptr<AiCombatAdapter> combat;
        std::unique_ptr<AiNpcAdapter> npc;
        std::unique_ptr<AiTradeAdapter> trade;
        std::unique_ptr<AiShopAdapter> shop;
        std::unique_ptr<AiAuctionAdapter> auction;
        RuntimeState state;
        std::string intentOperation;
        std::string intentKind;
        uint64_t intentRole = 0;
        std::map<int, uint64_t> intentFocus;
        std::deque<BridgeRequest> pending;
        std::shared_ptr<bool> callbackFault = std::make_shared<bool>(false);
        bool closed = false;
        AiControlPanel panel{[this]{return controller.get();},[this]{return transport.authenticated();}};

        explicit Impl(Client &c): client(c) {}

        MyHero *hero() const
        {
            if(auto run = client.getProcess<ProcessRun>()){
                return dynamic_cast<MyHero *>(run->findUID(run->getMyHeroUID(), false));
            }
            return nullptr;
        }

        std::vector<std::string> stopIntent()
        {
            if(intentOperation.empty()){ return {}; }
            auto kind = std::exchange(intentKind, {});
            intentOperation.clear();
            auto focus = std::exchange(intentFocus, {});
            if(auto run = client.getProcess<ProcessRun>(); run && run->getMyHeroUID() == intentRole){
                if(auto h = hero(); h && h->cancelAiIntent()){
                    for(const auto &[type, uid]: focus){
                        if(run->getFocusUID(type) == uid){ run->setFocusUID(type, 0); }
                    }
                    return kind=="queued" ? std::vector<std::string>{"queued"} : std::vector<std::string>{kind,"queued"};
                }
            }
            return {};
        }

        void refreshPanel()
        {
            panel.refresh();
        }
    };

    AiBridgeRuntime::AiBridgeRuntime(Client &client): m_impl(std::make_unique<Impl>(client)) {}
    AiBridgeRuntime::~AiBridgeRuntime(){ shutdown(); }

    void AiBridgeRuntime::start(const AiBridgeTransport::Options &options)
    {
        auto &s = *m_impl;
        try{
            s.transport.start(options);
            ControllerOptions config;
            config.instanceId = s.transport.instance()->instance_id;
            config.build = s.transport.instance()->build;
            config.policy = *s.transport.policy();
            config.autoEnable = g_clientArgParser->aiAutoEnable;
            config.auditDirectory = options.directory / ("audit-" + config.instanceId);
            s.controller = std::make_unique<AiBridgeController>(std::move(config));
            s.controller->setStopHook([&s]{ return s.stopIntent(); });
            s.game = std::make_unique<AiGameAdapter>(s.client, *this);
            s.game->registerTools();
            s.combat = std::make_unique<AiCombatAdapter>(s.client, *this);
            s.combat->registerTools();
            s.npc = std::make_unique<AiNpcAdapter>(s.client, *this);
            s.npc->registerTools();
            s.trade = std::make_unique<AiTradeAdapter>(s.client, *this);
            s.trade->registerTools();
            s.shop = std::make_unique<AiShopAdapter>(s.client, *this);
            s.shop->registerTools();
            s.auction = std::make_unique<AiAuctionAdapter>(s.client, *this);
            s.auction->registerTools();
            s.refreshPanel();
        }
        catch(...){
            s.transport.close();
            s.controller.reset();
            s.panel.close("初始化失败，请检查桥接配置");
            throw;
        }
    }

    void AiBridgeRuntime::shutdown() noexcept
    {
        auto &s = *m_impl;
        if(s.closed){ return; }
        s.closed = true;
        try{ s.panel.close("桥接已关闭"); }catch(...){}
        s.pending.clear();
        try{ if(s.controller){ s.controller->shutdown(); } }catch(...){}
        try{ s.stopIntent(); }catch(...){}
        s.transport.close();
    }

    bool AiBridgeRuntime::closeOnFault() noexcept
    {
        auto &s = *m_impl;
        if(s.closed){ return true; }
        try{
            if(!*s.callbackFault && (!s.controller || s.controller->pauseReason() != "controller_error")){ return false; }
        }
        catch(...){}
        shutdown();
        try{
            s.panel.close("桥接内部错误，已关闭；请重新启动客户端");
            const auto diagnostic = *s.callbackFault ? std::string("receipt_report/internal")
                : s.controller ? s.controller->faultDiagnostic() : std::string("controller/internal");
            g_mir2xLog->addLog(LOGTYPE_WARNING, "AI bridge closed after controller or receipt failure: %s", diagnostic.c_str());
        }
        catch(...){}
        return true;
    }

    void AiBridgeRuntime::observe(const RuntimeState &state)
    {
        auto &s = *m_impl;
        if(closeOnFault() || !s.controller){ return; }
        if(s.state != state){
            s.panel.resetGesture();
            s.controller->observe(state);
            if(closeOnFault()){ return; }
            if(s.controller->controlState() != ControlState::Active){ s.stopIntent(); }
            s.state = state;
        }
        s.refreshPanel();
    }

    void AiBridgeRuntime::poll()
    {
        auto &s = *m_impl;
        if(closeOnFault() || !s.controller){ return; }
        try{
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
            s.transport.pollBudget(16, std::chrono::milliseconds(1));
            for(const auto id: s.transport.takeDisconnected()){
                s.controller->onDisconnected(id);
                if(closeOnFault()){ return; }
                s.stopIntent();
                std::erase_if(s.pending, [id](const auto &request){ return request.connectionId == id; });
            }
            // Safety deadlines cannot be deferred by a saturated ordinary-work budget.
            s.controller->checkDeadlines();
            if(closeOnFault()){ return; }
            if(s.game){ s.game->beginPoll(); }
            const auto respond = [this, &s](const BridgeRequest &request)
            {
                const auto response = s.controller->handleRequest(request);
                if(closeOnFault()){ return false; }
                if(!s.transport.sendResponse(request.connectionId, response)){
                    s.controller->onDisconnected(request.connectionId);
                    s.stopIntent();
                }
                return !closeOnFault();
            };
            size_t processed = 0;
            auto incoming=s.transport.takeRequests(8);
            const auto isInterruption=[](const auto &request){
                return request.message.at("method") == "game_stop" && request.message.at("params").value("interrupt_only", false);
            };
            for(auto it=incoming.begin();it!=incoming.end();){
                if(it->message.at("method") == "game_stop" && !isInterruption(*it)){
                    // A stop must overtake even the bounded work deferred from an earlier poll.
                    if(!respond(*it)){ return; }
                    it=incoming.erase(it);
                    ++processed;
                }
                else{++it;}
            }
            const auto cancelBeforeAdvance=[&](auto &queue){
                for(auto it=queue.begin();it!=queue.end() && processed<8;){
                    if(isInterruption(*it)){
                        if(!respond(*it)){return false;}
                        it=queue.erase(it);++processed;
                    }
                    else{++it;}
                }
                return true;
            };
            if(!cancelBeforeAdvance(s.pending) || !cancelBeforeAdvance(incoming)){return;}
            const auto cancellationWaiting=[&]{
                const auto matches=[&](const auto &request){
                    return isInterruption(request) && s.controller->activeOperation() == request.message.at("params").at("operation_id").template get<std::string>();
                };
                return std::any_of(s.pending.begin(),s.pending.end(),matches) || std::any_of(incoming.begin(),incoming.end(),matches);
            };
            bool advanced=false;
            if(!cancellationWaiting() && s.controller->activeOperation() && std::chrono::steady_clock::now()<deadline){
                s.controller->advance(); advanced=true;
                if(closeOnFault()){ return; }
            }
            // Reserve one service slot for old work before fresh overflow can fill the response budget.
            if(!s.pending.empty() && processed<8){
                auto request=std::move(s.pending.front()); s.pending.pop_front();
                if(!respond(request)){ return; }
                ++processed;
            }
            for(auto &request:incoming){
                if(s.pending.size()<8){ s.pending.push_back(std::move(request)); }
                else{
                    ++processed;
                    if(!s.transport.sendResponse(request.connectionId,errorResponse(request.message.at("id"),"busy",s.controller->context()))){
                        s.controller->onDisconnected(request.connectionId); s.stopIntent();
                    }
                }
            }
            incoming.clear();
            // One bounded request may exceed the soft budget; observation cannot starve the queue.
            while(!s.pending.empty() && processed < 8 && (processed==0 || std::chrono::steady_clock::now() < deadline)){
                auto request = std::move(s.pending.front());
                s.pending.pop_front();
                if(!respond(request)){ return; }
                ++processed;
            }
            s.controller->checkDeadlines();
            if(closeOnFault()){ return; }
            if(!advanced && !cancellationWaiting() && std::chrono::steady_clock::now() < deadline){ s.controller->advance(); }
            if(closeOnFault()){ return; }
            if(s.game){ s.game->observe(); }
            if(s.trade){ s.trade->observe(); }
            if(s.shop){ s.shop->observe(); }
            if(s.auction){ s.auction->observe(); }
            if(s.npc){ s.npc->observe(); }
            if(closeOnFault()){ return; }
            s.refreshPanel();
        }
        catch(...){
            shutdown();
            s.panel.close("桥接运行失败，已关闭");
            g_mir2xLog->addLog(LOGTYPE_WARNING, "AI bridge stopped after runtime failure");
        }
    }

    bool AiBridgeRuntime::processEvent(const SDL_Event &event)
    {
        auto &s = *m_impl;
        if(closeOnFault() || !s.controller){ return false; }
        if(!ControlGesture::gameWindowEvent(event, SDL_GetWindowID(SDL_GetRenderWindow(g_sdlDevice->getRenderer())))){ return false; }
        if(s.client.getProcess<ProcessRun>() && s.panel.processEvent(event)){
            if(s.controller->controlState()!=ControlState::Active){ s.stopIntent(); }
            closeOnFault();
            return true;
        }
        if(ControlGesture::manual(event)){
            s.controller->manualInput();
            s.stopIntent();
            if(!closeOnFault()){ s.refreshPanel(); }
        }
        return false;
    }

    void AiBridgeRuntime::drawPanel() const
    {
        m_impl->panel.draw();
    }

    AiBridgeController *AiBridgeRuntime::controller() noexcept { return m_impl->controller.get(); }
    AiGameAdapter *AiBridgeRuntime::game() noexcept { return m_impl->game.get(); }
    AiNpcAdapter *AiBridgeRuntime::npc() noexcept { return m_impl->closed ? nullptr : m_impl->npc.get(); }
    AiTradeAdapter *AiBridgeRuntime::trade() noexcept { return m_impl->closed ? nullptr : m_impl->trade.get(); }
    AiShopAdapter *AiBridgeRuntime::shop() noexcept { return m_impl->closed ? nullptr : m_impl->shop.get(); }
    AiAuctionAdapter *AiBridgeRuntime::auction() noexcept { return m_impl->closed ? nullptr : m_impl->auction.get(); }
    const Policy *AiBridgeRuntime::policy() const noexcept { return m_impl->transport.policy(); }
    void AiBridgeRuntime::npcServerPanel(NpcPanelKind kind,uint64_t uid) noexcept
    {
        try{ if(auto adapter=npc()){ adapter->serverPanel(kind,uid); } }
        catch(...){
            shutdown();
            try{
                m_impl->panel.close("NPC 观察失败，桥接已关闭");
                g_mir2xLog->addLog(LOGTYPE_WARNING,"AI bridge closed after NPC panel observation failure");
            }catch(...){}
        }
    }

    void AiBridgeRuntime::tradeServerEvent(TradeEvent event,uint64_t uid,uint32_t error,uint64_t generation) noexcept
    {
        try{ if(auto adapter=trade()){ adapter->serverEvent(event,uid,error,generation); } }
        catch(...){
            shutdown();
            try{ g_mir2xLog->addLog(LOGTYPE_WARNING,"AI bridge closed after trade observation failure"); }catch(...){}
        }
    }
    void AiBridgeRuntime::shopBuyResult(uint64_t uid,uint32_t item,uint32_t seq,int error) noexcept
    {
        try{ if(auto adapter=shop()){ adapter->buyResult(uid,item,seq,error); } }
        catch(...){
            shutdown();
            try{ g_mir2xLog->addLog(LOGTYPE_WARNING,"AI bridge closed after shop observation failure"); }catch(...){}
        }
    }

    bool AiBridgeRuntime::claimIntent(std::string_view operationId, std::string_view kind)
    {
        auto &s = *m_impl;
        if(s.closed || !s.controller || s.controller->controlState() != ControlState::Active
            || s.controller->activeOperation() != operationId || (kind != "move" && kind != "attack" && kind != "follow" && kind != "queued")) { return false; }
        auto h = s.hero();
        if(!h || h->dead().value_or(true)){ return false; }
        if(s.intentOperation == operationId){ return true; }
        s.stopIntent();
        h->beginAiIntent();
        s.intentOperation = operationId;
        s.intentKind = kind;
        s.intentRole = h->UID();
        return true;
    }

    void AiBridgeRuntime::releaseIntent(std::string_view operationId)
    {
        if(m_impl->intentOperation == operationId){ m_impl->stopIntent(); }
    }

    bool AiBridgeRuntime::setIntentFocus(std::string_view operationId, int type, uint64_t uid)
    {
        auto &s = *m_impl;
        if(s.closed || !s.controller || s.intentOperation != operationId || s.controller->controlState() != ControlState::Active
            || (type != FOCUS_ATTACK && type != FOCUS_FOLLOW && type != FOCUS_MAGIC)){ return false; }
        if(auto run = s.client.getProcess<ProcessRun>(); run && run->getMyHeroUID() == s.intentRole){
            run->setFocusUID(type, uid);
            s.intentFocus[type] = uid;
            return true;
        }
        return false;
    }

    uint64_t AiBridgeRuntime::sendScoped(const OperationView &operation, const ClientMsgBuf &message, ReceiptDecoder decode)
    {
        auto &s = *m_impl;
        auto controller = s.controller.get();
        if(s.closed || !controller || !decode || controller->controlState() != ControlState::Active
            || controller->activeOperation() != operation.operationId || controller->context() != operation.context){
            throw ProtocolError("stale_context", "Operation is no longer current");
        }
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if(operation.deadlineMs <= uint64_t(now)){ throw ProtocolError("timeout", "Operation deadline passed"); }
        const auto life = controller->lifetime();
        const auto fault = s.callbackFault;
        const auto id = operation.operationId;
        const auto responseId = s.client.sendScoped(message, operation.deadlineMs - uint64_t(now),
            [life, fault, controller, id, decode=std::move(decode)](uint8_t code, const uint8_t *data, size_t size)
            {
                if(const auto held = life.lock(); held && !*fault){
                    // The controller retains terminal records and records late evidence without restarting work.
                    OperationUpdate result;
                    try{ result = decode(code, data, size); }
                    catch(...){
                        result.outcome = OperationOutcome::Unknown;
                        result.error = OperationError{"unavailable", "Receipt could not be decoded", Json::object()};
                    }
                    result.submitted = true;
                    try{ controller->report(id, result); }
                    catch(...){ *fault = true; }
                }
            },
            [life, fault, controller, id](ResponseCallbacks::CancelReason reason)
            {
                if(const auto held = life.lock(); held && !*fault){
                    OperationUpdate result;
                    result.submitted = reason != ResponseCallbacks::CancelReason::SendFailed;
                    result.outcome = result.submitted ? OperationOutcome::Unknown : OperationOutcome::Failed;
                    result.error = OperationError{reason == ResponseCallbacks::CancelReason::Timeout ? "timeout" : "unavailable", "Response scope ended", Json::object()};
                    try{ controller->report(id, result); }
                    catch(...){ *fault = true; }
                }
            });
        controller->report(id, OperationUpdate{.submitted=true});
        return responseId;
    }
}
