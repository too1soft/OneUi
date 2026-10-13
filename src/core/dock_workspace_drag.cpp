#include "dock_workspace_state.h"
#include "internal/ui_clock.h"
#include "oneui/animation.h"
#include <cmath>
namespace oneui {
bool DockWorkspaceView::interactionActive() const { return state_->gesture!=State::Gesture::None; }
bool DockWorkspaceView::hasDockTarget() const { return state_->hit && state_->hit->placement!=DockHitPlacement::None; }
void DockWorkspaceView::cancel() {
    auto& s=*state_;
    if(s.divider!=State::none)s.nodes[s.divider].ratio=s.beginRatio;
    s.gesture=State::Gesture::None;s.source=s.divider=State::none;s.keyboardDirty=false;
    s.hit.reset();s.previewActive=false;s.pendingRequest=0;
    layoutChildren();invalidate();
}
void DockWorkspaceView::showPreview(Rect r) {
    auto& s=*state_;s.previewTarget=r;
    if(!s.previewActive||!motion_||!motionEnabled())s.preview=r;
    s.previewActive=true;s.previewTime=internal::uiTimeMs();invalidate();requestAnimationFrame();
}
void DockWorkspaceView::updateMove(Point point) {
    auto& s=*state_;if(s.source==State::none)return;
    const auto& item=s.items[s.source];auto host=frame();
    Rect local=s.start;local.x+=point.x-s.origin.x;local.y+=point.y-s.origin.y;
    clampDockFloating(local,{0,0,host.width,host.height},item.view->floatMinimum,s.bounds);
    s.targets.clear();
    for(auto& target:s.items)if(target.shown&&target.view->visible())
        s.targets.push_back({target.identity,target.view->frame(),target.placement.mode=="floating"});
    s.hit=alt_||!s.snapshot.compactActive.empty()?std::nullopt:
        dockHitTest(item.identity,point,s.targets,item.placement.mode=="floating"?DockHitMode::GuideStrict:DockHitMode::GuideNearest);
    if(s.hit&&s.hit->placement!=DockHitPlacement::None)showPreview(s.hit->preview);
    else if(item.placement.mode=="floating") s.previewActive=false;
    else if(alt_) {
        auto r=s.bounds;r.x+=host.x;r.y+=host.y;showPreview(r);
    } else { auto r=s.start;r.x+=host.x;r.y+=host.y;showPreview(r); }
    // Floating content follows the pointer directly. Only the destination
    // overlay is smoothed; its geometry never participates in hit testing.
    layoutChildren();invalidate();
}
void DockWorkspaceView::drag(const std::string& source,int phase,Point point) {
    if(phase==3){cancel();return;}
    if(phase==0) {
        cancel();auto& s=*state_;auto found=s.itemIds.find(source);
        if(!interactive()||found==s.itemIds.end()||!s.snapshot.maximized.empty())return;
        const auto& item=s.items[found->second];
        if(!item.shown||(!s.snapshot.compactActive.empty()&&item.placement.mode!="floating"))return;
        s.source=found->second;s.gesture=State::Gesture::Move;s.origin=pointerOrigin_;
        auto r=item.view->frame(),host=frame();r.x-=host.x;r.y-=host.y;s.start=s.bounds=r;
    }
    if(state_->gesture!=State::Gesture::Move||state_->source==State::none||state_->items[state_->source].view->key!=source)return;
    updateMove(point);
    if(phase!=2)return;
    auto& s=*state_;const auto& item=s.items[s.source];
    DockIntent event{DockIntentKind::Bounds,source};event.bounds=s.bounds;
    bool send=false;
    if(s.hit&&s.hit->placement!=DockHitPlacement::None) {
        event.kind=DockIntentKind::Dock;
        for(auto& target:s.items)if(target.identity==s.hit->target)event.target=target.view->key;
        const char* edges[]={"","left","right","top","bottom"};event.edge=edges[int(s.hit->placement)];send=true;
    } else if(alt_||item.placement.mode=="floating") {
        event.kind=item.placement.mode=="floating"?DockIntentKind::Bounds:DockIntentKind::Float;send=true;
    }
    s.gesture=State::Gesture::None;s.hit.reset();s.previewActive=false;
    if(send)emit(std::move(event));else cancel();
}
bool DockWorkspaceView::tickAnimations(double now) {
    bool active=View::tickAnimations(now);auto& s=*state_;
    if(s.keyboardDirty&&now>=s.idleAt&&s.divider!=State::none) {
        DockIntent event{DockIntentKind::Ratio,s.snapshot.nodes[s.divider].id};event.ratio=s.nodes[s.divider].ratio;
        s.keyboardDirty=false;s.divider=State::none;emit(std::move(event));
    }
    auto& current=*state_;
    if(current.previewActive&&(!motion_||!motionEnabled()))current.preview=current.previewTarget;
    if(current.previewActive&&motion_&&motionEnabled()) {
        auto& r=current.preview;auto t=current.previewTarget;
        float k=float(1-std::exp(-std::max(now-current.previewTime,0.)/45.));current.previewTime=now;
        bool settled=std::abs(r.x-t.x)<.4f&&std::abs(r.y-t.y)<.4f&&std::abs(r.width-t.width)<.4f&&std::abs(r.height-t.height)<.4f;
        if(settled)r=t;
        else {r.x+=(t.x-r.x)*k;r.y+=(t.y-r.y)*k;r.width+=(t.width-r.width)*k;r.height+=(t.height-r.height)*k;active=true;}
        invalidate();
    }
    if(current.keyboardDirty)active=true;
    if(active)requestAnimationFrame();return active;
}
bool DockWorkspaceView::hasInteractionState() const{return state_->gesture!=State::Gesture::None||state_->keyboardDirty||View::hasInteractionState();}
void DockWorkspaceView::resetInteractionState(){cancel();View::resetInteractionState();}
} // namespace oneui
