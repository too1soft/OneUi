#include "pane_grid_state.h"
#include "oneui/style_sheet.h"
#include "internal/split_geometry.h"
#include "internal/ui_clock.h"
#include <cmath>
#include <stdexcept>

namespace oneui {
void PaneView::layoutChildren() {
    auto bounds=frame();
    auto heading=header.lock();
    if(heading && heading->visible()) {
        auto rect=bounds;rect.height=std::min(30.f,bounds.height);heading->setFrame(rect);
        bounds.y+=rect.height;bounds.height-=rect.height;
    }
    bounds.x+=16;bounds.y+=8;
    bounds.width=std::max(0.f,bounds.width-32);bounds.height=std::max(0.f,bounds.height-16);
    for (const auto& child:children()) {
        if (child->visible() && child!=heading)child->setFrame(bounds);
    }
}
PaneGrid::PaneGrid():state_(std::make_shared<State>()) {state_->navigation.reserve(8);}
PaneGrid::~PaneGrid() {
    intent_={};
    for (auto& terminal:state_->terminals) {
        terminal.view->setWorkspaceViewportObserver({});
        terminal.view->holdViewportNotifications(false);
    }
}
const PaneGridSnapshot& PaneGrid::snapshot() const {return state_->snapshot;}
PaneMotionPhase PaneGrid::motionPhase() const {return state_->phase;}
uint64_t PaneGrid::motionGeneration() const {return generation_;}
bool PaneGrid::resizeHeld() const {return state_->held;}
void PaneGrid::setOnIntent(std::function<void(const WorkspaceIntent&)> callback) {intent_=std::move(callback);}
void PaneGrid::setMotionEnabled(bool enabled) {motionEnabled_=enabled;if(!enabled && state_->phase!=PaneMotionPhase::Idle)finishMotion();}
void PaneGrid::emit(WorkspaceIntent event) {
    event.revision=state_->snapshot.revision;
    if (!event.request) event.request=++request_;
    if(event.kind==WorkspaceIntentKind::Drop)state_->previewRequest=event.request;
    if (intent_) {auto callback=intent_;callback(event);}
}
Size PaneGrid::naturalSize() const {return state_->index.minimum(state_->snapshot.activeGroup);}
void PaneGrid::setSnapshot(PaneGridSnapshot snapshot,std::vector<std::shared_ptr<PaneView>> panes) {
    auto children=std::vector<std::shared_ptr<Widget>>(panes.begin(),panes.end());
    auto publish=prepare(std::move(snapshot),std::move(panes));
    auto plan=prepareChildren(std::move(children));plan->commit();publish();
}
void PaneGrid::layoutChildren() {
    if(state_->phase==PaneMotionPhase::Preparing){arrange(0,false);return;}
    if(state_->phase==PaneMotionPhase::Running || state_->phase==PaneMotionPhase::WarmFrame)return;
    arrange(1,true);
}
void PaneGrid::arrange(float progress,bool minimums) {
    auto& s=*state_;
    if(s.root==State::none)return;
    // Collapsed siblings must not reserve invisible dividers. Keep the gap
    // on the same motion clock, including redirects and restoration.
    s.currentGap=s.fromGap+(s.toGap-s.fromGap)*progress;
    auto bounds=frame();
    if(minimums && s.snapshot.maximized.empty()) {
        const auto size=s.index.minimum(s.snapshot.activeGroup);
        bounds.width=std::max(bounds.width,size.width);bounds.height=std::max(bounds.height,size.height);
    }
    size_t count=0;s.work[count++]={s.root,bounds};
    while(count) {
        const auto work=s.work[--count];auto& n=s.nodes[work.node];n.bounds=work.rect;
        if(n.pane!=State::none) {
            auto& p=s.panes[n.pane];
            if(p.shown)p.view->setFrame(work.rect);
            continue;
        }
        const bool horizontal=s.snapshot.nodes[work.node].axis==DockAxis::Horizontal;
        const auto gap=std::min(s.currentGap,horizontal?work.rect.width:work.rect.height);
        const auto extent=std::max(0.f,(horizontal?work.rect.width:work.rect.height)-gap);
        n.current=n.from+(n.to-n.from)*progress;
        if(s.divider==work.node)n.current=n.to;
        auto amount=extent*n.current;
        if(minimums && s.snapshot.maximized.empty() && s.closing.empty()) {
            auto a=s.nodes[n.first].minimum,b=s.nodes[n.second].minimum;
            amount=detail::splitAmount(extent,n.current,horizontal?a.width:a.height,horizontal?b.width:b.height);
        }
        auto rects=detail::splitRects(work.rect,gap,amount,horizontal);n.divider=rects.divider;
        s.work[count++]={n.second,rects.second};s.work[count++]={n.first,rects.first};
    }
}
void PaneGrid::paint(Canvas& canvas) {
    // More than one WM_PAINT may precede a timer tick. Only the first completed
    // presentation is warm; a second paint starts the clock at unchanged t=0.
    if(state_->phase==PaneMotionPhase::WarmFrame && state_->warmPainted) {
        state_->phase=PaneMotionPhase::Running;
        state_->started=internal::uiTimeMs();
        requestAnimationFrame();
    }
    View::paint(canvas);
    auto& s=*state_;
    if(s.snapshot.maximized.empty())for(size_t i=0;i<s.nodes.size();++i) {
        const auto& n=s.nodes[i];if(n.first==State::none || n.divider.width<=0 || n.divider.height<=0)continue;
        const bool hovered=i==s.hoveredDivider, pressed=s.dragging && i==s.divider;
        const bool focused=focusVisible() && i==s.focusedDivider;
        Color color=(hovered||pressed||focused)?s.accent:Color{90,100,110,70};
        if(auto sheet=styleScope()) {
            StyleNode node{"pane-grid-part",{"pane-grid-divider"},
                (hovered?StyleStateHover:0u)|(pressed?StyleStateActive:0u)|(focused?StyleStateFocus:0u)};
            color=sheet->resolve(node).background.color.value_or(color);
        }
        canvas.fillRect(n.divider,color);
    }
    if(s.previewActive) {
        auto accent=s.accent;
        if(auto sheet=styleScope()) {
            StyleNode node{"pane-grid-part",{"pane-grid-preview"},0};
            accent=sheet->resolve(node).background.color.value_or(accent);
        }
        auto fill=accent;fill.a=s.previewValid?41:15;
        canvas.fillRect(s.preview,fill,10);canvas.strokeRect(s.preview,accent,10,s.previewValid?2.f:1.f);
    }
    // Win32 may paint an invalidated snapshot before its first animation tick.
    // That completed initial-geometry paint is the warm frame. Do not paint a
    // second warm frame merely because the scheduler order was paint -> tick.
    // The next tick starts the 200 ms clock, exactly as in tick -> paint order.
    if(s.phase==PaneMotionPhase::Preparing && s.ready) {
        s.phase=PaneMotionPhase::WarmFrame;
        requestAnimationFrame();
    }
    if(s.phase==PaneMotionPhase::WarmFrame)s.warmPainted=true;
}
std::vector<WorkspaceRect> PaneGrid::visiblePanes() const {
    std::vector<WorkspaceRect> result;
    for(auto i:state_->visibleOrder) if(state_->panes[i].shown)result.push_back({state_->panes[i].view->key,state_->panes[i].view->frame()});
    return result;
}
} // namespace oneui
