#include "dock_workspace_state.h"
#include "internal/split_geometry.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <set>
namespace oneui {
namespace {
bool finite(Rect r) {return std::isfinite(r.x)&&std::isfinite(r.y)&&std::isfinite(r.width)&&std::isfinite(r.height);}
bool valid(Size s) {return std::isfinite(s.width)&&std::isfinite(s.height)&&s.width>=0&&s.height>=0;}
}
std::function<void()> DockWorkspaceView::prepare(DockWorkspaceSnapshot model,std::vector<std::shared_ptr<DockItemView>> items) {
    return prepare(std::move(model),std::move(items),state_->presentation);
}
std::function<void()> DockWorkspaceView::prepare(DockWorkspaceSnapshot model,std::vector<std::shared_ptr<DockItemView>> items,DockWorkspacePresentation presentation) {
    auto next=std::make_shared<State>();auto& s=*next;
    s.presentation=std::move(presentation);
    s.snapshot=std::move(model);const auto& m=s.snapshot;
    s.identities=state_->identities;s.nextIdentity=state_->nextIdentity;
    // Owner style survives snapshot replacement; only layout state is rebuilt.
    s.sheet=state_->sheet;s.node=state_->node;
    s.floatSurface=state_->floatSurface;s.floatSurfaceSheet=state_->floatSurfaceSheet;
    if(!std::isfinite(m.dividerHit)||m.dividerHit<0)throw std::invalid_argument("Invalid dock divider hit");
    std::map<std::string,DockPlacement> placements;
    for(auto& p:m.placements) {
        if(p.id.empty()||!finite(p.bounds)||!placements.emplace(p.id,p).second||
           (p.mode!="docked"&&p.mode!="floating"&&p.mode!="hidden")||
           (p.mode=="floating"&&(p.bounds.width<=0||p.bounds.height<=0)))
            throw std::invalid_argument("Invalid dock placement");
    }
    std::vector<WorkspacePane> minimums;
    for(auto& item:items) {
        if(!item||item->key.empty()||!valid(item->dockMinimum)||!valid(item->floatMinimum)||
           !placements.count(item->key)||!s.itemIds.emplace(item->key,s.items.size()).second)
            throw std::invalid_argument("Invalid dock item");
        if(!s.identities.count(item->key))s.identities.emplace(item->key,s.nextIdentity++);
        const auto& p=placements.at(item->key);
        if(!item->hideable&&p.mode=="hidden")throw std::invalid_argument("Non-hideable dock item is hidden");
        s.items.push_back({item,p,s.identities.at(item->key),false});
        minimums.push_back({item->key,item->dockMinimum});
    }
    if(placements.size()!=items.size())throw std::invalid_argument("Dock placement has no item");
    if(s.index.reset(m.nodes,{{"root",m.root}},minimums,m.gap)!=DockError::None)
        throw std::invalid_argument("Invalid dock tree");
    s.nodes.resize(m.nodes.size());s.work.resize(m.nodes.size()+1);s.targets.reserve(items.size());
    for(size_t i=0;i<m.nodes.size();++i)s.nodeIds.emplace(m.nodes[i].id,i);
    if(!m.root.empty())s.root=s.nodeIds.at(m.root);
    std::set<std::string> leaves;
    for(size_t i=0;i<m.nodes.size();++i) {
        const auto& node=m.nodes[i];auto& n=s.nodes[i];n.ratio=node.ratio;
        if(!node.pane.empty()) {
            n.item=s.itemIds.at(node.pane);n.minimum=s.items[n.item].view->dockMinimum;leaves.insert(node.pane);
        } else {n.first=s.nodeIds.at(node.first);n.second=s.nodeIds.at(node.second);}
    }
    for(auto& item:s.items)if(leaves.count(item.view->key)!=(item.placement.mode=="docked"))
        throw std::invalid_argument("Dock placement/tree membership mismatch: "+item.view->key+" mode="+item.placement.mode+" leaf="+std::to_string(leaves.count(item.view->key)));
    if(!m.compactActive.empty()&&!leaves.count(m.compactActive))throw std::invalid_argument("Invalid compact dock item");
    if(!m.maximized.empty()&&(!s.itemIds.count(m.maximized)||placements.at(m.maximized).mode=="hidden"))
        throw std::invalid_argument("Invalid maximized dock item");
    std::vector<std::pair<size_t,bool>> work;
    if(s.root!=State::none)work.push_back({s.root,false});
    while(!work.empty()) {
        auto [i,exit]=work.back();work.pop_back();auto& n=s.nodes[i];
        if(n.item!=State::none)continue;
        if(!exit){work.push_back({i,true});work.push_back({n.second,false});work.push_back({n.first,false});continue;}
        n.minimum=detail::splitMinimum(s.nodes[n.first].minimum,s.nodes[n.second].minimum,m.gap,m.nodes[i].axis==DockAxis::Horizontal);
    }
    std::vector<size_t> order(s.items.size());for(size_t i=0;i<order.size();++i)order[i]=i;
    std::sort(order.begin(),order.end(),[&](size_t a,size_t b){
        const auto& x=s.items[a].placement;const auto& y=s.items[b].placement;
        return std::make_tuple(x.mode=="floating",x.pinned,x.order,x.id)<std::make_tuple(y.mode=="floating",y.pinned,y.order,y.id);
    });
    std::vector<std::shared_ptr<Widget>> children;children.reserve(order.size());
    for(auto i:order)children.push_back(s.items[i].view);
    auto plan=prepareChildren(std::move(children));
    // A same-revision property notification cannot cancel an active gesture.
    // New authoritative revisions, and acknowledgements, always retire preview.
    const auto& old=*state_;
    if(m.revision==old.snapshot.revision&&m.ackRequest==old.snapshot.ackRequest) {
        if(old.source!=State::none&&s.itemIds.count(old.items[old.source].view->key)) {
            s.source=s.itemIds.at(old.items[old.source].view->key);s.gesture=old.gesture;
            s.origin=old.origin;s.start=old.start;s.bounds=old.bounds;s.resizeEdge=old.resizeEdge;
        }
        s.previewActive=old.previewActive;s.preview=old.preview;s.previewTarget=old.previewTarget;
        s.previewTime=old.previewTime;s.pendingRequest=old.pendingRequest;s.hit=old.hit;
        if(old.divider!=State::none&&s.nodeIds.count(old.snapshot.nodes[old.divider].id)) {
            s.divider=s.nodeIds.at(old.snapshot.nodes[old.divider].id);s.gesture=old.gesture;
            s.beginRatio=old.beginRatio;s.grab=old.grab;s.keyboardDirty=old.keyboardDirty;s.idleAt=old.idleAt;
            s.nodes[s.divider].ratio=old.nodes[old.divider].ratio;
        }
    }
    if(old.focusedDivider!=State::none&&s.nodeIds.count(old.snapshot.nodes[old.focusedDivider].id))
        s.focusedDivider=s.nodeIds.at(old.snapshot.nodes[old.focusedDivider].id);
    return [this,next=std::move(next),plan=std::move(plan)]() mutable {
        plan->commit();state_=std::move(next);layoutChildren();invalidate();
    };
}
} // namespace oneui
