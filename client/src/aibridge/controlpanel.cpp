#include "controlpanel.hpp"
#include "sdldevice.hpp"

extern SDLDevice *g_sdlDevice;

namespace aibridge
{
    AiControlPanel::AiControlPanel(std::function<AiBridgeController *()> controller, std::function<bool()> connected)
        : m_controller(std::move(controller)), m_connected(std::move(connected))
        , m_statusText{{.x=[this]{return int(bounds().x)+10;}, .y=[this]{return int(bounds().y)+8;},
            .textFunc=[this]{return m_status;}, .font{.size=14,.color=colorf::WHITE_A255}}}
    {
        refresh();
    }

    SDL_FRect AiControlPanel::bounds() const
    {
        return {12,12,float(m_statusText.w()+20),float(m_statusText.h()+16)};
    }
    void AiControlPanel::close(std::string)
    {
        m_closed=true;
        m_gesture.reset();
        refresh();
    }
    void AiControlPanel::refresh()
    {
        const auto controller=m_controller();
        m_active=!m_closed && controller && m_connected() && controller->controlState()==ControlState::Active;
        m_status=m_active ? "AI接管中" : "AI未接管";
    }
    bool AiControlPanel::processEvent(const SDL_Event &event)
    {
        const auto controller=m_controller();
        if(m_closed || !controller){ return false; }
        refresh();
        const auto window=SDL_GetWindowID(SDL_GetRenderWindow(g_sdlDevice->getRenderer()));
        if(!ControlGesture::gameWindowEvent(event,window)){ return false; }
        const auto rect=bounds();
        const auto [width,height]=g_sdlDevice->getWindowLogicalSize();
        // The fixed corner does not move on resize; bind the gesture to viewport and action too.
        const auto key=controller->context().dump()+"/"+std::to_string(width)+"/"+std::to_string(height)+(m_active ? "/stop" : "/start");
        const auto result=m_gesture.route(event,m_active ? SDL_FRect{} : rect,m_active ? rect : SDL_FRect{},key);
        switch(result){
            case ControlGesture::Result::Enable:
                controller->enableByUser();
                refresh();
                return true;
            case ControlGesture::Result::Pause:
                controller->disableByUser();
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
        g_sdlDevice->fillRectangle(m_gesture.pendingButton() ? colorf::RGBA(35,55,60,235) : colorf::RGBA(18,28,34,235),int(rect.x),int(rect.y),int(rect.w),int(rect.h));
        g_sdlDevice->drawRectangle(m_active ? colorf::RGBA(102,170,145,255) : colorf::RGBA(95,110,115,255),int(rect.x),int(rect.y),int(rect.w),int(rect.h));
        SDLDeviceHelper::EnableRenderCropRectangle crop(int(rect.x),int(rect.y),int(rect.w),int(rect.h));
        m_statusText.drawRoot({});
    }
}
