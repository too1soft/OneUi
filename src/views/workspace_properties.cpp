#include "workspace_cache.h"
#include <cmath>

namespace oneui::views {
bool Runtime::workspaceScalar(Instance& instance,const std::string& name,const Value& v) {
    auto& e=instance.element;
    if(shortcutScalar(instance,name,v))return true;
    if(pickerScalar(instance,name,v))return true;
    if(dockScalar(instance,name,v))return true;
    if(e.component=="ContainerPage"&&name=="selected"){e.as<ContainerPage>()->setSelected(bool(v.integer));return true;}
    if(dragScalar(instance,name,v))return true;
    if(paneHeaderScalar(instance,name,v))return true;
    if(e.component=="NativeHost" && name=="resize-policy") {
        check(v.text=="immediate"||v.text=="settled",ONEUI_VIEW_TYPE,"Invalid resize policy");
        e.as<ui::NativeHostView>()->setSettledViewport(v.text=="settled");return true;
    }
    if(e.component=="Pane") {
        auto p=e.as<PaneView>();
        if(staging)rollbacks.push_back([&instance]{for(auto& b:instance.bindings)b.previous.reset();});
        if(name=="pane-key") {
            check(!v.text.empty(),ONEUI_VIEW_TYPE,"Empty pane key");
            check(p->key.empty()||p->key==v.text,ONEUI_VIEW_TYPE,"Pane identity cannot change in place");
            if(staging)rollbacks.push_back([p,old=p->key]{p->key=old;});p->key=v.text;
        } else if(name=="closing") {
            if(staging)rollbacks.push_back([p,old=p->closing]{p->closing=old;});p->closing=bool(v.integer);
        } else if(name=="close-request") {
            if(staging)rollbacks.push_back([p,old=p->closeRequest]{p->closeRequest=old;});p->closeRequest=v.integer;
        } else if(name=="min-width" || name=="min-height") {
            check(std::isfinite(v.number)&&v.number>=0,ONEUI_VIEW_TYPE,"Invalid pane minimum");
            if(staging)rollbacks.push_back([p,old=p->minimum]{p->minimum=old;});
            (name=="min-width"?p->minimum.width:p->minimum.height)=float(v.number);
        } else return false;
        return true;
    }
    if(e.component!="PaneGrid")return false;
    using namespace ui::schema;
    if(!contains(component(Kind::PaneGrid).properties,name))return false;
    auto old=instance.workspaceCache;
    auto next=old?std::make_shared<Instance::WorkspaceCache>(*old):std::make_shared<Instance::WorkspaceCache>();
    auto& s=next->grid;
    if(name=="active-group")s.activeGroup=v.text;
    else if(name=="active-pane")s.activePane=v.text;
    else if(name=="maximized")s.maximized=v.text;
    else if(name=="revision")s.revision=v.integer;
    else if(name=="ack-request")s.ackRequest=v.integer;
    else if(name=="ack-accepted")s.ackAccepted=bool(v.integer);
    else if(name=="motion")s.motion=bool(v.integer);
    else if(name=="gap")s.gap=float(v.number);
    else if(name=="divider-hit")s.dividerHit=float(v.number);
    else if(name=="motion-ms")s.motionMs=v.number;
    else if(name=="resize-idle-ms")s.resizeIdleMs=v.number;
    else if(name=="navigation-serial")next->navigation=v.integer;
    else if(name=="navigation-edge")next->edge=v.text;
    else if(name=="navigation-mode")next->mode=v.text;
    else if(name=="drag-domain")next->domain=v.text;
    else return false;
    if(staging)rollbacks.push_back([&instance,old]{instance.workspaceCache=old;for(auto& b:instance.bindings)b.previous.reset();});
    instance.workspaceCache=std::move(next);return true;
}
bool Runtime::workspaceList(Instance& instance,const std::string& name,ListState& list) {
    if(pickerList(instance,name,list))return true;
    if(dockList(instance,name,list))return true;
    if(dragList(instance,name,list))return true;
    if(instance.element.component!="PaneGrid" || (name!="nodes" && name!="groups"))return false;
    auto old=instance.workspaceCache;
    auto next=old?std::make_shared<Instance::WorkspaceCache>(*old):std::make_shared<Instance::WorkspaceCache>();
    auto field=[&](const std::shared_ptr<Scope>& scope,const char* name) {
        return read(scope,image.schema.model(scope->table.model_id).properties.at(name).id);
    };
    if(name=="nodes") {
        next->grid.nodes.clear();next->grid.nodes.reserve(list.items.size());
        for(size_t i=0;i<list.items.size();++i) {
            auto scope=list.items[i];auto kind=field(scope,"kind").text,axis=field(scope,"axis").text;
            check(kind=="leaf"||kind=="split",ONEUI_VIEW_TYPE,"Invalid workspace node kind");
            WorkspaceNode node;node.id=list.keys[i];node.pane=field(scope,"pane").text;
            node.first=field(scope,"first").text;node.second=field(scope,"second").text;
            node.ratio=float(field(scope,"ratio").number);
            check(kind=="leaf" ? !node.pane.empty()&&node.first.empty()&&node.second.empty()&&axis.empty()&&node.ratio==.5f :
                  node.pane.empty()&&(axis=="horizontal"||axis=="vertical"),ONEUI_VIEW_TYPE,"Node kind/fields mismatch");
            node.axis=axis=="vertical"?DockAxis::Vertical:DockAxis::Horizontal;
            next->grid.nodes.push_back(std::move(node));
        }
    } else {
        next->grid.groups.clear();
        for(size_t i=0;i<list.items.size();++i)next->grid.groups.push_back({list.keys[i],field(list.items[i],"root").text});
    }
    if(staging)rollbacks.push_back([&instance,old]{instance.workspaceCache=old;});
    instance.workspaceCache=std::move(next);return true;
}
void Runtime::prepareWorkspace(const std::shared_ptr<Instance>& instance,const std::vector<std::shared_ptr<Widget>>& children) {
    if(instance->element.component!="PaneGrid")return;
    if(!instance->workspaceCache)instance->workspaceCache=std::make_shared<Instance::WorkspaceCache>();
    std::vector<std::shared_ptr<PaneView>> panes;
    for(auto& child:children) {
        auto pane=std::dynamic_pointer_cast<PaneView>(child);
        check(bool(pane),ONEUI_VIEW_TYPE,"PaneGrid requires Pane children");panes.push_back(std::move(pane));
    }
    auto grid=instance->element.as<PaneGrid>();
    auto cache=instance->workspaceCache;
    check(cache->mode=="focus"||cache->mode=="swap",ONEUI_VIEW_TYPE,"Invalid navigation mode");
    check(cache->navigation<=0 || cache->edge=="left" || cache->edge=="right" || cache->edge=="top" || cache->edge=="bottom",ONEUI_VIEW_TYPE,"Invalid navigation edge");
    auto publish=grid->prepare(cache->grid,std::move(panes));
    auto registration=prepareDragGrid(instance);
    auto commit=[grid,cache,edge=cache->edge,publish=std::move(publish),registration=std::move(registration)]()mutable {
        publish();
        grid->queueNavigation(cache->navigation,std::move(edge),cache->mode=="swap");
        registration();
    };
    if(staging&&instance->published)commits.push_back(std::move(commit));else commit();
}
} // namespace oneui::views
