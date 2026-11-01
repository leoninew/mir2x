#pragma once
#include <atomic>
#include <string>
#include <type_traits>
#include <SDL3/SDL.h>

#include "stdf.hpp"
#include "netio.hpp"
#include "process.hpp"
#include "message.hpp"
#include "sdldevice.hpp"
#include "raiitimer.hpp"
#include "responsecallbacks.hpp"

class ProcessRun;
namespace aibridge { class AiBridgeRuntime; struct RuntimeState; }
class Client final
{
    private:
        struct SMProcMonitor
        {
            uint64_t procTick  = 0;
            uint32_t recvCount = 0;
        };

        struct ClientMonitor
        {
            std::array<SMProcMonitor, SM_END> SMProcMonitorList;
            ClientMonitor()
                : SMProcMonitorList()
            {}
        };

    private:
        std::string m_token;

    private:
        ClientMonitor m_clientMonitor;

    private:
        hres_timer m_clientTimer;

    private:
        double m_serverDelay;
        double m_netPackTick;

    private:
        NetIO m_netIO;
        ResponseCallbacks m_respHandlers;

    private:
        int m_requestProcess = PROCESSID_NONE;
        std::unique_ptr<Process> m_currentProcess;
        std::unique_ptr<aibridge::AiBridgeRuntime> m_aiRuntime;
        bool m_quitRequested = false;
        bool m_shutdown = false;
        uint64_t m_renderSequence = 0;

    private:
        std::optional<SMOnlineOK> m_smOOK;

    private:
        std::string m_clipboardBuf;

    public:
        Client();

    public:
        ~Client();

    public:
        void mainLoop();
        void requestQuit() noexcept { m_quitRequested = true; }
        void shutdown() noexcept;
        void logoutAi();
        void observeAi();
        void observeAi(const aibridge::RuntimeState &);
        void drawAiPanel() const;
        aibridge::AiBridgeRuntime *aiRuntime() const noexcept { return m_aiRuntime.get(); }
        uint64_t renderSequence() const noexcept { return m_renderSequence; }

    public:
        void Clipboard(const std::string &szInfo)
        {
            m_clipboardBuf = szInfo;
        }

        std::string Clipboard()
        {
            return m_clipboardBuf;
        }

    public:
        void requestProcess(int processID)
        {
            m_requestProcess = processID;
        }

    private:
        void initializeAi();
        void switchProcess();
        void switchProcess(int);
        void switchProcess(int, int);

    public:
        void initASIO();

    private:
        void onServerMessage(uint8_t, const uint8_t *, size_t);

    private:
        void eventDelay(double);
        void processEvent();

    private:
        void draw()
        {
            if(m_currentProcess){
                m_currentProcess->draw();
                ++m_renderSequence;
            }
        }

        void update(double fUpdateTime)
        {
            if(m_currentProcess){
                m_currentProcess->update(fUpdateTime);
                switchProcess();
            }
        }

    public:
        Process *ProcessValid(int processID)
        {
            return (m_currentProcess && m_currentProcess->id() == processID) ? m_currentProcess.get() : nullptr;
        }

    public:
        template<typename T> auto getProcess() const
        {
            return m_currentProcess ? dynamic_cast<T *>(m_currentProcess.get()) : nullptr;
        }

    public:
        void setOnlineOK(const SMOnlineOK &smOOK)
        {
            m_smOOK = smOOK;
        }

    private:
        void sendCMsgLog(uint8_t);
        void sendSMsgLog(uint8_t);

    public:
        void send(const ClientMsgBuf &, ResponseCallbacks::Handler = {});
        uint64_t sendScoped(const ClientMsgBuf &, uint64_t timeoutMS, ResponseCallbacks::Handler, ResponseCallbacks::OnCancel = {});
        void cancelResponses(ResponseCallbacks::CancelReason) noexcept;

    private:
        uint64_t sendWithResponse(const ClientMsgBuf &, ResponseCallbacks::Handler, std::optional<uint64_t>, ResponseCallbacks::OnCancel);

    public:
        void PrintMonitor() const;

    public:
        void setToken(std::string token)
        {
            m_token = std::move(token);
        }

        const std::string &getToken() const
        {
            return m_token;
        }
};
