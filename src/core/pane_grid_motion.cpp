#include "pane_grid_state.h"
#include "oneui/ui_native_host.h"
#include <algorithm>

namespace oneui {
bool PaneGrid::contentReady() {
    auto& s=*state_;const bool ready=scanContent(s);publishContent(s);return ready;
}
bool PaneGrid::scanContent(State& s) {
    bool ready=true;
    std::vector<State::Terminal> terminals;
    for(auto p:s.visibleOrder) {
        auto pane=s.panes[p].view;
        std::vector<std::shared_ptr<Widget>> pending{pane};
        while(!pending.empty()) {
            auto item=std::move(pending.back());pending.pop_back();
            if(auto host=std::dynamic_pointer_cast<ui::NativeHostView>(item)) {
                if(!host->settledViewport())continue;
                if(host->children().empty())ready=false;
            }
            if(auto terminal=std::dynamic_pointer_cast<ContentViewportSource>(item))terminals.push_back({terminal,pane->key});
            else if(auto view=std::dynamic_pointer_cast<View>(item))
                for(auto& child:view->children())pending.push_back(child);
        }
    }
    for(auto& old:s.terminals)if(std::none_of(terminals.begin(),terminals.end(),[&](const auto& t){return t.view==old.view;})) {
        s.retiredTerminals.push_back(old);
    }
    auto oldTerminals=std::move(s.terminals);
    s.terminals=std::move(terminals);
    auto life=lifetimeToken();
    for(auto& terminal:s.terminals) {
        if(std::any_of(oldTerminals.begin(),oldTerminals.end(),[&](const auto& t){return t.view==terminal.view && t.pane==terminal.pane;}))continue;
        terminal.observer=[this,life,key=terminal.pane](ContentViewport viewport) {
            if(life.expired()||state_->held)return;
            auto& pending=state_->pendingViewports;
            auto found=std::find_if(pending.begin(),pending.end(),[&](const auto& v){return v.pane==key;});
            State::Viewport value{key,viewport,generation_};
            if(found==pending.end())pending.push_back(std::move(value));else *found=std::move(value);
            requestAnimationFrame();
        };
    }
    return ready;
}
void PaneGrid::publishContent(State& s) {
    for(auto& old:s.retiredTerminals) {
        old.view->setWorkspaceViewportObserver({});old.view->holdViewportNotifications(false);
    }
    s.retiredTerminals.clear();
    for(auto& terminal:s.terminals) {
        terminal.view->holdViewportNotifications(s.held);
        if(terminal.observer)terminal.view->setWorkspaceViewportObserver(std::move(terminal.observer));
    }
}
void PaneGrid::hold(bool value) {
    state_->held=value;
    for(auto& terminal:state_->terminals)terminal.view->holdViewportNotifications(value);
}
void PaneGrid::finishMotion() {
    auto saved=state_;auto& s=*saved;
    s.phase=PaneMotionPhase::FinalLayout;arrange(1,true);
    for(auto p:s.visibleOrder) {
        auto& pane=s.panes[p];pane.shown=s.snapshot.maximized.empty()||s.snapshot.maximized==pane.view->key;
        pane.view->setVisible(pane.shown);
    }
    hold(false);s.phase=PaneMotionPhase::Idle;
    invalidate();
    for(const auto& [pane,request]:s.closing) {
        if(state_!=saved)break;
        s.completedClosing[pane]=request;
        WorkspaceIntent event{WorkspaceIntentKind::Transition,pane};event.request=request;emit(std::move(event));
    }
}
bool PaneGrid::tickAnimations(double now) {
    for(unsigned handled=0;!state_->navigation.empty();++handled) {
        if(handled==8){state_->navigationOverflow+=state_->navigation.size();state_->navigation.clear();break;}
        auto command=std::move(state_->navigation.front());state_->navigation.erase(state_->navigation.begin());
        navigate(command.edge,command.swap);
    }
    auto saved=state_;
    bool active=View::tickAnimations(now);active=tickPreview(now)||active;auto& s=*saved;
    if(!s.held && !s.pendingViewports.empty()) {
        auto pending=std::move(s.pendingViewports);
        for(auto& value:pending) {
            if(state_!=saved)break;
            auto p=s.paneIds.find(value.pane);
            if(value.generation!=generation_ || p==s.paneIds.end() || !s.panes[p->second].shown)continue;
            WorkspaceIntent event{WorkspaceIntentKind::Resize,value.pane};
            event.columns=value.size.columns;event.rows=value.size.rows;event.request=int64_t(value.generation);
            emit(std::move(event));
        }
    }
    if(state_!=saved)return true;
    if(!s.ready)s.ready=contentReady();
    if(s.dirtyDivider && now>=s.idleAt)settleDivider();
    switch(s.phase) {
    case PaneMotionPhase::FinalLayout:finishMotion();return true;
    case PaneMotionPhase::Preparing:
        if(!s.ready)return active;
        s.phase=PaneMotionPhase::WarmFrame;
        invalidate();
        requestAnimationFrame();return true;
    case PaneMotionPhase::WarmFrame:
        if(!s.warmPainted){invalidate();requestAnimationFrame();return true;}
        s.started=now;s.phase=PaneMotionPhase::Running;
        requestAnimationFrame();return true;
    case PaneMotionPhase::Running: {
        float t=float(std::clamp((now-s.started)/s.snapshot.motionMs,0.,1.));
        arrange(1-(1-t)*(1-t)*(1-t),false);
        invalidate();
        if(t>=1)finishMotion();else requestAnimationFrame();
        return true;
    }
    default:break;
    }
    return active||s.dirtyDivider;
}
} // namespace oneui
