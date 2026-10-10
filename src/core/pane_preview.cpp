#include "pane_grid_state.h"
#include <cmath>
namespace oneui {
void PaneGrid::setAccent(Color color){state_->accent=color;invalidate();}
void PaneGrid::showPreview(Rect target,bool valid) {
    auto& s=*state_;s.previewTarget=target;s.previewValid=valid;
    if(!s.previewActive || !motionEnabled_ || !s.snapshot.motion){s.preview=target;s.previewTime=0;}
    s.previewActive=true;invalidate();requestAnimationFrame();
}
void PaneGrid::clearPreview(){state_->previewRequest=0;if(state_->previewActive){state_->previewActive=false;state_->previewTime=0;invalidate();}}
std::optional<Rect> PaneGrid::previewRect() const{return state_->previewActive?std::optional<Rect>(state_->preview):std::nullopt;}
bool PaneGrid::tickPreview(double now) {
    auto& s=*state_;if(!s.previewActive)return false;
    const auto dt=s.previewTime?std::clamp(now-s.previewTime,0.,50.):16.;s.previewTime=now;
    const float amount=float(1-std::exp(-dt/45.));
    auto near=[](float a,float b){return std::abs(a-b)<.4f;};
    auto& p=s.preview;auto t=s.previewTarget;
    if(near(p.x,t.x)&&near(p.y,t.y)&&near(p.width,t.width)&&near(p.height,t.height)) {
        if(p.x!=t.x||p.y!=t.y||p.width!=t.width||p.height!=t.height){p=t;invalidate();}return false;
    }
    p.x+=(t.x-p.x)*amount;p.y+=(t.y-p.y)*amount;p.width+=(t.width-p.width)*amount;p.height+=(t.height-p.height)*amount;
    invalidate();requestAnimationFrame();return true;
}
} // namespace oneui
