#include "pane_grid_state.h"
#include "internal/split_geometry.h"
#include "internal/ui_clock.h"
#include <cmath>

namespace oneui {
size_t PaneGrid::dividerAt(Point point) const {
    const auto& s=*state_;
    if(!s.snapshot.maximized.empty()||s.phase!=PaneMotionPhase::Idle)return State::none;
    for(size_t i=0;i<s.nodes.size();++i) {
        const auto& n=s.nodes[i];if(n.first==State::none)continue;
        auto rect=n.divider;const auto hit=std::max(s.snapshot.gap,s.snapshot.dividerHit);
        if(s.snapshot.nodes[i].axis==DockAxis::Horizontal){rect.x+=(rect.width-hit)*.5f;rect.width=hit;}
        else {rect.y+=(rect.height-hit)*.5f;rect.height=hit;}
        if(rect.width>0 && rect.height>0 && rect.contains(point))return i;
    }
    return State::none;
}
void PaneGrid::updateDivider(Point point) {
    auto& s=*state_;if(s.divider==State::none)return;
    auto& n=s.nodes[s.divider];const auto horizontal=s.snapshot.nodes[s.divider].axis==DockAxis::Horizontal;
    auto size=horizontal?n.bounds.width:n.bounds.height;auto available=std::max(0.f,size-s.snapshot.gap);
    if(available<=0)return;
    float ratio=((horizontal?point.x-n.bounds.x:point.y-n.bounds.y)-s.grab-s.snapshot.gap*.5f)/available;
    auto a=s.nodes[n.first].minimum,b=s.nodes[n.second].minimum;
    ratio=detail::splitAmount(available,std::clamp(ratio,0.f,1.f),horizontal?a.width:a.height,horizontal?b.width:b.height)/available;
    if(std::abs(ratio-n.to)<.00001f)return;
    n.to=ratio;s.dirtyDivider=true;s.idleAt=internal::uiTimeMs()+s.snapshot.resizeIdleMs;
    hold(true);arrange(1,true);invalidate();requestAnimationFrame();
}
void PaneGrid::settleDivider() {
    state_->dirtyDivider=false;hold(false);invalidate();
}
void PaneGrid::cancelDivider() {
    auto& s=*state_;if(s.divider==State::none)return;
    s.nodes[s.divider].from=s.nodes[s.divider].to=s.beginRatio;
    s.dragging=false;s.dirtyDivider=false;s.divider=State::none;
    arrange(1,true);hold(false);invalidate();
}
bool PaneGrid::onMouseDown(const MouseEvent& event) {
    if(!interactive())return false;
    auto index=dividerAt(event.position);
    if(event.button!=MouseButton::Left||index==State::none)return View::onMouseDown(event);
    auto& s=*state_;s.divider=s.focusedDivider=index;s.beginRatio=s.nodes[index].current;
    onFocusChanged(true);setFocusVisible(false);
    if(event.clickCount==2) {
        s.nodes[index].from=s.nodes[index].to=.5f;arrange(1,true);s.divider=State::none;
        WorkspaceIntent e{WorkspaceIntentKind::Ratio,s.snapshot.nodes[index].id};e.value=.5;emit(std::move(e));return true;
    }
    auto rect=s.nodes[index].divider;bool h=s.snapshot.nodes[index].axis==DockAxis::Horizontal;
    s.grab=h?event.position.x-(rect.x+rect.width*.5f):event.position.y-(rect.y+rect.height*.5f);
    s.dragging=true;return true;
}
bool PaneGrid::onMouseMove(const MouseEvent& event) {
    if(state_->dragging){updateDivider(event.position);return true;}
    const auto hover=pressedChild()?State::none:dividerAt(event.position);
    if(hover!=state_->hoveredDivider){state_->hoveredDivider=hover;invalidate();}
    if(pressedChild())return View::onMouseMove(event);
    return hover!=State::none || View::onMouseMove(event);
}
bool PaneGrid::onMouseUp(const MouseEvent& event) {
    if(!state_->dragging)return View::onMouseUp(event);
    updateDivider(event.position);auto& s=*state_;auto index=s.divider;
    const auto ratio=s.nodes[index].to;s.dragging=false;s.divider=State::none;settleDivider();
    WorkspaceIntent e{WorkspaceIntentKind::Ratio,s.snapshot.nodes[index].id};e.value=ratio;emit(std::move(e));return true;
}
bool PaneGrid::onKeyDown(const KeyEvent& event) {
    auto& s=*state_;
    if(event.pressed && event.key==Key::Escape && s.previewRequest){clearPreview();return true;}
    if(event.pressed && event.key==Key::Escape && (s.dragging||s.dirtyDivider)){
        WorkspaceIntent intent{WorkspaceIntentKind::Ratio,s.snapshot.nodes[s.divider].id};intent.value=s.beginRatio;
        cancelDivider();emit(std::move(intent));return true;
    }
    auto index=s.focusedDivider;
    if(!interactive()||!event.pressed||event.control||event.alt||event.win||s.dragging||index==State::none)
        return View::onKeyDown(event);
    auto& n=s.nodes[index];bool h=s.snapshot.nodes[index].axis==DockAxis::Horizontal;
    float ratio=n.to;
    if(event.key==Key::Home)ratio=0;
    else if(event.key==Key::End)ratio=1;
    else if(event.key==(h?Key::Left:Key::Up))ratio-=.025f;
    else if(event.key==(h?Key::Right:Key::Down))ratio+=.025f;
    else return View::onKeyDown(event);
    auto available=std::max(0.f,(h?n.bounds.width:n.bounds.height)-s.snapshot.gap);
    if(available<=0)return true;
    auto a=s.nodes[n.first].minimum,b=s.nodes[n.second].minimum;
    ratio=detail::splitAmount(available,std::clamp(ratio,0.f,1.f),h?a.width:a.height,h?b.width:b.height)/available;
    if(!s.dirtyDivider)s.beginRatio=n.current;
    n.from=n.to=ratio;s.divider=index;s.dirtyDivider=true;s.idleAt=internal::uiTimeMs()+s.snapshot.resizeIdleMs;
    hold(true);arrange(1,true);invalidate();requestAnimationFrame();
    WorkspaceIntent e{WorkspaceIntentKind::Ratio,s.snapshot.nodes[index].id};e.value=ratio;emit(std::move(e));return true;
}
bool PaneGrid::onFocusChanged(bool focused) {
    bool changed=View::onFocusChanged(focused);
    if(focused && state_->focusedDivider==State::none && state_->snapshot.maximized.empty()) {
        for(size_t i=0;i<state_->nodes.size();++i)if(state_->nodes[i].first!=State::none) {
            state_->focusedDivider=i;break;
        }
    }
    if(!focused && state_->dragging)cancelDivider();
    return changed;
}
CursorKind PaneGrid::cursor(Point point) const {
    auto i=state_->dragging?state_->divider:dividerAt(point);
    if(i!=State::none)return state_->snapshot.nodes[i].axis==DockAxis::Horizontal?CursorKind::ResizeHorizontal:CursorKind::ResizeVertical;
    return View::cursor(point);
}
bool PaneGrid::hasInteractionState() const {return state_->dragging||state_->dirtyDivider||state_->hoveredDivider!=State::none||View::hasInteractionState();}
void PaneGrid::resetInteractionState() {cancelDivider();state_->hoveredDivider=State::none;View::resetInteractionState();invalidate();}
} // namespace oneui
