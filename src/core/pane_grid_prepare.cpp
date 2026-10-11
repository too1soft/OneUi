#include "pane_grid_state.h"
#include "internal/split_geometry.h"
#include <cmath>
#include <stdexcept>

namespace oneui {
namespace {
bool sameSnapshot(const PaneGridSnapshot& a,const PaneGridSnapshot& b) {
    if(a.activeGroup!=b.activeGroup || a.activePane!=b.activePane || a.maximized!=b.maximized ||
       a.revision!=b.revision || a.ackRequest!=b.ackRequest || a.ackAccepted!=b.ackAccepted ||
       a.motion!=b.motion || a.gap!=b.gap || a.dividerHit!=b.dividerHit || a.motionMs!=b.motionMs ||
       a.resizeIdleMs!=b.resizeIdleMs || a.nodes.size()!=b.nodes.size() || a.groups.size()!=b.groups.size())return false;
    for(size_t i=0;i<a.nodes.size();++i) {
        const auto& x=a.nodes[i];const auto& y=b.nodes[i];
        if(x.id!=y.id||x.pane!=y.pane||x.first!=y.first||x.second!=y.second||x.axis!=y.axis||x.ratio!=y.ratio)return false;
    }
    for(size_t i=0;i<a.groups.size();++i)if(a.groups[i].id!=b.groups[i].id||a.groups[i].root!=b.groups[i].root)return false;
    return true;
}
}
std::function<void()> PaneGrid::prepare(PaneGridSnapshot snapshot,
                                      std::vector<std::shared_ptr<PaneView>> panes) {
    bool same=sameSnapshot(snapshot,state_->snapshot) && panes.size()==state_->panes.size();
    for(size_t i=0;same && i<panes.size();++i) {
        const auto& p=state_->panes[i];const auto& n=panes[i];
        same=n && n==p.view && n->minimum.width==p.minimum.width && n->minimum.height==p.minimum.height &&
             n->closing==p.closing && n->closeRequest==p.closeRequest;
    }
    if(same)return []{}; // Title/output dirtiness must not restart a motion epoch.
    auto next=std::make_shared<State>();next->snapshot=std::move(snapshot);
    auto& s=*next;const auto& previous=*state_;
    s.navigation=previous.navigation;s.navigation.reserve(8);
    s.navigationSerial=previous.navigationSerial;s.navigationOverflow=previous.navigationOverflow;s.accent=previous.accent;
    if(previous.previewRequest && s.snapshot.revision==previous.snapshot.revision && s.snapshot.ackRequest!=previous.previewRequest) {
        s.previewRequest=previous.previewRequest;s.previewActive=previous.previewActive;s.previewValid=previous.previewValid;
        s.preview=previous.preview;s.previewTarget=previous.previewTarget;s.previewTime=previous.previewTime;
    }
    s.completedClosing=previous.completedClosing;
    const auto& model=s.snapshot;
    if(!std::isfinite(model.motionMs)||model.motionMs<0||!std::isfinite(model.resizeIdleMs)||model.resizeIdleMs<0||
       !std::isfinite(model.dividerHit)||model.dividerHit<0)throw std::invalid_argument("Invalid workspace timing/hit size");
    std::vector<WorkspacePane> minimums;
    for(auto& pane:panes) {
        if(!pane || pane->key.empty() || !s.paneIds.emplace(pane->key,s.panes.size()).second)
            throw std::invalid_argument("Duplicate/empty pane key");
        minimums.push_back({pane->key,pane->minimum});s.panes.push_back({pane,false,pane->minimum,pane->closing,pane->closeRequest});
    }
    if(s.index.reset(model.nodes,model.groups,minimums,model.gap)!=DockError::None)
        throw std::invalid_argument("Invalid workspace tree");
    s.nodes.resize(model.nodes.size());s.work.resize(model.nodes.size()+1);
    for(auto it=s.completedClosing.begin();it!=s.completedClosing.end();) {
        auto found=s.paneIds.find(it->first);
        if(found==s.paneIds.end() || !s.panes[found->second].closing)it=s.completedClosing.erase(it);else ++it;
    }
    for(size_t i=0;i<model.nodes.size();++i)s.nodeIds.emplace(model.nodes[i].id,i);
    for(const auto& group:model.groups)if(group.id==model.activeGroup && !group.root.empty())s.root=s.nodeIds.at(group.root);
    if(!model.activeGroup.empty() && std::none_of(model.groups.begin(),model.groups.end(),[&](const auto& g){return g.id==model.activeGroup;}))
        throw std::invalid_argument("Active workspace group missing");
    s.toGap=model.maximized.empty()?model.gap:0.f;
    s.fromGap=previous.root==State::none?s.toGap:previous.currentGap;
    s.currentGap=s.fromGap;
    bool animate=std::abs(s.fromGap-s.toGap)>.00001f;
    for(size_t i=0;i<model.nodes.size();++i) {
        const auto& node=model.nodes[i];auto& n=s.nodes[i];
        n.from=n.to=n.current=node.ratio;
        if(!node.pane.empty()) {n.pane=s.paneIds.at(node.pane);n.minimum=s.panes[n.pane].view->minimum;}
        else {
            n.first=s.nodeIds.at(node.first);n.second=s.nodeIds.at(node.second);
            s.nodes[n.first].parent=i;s.nodes[n.second].parent=i;
            auto old=previous.nodeIds.find(node.id);
            if(old!=previous.nodeIds.end()) {
                n.from=previous.nodes[old->second].current;
                animate|=std::abs(n.from-n.to)>.00001f && !previous.dragging && previous.divider==State::none;
            }
        }
    }
    std::vector<std::pair<size_t,bool>> stack;
    if(s.root!=State::none)stack.push_back({s.root,false});
    std::vector<bool> oldContent(s.nodes.size());
    while(!stack.empty()) {
        auto [i,exit]=stack.back();stack.pop_back();auto& n=s.nodes[i];
        if(n.pane!=State::none) {
            auto& pane=s.panes[n.pane];pane.shown=model.maximized.empty()||model.maximized==pane.view->key;
            s.visibleOrder.push_back(n.pane);
            auto old=previous.paneIds.find(pane.view->key);
            oldContent[i]=old!=previous.paneIds.end() && previous.panes[old->second].shown;
            continue;
        }
        if(!exit) {stack.push_back({i,true});stack.push_back({n.second,false});stack.push_back({n.first,false});continue;}
        n.minimum=detail::splitMinimum(s.nodes[n.first].minimum,s.nodes[n.second].minimum,model.gap,
                                      model.nodes[i].axis==DockAxis::Horizontal);
        oldContent[i]=oldContent[n.first]||oldContent[n.second];
        auto oldNode=previous.nodeIds.find(model.nodes[i].id);
        const bool wasSplit=oldNode!=previous.nodeIds.end() && previous.nodes[oldNode->second].first!=State::none;
        if(previous.root!=State::none && !wasSplit && oldContent[n.first]!=oldContent[n.second]) {
            n.from=oldContent[n.first]?1.f:0.f;animate=true;
        }
    }
    if(!model.maximized.empty() && (s.paneIds.count(model.maximized)==0 ||
        std::find(s.visibleOrder.begin(),s.visibleOrder.end(),s.paneIds.at(model.maximized))==s.visibleOrder.end()))
        throw std::invalid_argument("Maximized pane outside active group");
    for(size_t i=0;i<s.nodes.size();++i) {
        auto& n=s.nodes[i];if(n.pane==State::none)continue;
        const auto& pane=s.panes[n.pane].view;
        if(pane->closing && (!s.completedClosing.count(pane->key) || s.completedClosing.at(pane->key)!=pane->closeRequest)) {
            if(n.parent!=State::none) {
                s.nodes[n.parent].to=s.nodes[n.parent].first==i?0.f:1.f;animate=true;
            }
            s.closing.emplace_back(pane->key,pane->closeRequest);
        }
        if(pane->key==model.maximized) {
            for(size_t child=i,parent=n.parent;parent!=State::none;child=parent,parent=s.nodes[parent].parent)
                s.nodes[parent].to=s.nodes[parent].first==child?1.f:0.f;
        }
    }
    if(previous.snapshot.maximized!=model.maximized && previous.root!=State::none)animate=true;
    // Keep siblings visible until maximize motion finishes, then remove them
    // from painting/hit testing without dropping their NativeHost or lease.
    if(previous.snapshot.activeGroup!=model.activeGroup)animate=false;
    if(previous.focusedDivider!=State::none) {
        auto oldId=previous.snapshot.nodes[previous.focusedDivider].id;
        if(s.nodeIds.count(oldId))s.focusedDivider=s.nodeIds.at(oldId);
    }
    // Keyboard requests may be accepted synchronously. Acceptance must not
    // release the 120 ms viewport hold between repeated key presses.
    if(previous.dirtyDivider && !previous.dragging && model.ackAccepted && previous.divider!=State::none) {
        auto oldId=previous.snapshot.nodes[previous.divider].id;
        auto found=s.nodeIds.find(oldId);
        if(found!=s.nodeIds.end() && std::abs(s.nodes[found->second].to-previous.nodes[previous.divider].to)<.00001f) {
            s.divider=found->second;s.dirtyDivider=true;s.idleAt=previous.idleAt;s.beginRatio=previous.beginRatio;animate=false;
        }
    }
    s.phase=animate && motionEnabled_ && model.motion && model.motionMs>0 ? PaneMotionPhase::Preparing:PaneMotionPhase::Idle;
    if(s.phase==PaneMotionPhase::Idle && !s.closing.empty())s.phase=PaneMotionPhase::FinalLayout;
    if(s.phase!=PaneMotionPhase::Idle)for(auto p:s.visibleOrder)s.panes[p].shown=true;
    s.terminals=previous.terminals;s.held=previous.held;
    s.pendingViewports.reserve(s.visibleOrder.size());
    s.ready=scanContent(s);
    return [this,next=std::move(next)]() mutable {
        state_=std::move(next);++generation_;
        for(auto& p:state_->panes)p.view->setVisible(p.shown);
        hold(state_->phase!=PaneMotionPhase::Idle || state_->dirtyDivider);
        publishContent(*state_);
        if(state_->phase==PaneMotionPhase::Preparing) arrange(0,false);
        else arrange(1,true);
        invalidate();
        if(!state_->ready || state_->phase!=PaneMotionPhase::Idle)requestAnimationFrame();
        if(auto found=state_->paneIds.find(state_->snapshot.activePane);found!=state_->paneIds.end())
            if(state_->panes[found->second].shown) {
                auto pane=state_->panes[found->second].view;auto header=pane->header.lock();
                // Header actions must not become the default destination for
                // directional focus. Preserve an existing content focus chain.
                for(auto& child:pane->children())if(child!=header&&child->isFocusable()) {
                    requestFocus(child.get(),false);break;
                }
            }
    };
}
} // namespace oneui
