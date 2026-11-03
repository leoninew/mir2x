#include "controlpanel.hpp"
#include "sdldevice.hpp"
#include <algorithm>

extern SDLDevice *g_sdlDevice;

namespace aibridge
{
    AiControlPanel::AiControlPanel(std::function<AiBridgeController *()> controller, std::function<bool()> connected)
        : m_controller(std::move(controller)), m_connected(std::move(connected))
        , m_statusText{{.x=[this]{return int(bounds().x)+12;}, .y=[this]{return int(bounds().y)+8;},
            .textFunc=[this]{return m_status;}, .font{.size=15,.color=colorf::WHITE_A255}}}
        , m_roleText{{.x=[this]{return int(bounds().x)+12;}, .y=[this]{return int(bounds().y)+29;},
            .textFunc=[this]{return m_role;}, .font{.size=12,.color=colorf::WHITE_A255}}}
        , m_reasonText{{.x=[this]{return int(bounds().x)+12;}, .y=[this]{return int(bounds().y)+48;},
            .textFunc=[this]{return m_reason;}, .font{.size=12,.color=colorf::WHITE_A255}}}
        , m_enable{{.x=[this]{return int(enableBounds().x)+8;}, .y=[this]{return int(enableBounds().y)+4;},
            .textFunc=to_cstr(u8"开启 / 恢复 AI"), .font{.size=15}}}
        , m_disable{{.x=[this]{return int(disableBounds().x)+8;}, .y=[this]{return int(disableBounds().y)+4;},
            .textFunc=to_cstr(u8"关闭 AI"), .font{.size=15}}}
    {
        refresh();
    }

    SDL_FRect AiControlPanel::bounds() const
    {
        const auto [width,height]=g_sdlDevice->getWindowLogicalSize();
        // Leave the original bottom HUD (133 pixels plus its 21-pixel title) unobstructed.
        return {float(std::max(0,width-332)),float(std::max(0,height-266)),320,104};
    }
    SDL_FRect AiControlPanel::enableBounds() const
    {
        const auto rect=bounds();
        return {rect.x+12,rect.y+72,126,24};
    }
    SDL_FRect AiControlPanel::disableBounds() const
    {
        const auto rect=bounds();
        return {rect.x+218,rect.y+72,90,24};
    }
    void AiControlPanel::close(std::string reason)
    {
        m_closed=true;
        m_status="AI：故障 / 桥接已关闭";
        m_reason=std::move(reason);
        m_gesture.reset();
    }
    void AiControlPanel::refresh()
    {
        if(m_closed){ return; }
        const auto controller=m_controller();
        if(!controller){ m_status="AI：不可用"; return; }
        if(controller->pauseReason()=="controller_error"){
            m_status="AI：故障";
        }
        else if(!m_connected()){
            m_status=controller->controlState()==ControlState::Disabled ? "AI：已关闭 / 未连接" : "AI：未连接";
        }
        else{
            switch(controller->controlState()){
                case ControlState::Disabled: m_status="AI：已关闭"; break;
                case ControlState::ReadOnly: m_status="AI：只读 / 未开启"; break;
                case ControlState::Active: m_status="AI：控制中"; break;
                case ControlState::Paused: m_status="AI：已暂停"; break;
            }
        }
        if(SDL_GetTicks()<m_noticeUntil){ return; }
        const auto why=controller->pauseReason();
        if(why=="manual_input"){ m_reason="玩家操作后暂停，请用按钮恢复"; }
        else if(why=="death"){ m_reason="角色死亡，存活后用按钮恢复"; }
        else if(why=="life_unknown"){ m_reason="生命状态未知，确认存活后用按钮恢复"; }
        else if(why=="map_changed" || why=="session_changed"){ m_reason="世界已改变，请确认角色后恢复"; }
        else if(why=="transport_lost"){ m_reason="连接中断，重新绑定后用按钮恢复"; }
        else if(why=="disabled_by_user"){ m_reason="已关闭控制；认证只读查询仍可用"; }
        else if(why=="stop_requested" || why=="paused_by_user"){ m_reason="已停止，请用按钮恢复"; }
        else if(why=="controller_error"){ m_reason="内部故障，请重启客户端"; }
        else if(!m_connected()){ m_reason="等待认证宿主；不会自动改绑实例"; }
        else if(controller->controlState()==ControlState::ReadOnly){ m_reason="需有效角色；手动模式请用按钮开启"; }
        else{ m_reason="首次启动自动开启；玩家操作会暂停"; }
    }
    bool AiControlPanel::processEvent(const SDL_Event &event)
    {
        const auto controller=m_controller();
        if(m_closed || !controller){ return false; }
        const auto window=SDL_GetWindowID(SDL_GetRenderWindow(g_sdlDevice->getRenderer()));
        if(!ControlGesture::gameWindowEvent(event,window)){ return false; }
        const auto rect=bounds();
        // A resize between press/release must not move a grant onto another control.
        const auto key=controller->context().dump()+"/"+std::to_string(rect.x)+"/"+std::to_string(rect.y);
        const auto result=m_gesture.route(event,enableBounds(),disableBounds(),key);
        m_enable.setState(m_gesture.pendingButton()==1 ? BEVENT_DOWN : BEVENT_OFF);
        m_disable.setState(m_gesture.pendingButton()==2 ? BEVENT_DOWN : BEVENT_OFF);
        switch(result){
            case ControlGesture::Result::Enable:
                if(!controller->enableByUser()){
                    m_reason="需有效存活角色及认证连接；故障不可恢复";
                    m_noticeUntil=SDL_GetTicks()+2500;
                }
                else{ m_noticeUntil=0; }
                refresh();
                return true;
            case ControlGesture::Result::Pause:
                controller->disableByUser();
                m_noticeUntil=0;
                refresh();
                return true;
            case ControlGesture::Result::Consumed: return true;
            case ControlGesture::Result::Pass: return false;
        }
        return false;
    }
    void AiControlPanel::draw() const
    {
        const auto rect=bounds();
        g_sdlDevice->fillRectangle(colorf::RGBA(18,28,34,235),int(rect.x),int(rect.y),int(rect.w),int(rect.h));
        g_sdlDevice->drawRectangle(colorf::RGBA(102,170,145,255),int(rect.x),int(rect.y),int(rect.w),int(rect.h));
        SDLDeviceHelper::EnableRenderCropRectangle crop(int(rect.x),int(rect.y),int(rect.w),int(rect.h));
        m_statusText.drawRoot({}); m_roleText.drawRoot({}); m_reasonText.drawRoot({});
        if(!m_closed && m_controller()){
            for(const auto button:{enableBounds(),disableBounds()}){
                g_sdlDevice->fillRectangle(colorf::RGBA(35,55,60,255),int(button.x),int(button.y),int(button.w),int(button.h));
            }
            m_enable.drawRoot({}); m_disable.drawRoot({});
        }
    }
}
