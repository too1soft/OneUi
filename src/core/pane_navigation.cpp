#include "pane_grid_state.h"
#include "internal/split_geometry.h"
#include <cmath>
#include <stdexcept>
namespace oneui {
void PaneGrid::navigate(const std::string& edge,bool swap) {
    if(!state_->snapshot.maximized.empty())return;
    auto panes=visiblePanes();auto active=std::find_if(panes.begin(),panes.end(),[&](auto& p){return p.key==state_->snapshot.activePane;});
    if(active==panes.end())return;
    const bool horizontal=edge=="left"||edge=="right";
    if(!horizontal && edge!="top" && edge!="bottom")return;
    const auto a=active->bounds;float best=std::numeric_limits<float>::max();std::string target;
    for(const auto& pane:panes) {
        if(pane.key==active->key)continue;auto b=pane.bounds;
        float gap=edge=="left"?a.x-b.x-b.width:edge=="right"?b.x-a.x-a.width:edge=="top"?a.y-b.y-b.height:b.y-a.y-a.height;
        if(gap< -1)continue;
        float overlap=horizontal?std::min(a.y+a.height,b.y+b.height)-std::max(a.y,b.y):std::min(a.x+a.width,b.x+b.width)-std::max(a.x,b.x);
        float centers=horizontal?std::abs(a.y+a.height*.5f-b.y-b.height*.5f):std::abs(a.x+a.width*.5f-b.x-b.width*.5f);
        float score=(overlap>0?0.f:10000.f)+std::max(0.f,gap)+centers*.25f;
        if(score<best){best=score;target=pane.key;}
    }
    if(!target.empty())emit({swap?WorkspaceIntentKind::Swap:WorkspaceIntentKind::Focus,active->key,target});
}
void PaneGrid::queueNavigation(int64_t serial,std::string edge,bool swap) {
    if(serial<=state_->navigationSerial)return;
    if(edge!="left"&&edge!="right"&&edge!="top"&&edge!="bottom")throw std::invalid_argument("Invalid navigation edge");
    state_->navigationSerial=serial;
    if(state_->navigation.size()==8){++state_->navigationOverflow;return;}
    state_->navigation.push_back({std::move(edge),swap});requestAnimationFrame();
}
uint64_t PaneGrid::navigationDropped() const{return state_->navigationOverflow;}
std::pair<std::string,std::string> PaneGrid::dropTarget(Point point,bool tabSource) const {
    if(!state_->snapshot.maximized.empty())return {};
    static const char* edges[]={"left","right","top","bottom"};
    for(const auto& pane:visiblePanes()) {
        auto b=pane.bounds;
        if(!b.contains(point)||b.width<=0||b.height<=0)continue;
        auto p=state_->paneIds.at(pane.key);if(state_->panes[p].closing)continue;
        float x=(point.x-b.x)/b.width,y=(point.y-b.y)/b.height;
        const bool center=point.x>b.x+b.width*.28f&&point.x<b.x+b.width*.72f&&
                          point.y>b.y+b.height*.28f&&point.y<b.y+b.height*.72f;
        auto edge=!tabSource&&center?"center":edges[detail::nearestNormalizedEdge(x,y)];
        return {pane.key,edge};
    }
    return {};
}
void PaneGrid::requestDrop(std::string source,Point point,bool tabSource) {
    auto origin=state_->paneIds.find(source);
    if(origin==state_->paneIds.end()||state_->panes[origin->second].closing)return;
    auto target=dropTarget(point,tabSource);
    if(target.first.empty()||target.first==source)return;
    if(tabSource && std::find(state_->visibleOrder.begin(),state_->visibleOrder.end(),origin->second)!=state_->visibleOrder.end())return;
    emit({WorkspaceIntentKind::Drop,std::move(source),std::move(target.first),std::move(target.second)});
}
bool PaneGrid::canDrag(const std::string& source,bool tabSource) const {
    if(!state_->snapshot.maximized.empty())return false;
    auto pane=state_->paneIds.find(source);
    if(pane==state_->paneIds.end() || state_->panes[pane->second].closing)return false;
    if(!tabSource)return state_->panes[pane->second].shown;
    if(state_->panes[pane->second].shown)return false;
    for(auto& group:state_->snapshot.groups)if(!group.root.empty()) {
        auto node=state_->nodeIds.at(group.root);
        if(state_->nodes[node].pane==pane->second)return true;
    }
    return false;
}
void PaneGrid::requestTabDrop(std::string source,std::string target) {
    if(canDrag(source) && state_->visibleOrder.size()>1)
        emit({WorkspaceIntentKind::Drop,std::move(source),std::move(target),"tab-strip"});
}
} // namespace oneui
