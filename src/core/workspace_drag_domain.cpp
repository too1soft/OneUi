#include "oneui/layout/workspace_drag.h"
#include <algorithm>
#include <stdexcept>
namespace oneui {
void WorkspaceDragDomain::registerGrid(std::shared_ptr<PaneGrid> grid) {
    prepareGrid(std::move(grid))();
}
std::function<void()> WorkspaceDragDomain::prepareGrid(std::shared_ptr<PaneGrid> grid) {
    if(std::any_of(grids_.begin(),grids_.end(),[&](auto& v){return v.lock()==grid;}))return []{};
    // Reserve for every concurrently prepared registration. Copying the old
    // vector into each plan would lose earlier commits in the same flush.
    grids_.reserve(grids_.size()+registrations_+1);
    auto reservation=std::shared_ptr<void>(this,[this](void*){--registrations_;});
    ++registrations_;
    return [this,grid=std::move(grid),reservation=std::move(reservation)] {
        grids_.erase(std::remove_if(grids_.begin(),grids_.end(),[](auto& v){return v.expired();}),grids_.end());
        if(std::none_of(grids_.begin(),grids_.end(),[&](auto& v){return v.lock()==grid;}))grids_.push_back(grid);
    };
}
void WorkspaceDragDomain::registerTabs(std::string id,std::shared_ptr<Widget> widget) {
    strips_.erase(std::remove_if(strips_.begin(),strips_.end(),[](auto& s){return s.widget.expired();}),strips_.end());
    for(auto& s:strips_) {
        if(s.widget.lock()==widget){s.id=std::move(id);return;}
        if(s.id==id)throw std::invalid_argument("Duplicate tab strip identity in drag domain");
    }
    strips_.push_back({std::move(id),widget});
}
bool WorkspaceDragDomain::begin(std::string source,Point point,bool tabSource) {
    cancel();
    for(auto& weak:grids_)if(auto grid=weak.lock())if(grid->visible()&&grid->canDrag(source,tabSource)) {
        origin_=grid;previewOwner_=grid;source_=std::move(source);tabSource_=tabSource;revision_=grid->snapshot().revision;
        if(!tabSource)for(auto& pane:grid->visiblePanes())if(pane.key==source_)grid->showPreview(pane.bounds,false);
        move(point);return true;
    }
    return false;
}
bool WorkspaceDragDomain::current() {
    auto origin=origin_.lock();
    if(!origin || origin->snapshot().revision!=revision_ || !origin->canDrag(source_,tabSource_)){cancel();return false;}
    return true;
}
void WorkspaceDragDomain::move(Point point) {
    if(!active()||!current())return;
    auto origin=origin_.lock();
    if(!tabSource_ && origin->visiblePanes().size()>1)for(auto& strip:strips_)if(auto widget=strip.widget.lock())
        if((widget->visible()&&!widget->disabled())&&widget->frame().contains(point)) {
            if(auto old=previewOwner_.lock())if(old!=origin)old->clearPreview();
            previewOwner_=origin;origin->showPreview(widget->frame(),true);return;
        }
    for(auto& weak:grids_)if(auto grid=weak.lock())if((grid->visible()&&!grid->disabled())) {
        if(grid!=origin)continue; // A stable pane pool has one geometry owner.
        auto hit=grid->dropTarget(point,tabSource_);
        if(hit.first.empty()||hit.first==source_)continue;
        Rect bounds;
        for(auto& pane:grid->visiblePanes())if(pane.key==hit.first)bounds=pane.bounds;
        if(hit.second=="left"||hit.second=="right") {
            bounds.width*=.5f;if(hit.second=="right")bounds.x+=bounds.width;
        } else if(hit.second=="top"||hit.second=="bottom") {
            bounds.height*=.5f;if(hit.second=="bottom")bounds.y+=bounds.height;
        }
        if(auto old=previewOwner_.lock())if(old!=grid)old->clearPreview();
        previewOwner_=grid;grid->showPreview(bounds,true);return;
    }
    if(auto old=previewOwner_.lock())old->clearPreview();
    previewOwner_=origin;
    if(!tabSource_)for(auto& pane:origin->visiblePanes())if(pane.key==source_)origin->showPreview(pane.bounds,false);
}
void WorkspaceDragDomain::release(Point point) {
    if(!active()||!current())return;
    auto origin=origin_.lock();auto source=source_;const auto tabSource=tabSource_;
    // Final coordinates are authoritative, even when no move arrived there.
    move(point);
    std::string stripId;
    if(!tabSource)for(auto& strip:strips_)if(auto widget=strip.widget.lock())
        if((widget->visible()&&!widget->disabled())&&widget->frame().contains(point)){stripId=strip.id;break;}
    std::shared_ptr<PaneGrid> target;
    if(stripId.empty())for(auto& weak:grids_)if(auto grid=weak.lock())if((grid->visible()&&!grid->disabled())) {
        if(grid!=origin)continue;
        auto hit=grid->dropTarget(point,tabSource);
        if(!hit.first.empty()&&hit.first!=source){target=grid;break;}
    }
    // End capture before a callback can reenter. Keep the single preview until
    // the VM acknowledges the request or publishes another revision.
    if(stripId.empty()&&!target)cancel();
    source_.clear();origin_.reset();previewOwner_.reset();
    if(!stripId.empty())origin->requestTabDrop(std::move(source),std::move(stripId));
    else if(target)target->requestDrop(std::move(source),point,tabSource);
}
void WorkspaceDragDomain::cancel() {
    if(auto old=previewOwner_.lock())old->clearPreview();
    source_.clear();origin_.reset();previewOwner_.reset();
}
} // namespace oneui
