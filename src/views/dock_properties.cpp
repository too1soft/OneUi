#include "workspace_cache.h"
#include <cmath>
namespace oneui::views {
bool Runtime::dockScalar(Instance& instance,const std::string& name,const Value& v) {
    auto& e=instance.element;
    if(e.component=="DockItem") {
        auto p=e.as<DockItemView>();
        if(name=="pane-key") {
            check(!v.text.empty()&&(p->key.empty()||p->key==v.text),ONEUI_VIEW_TYPE,"Dock item identity cannot change");
            if(staging)rollbacks.push_back([p,old=p->key]{p->key=old;});p->key=v.text;
        } else if(name=="hideable") {
            if(staging)rollbacks.push_back([p,old=p->hideable]{p->hideable=old;});p->hideable=bool(v.integer);
        } else if(name=="dock-min-width"||name=="dock-min-height"||name=="float-min-width"||name=="float-min-height") {
            check(std::isfinite(v.number)&&v.number>=0,ONEUI_VIEW_TYPE,"Invalid dock minimum");
            if(staging)rollbacks.push_back([p,a=p->dockMinimum,b=p->floatMinimum]{p->dockMinimum=a;p->floatMinimum=b;});
            auto& size=name.substr(0,4)=="dock"?p->dockMinimum:p->floatMinimum;
            (name.find("width")!=std::string::npos?size.width:size.height)=float(v.number);
        } else return false;
        if(staging)rollbacks.push_back([&instance]{for(auto& b:instance.bindings)b.previous.reset();});
        return true;
    }
    if(e.component!="DockWorkspace")return false;
    auto old=instance.workspaceCache;
    auto next=old?std::make_shared<Instance::WorkspaceCache>(*old):std::make_shared<Instance::WorkspaceCache>();
    auto& s=next->dock;
    if(name=="root")s.root=v.text;
    else if(name=="maximized")s.maximized=v.text;
    else if(name=="compact-active")s.compactActive=v.text;
    else if(name=="revision")s.revision=v.integer;
    else if(name=="ack-request")s.ackRequest=v.integer;
    else if(name=="ack-accepted")s.ackAccepted=bool(v.integer);
    else if(name=="gap")s.gap=float(v.number);
    else if(name=="divider-hit")s.dividerHit=float(v.number);
    else return false;
    if(staging)rollbacks.push_back([&instance,old]{instance.workspaceCache=old;for(auto& b:instance.bindings)b.previous.reset();});
    instance.workspaceCache=std::move(next);return true;
}
bool Runtime::dockList(Instance& instance,const std::string& name,ListState& list) {
    if(instance.element.component!="DockWorkspace"||(name!="nodes"&&name!="placements"))return false;
    auto old=instance.workspaceCache;
    auto next=old?std::make_shared<Instance::WorkspaceCache>(*old):std::make_shared<Instance::WorkspaceCache>();
    auto field=[&](const std::shared_ptr<Scope>& scope,const char* name) {
        return read(scope,image.schema.model(scope->table.model_id).properties.at(name).id);
    };
    if(name=="nodes") {
        next->dock.nodes.clear();next->dock.nodes.reserve(list.items.size());
        for(size_t i=0;i<list.items.size();++i) {
            auto scope=list.items[i];auto kind=field(scope,"kind").text,axis=field(scope,"axis").text;
            WorkspaceNode n;n.id=list.keys[i];n.pane=field(scope,"pane").text;n.first=field(scope,"first").text;n.second=field(scope,"second").text;
            n.ratio=float(field(scope,"ratio").number);
            check(kind=="leaf" ? !n.pane.empty()&&n.first.empty()&&n.second.empty()&&axis.empty()&&n.ratio==.5f :
                  kind=="split"&&n.pane.empty()&&(axis=="horizontal"||axis=="vertical"),ONEUI_VIEW_TYPE,"Invalid dock node fields");
            n.axis=axis=="vertical"?DockAxis::Vertical:DockAxis::Horizontal;next->dock.nodes.push_back(std::move(n));
        }
    } else {
        next->dock.placements.clear();next->dock.placements.reserve(list.items.size());
        for(size_t i=0;i<list.items.size();++i) {
            auto scope=list.items[i];DockPlacement p;p.id=list.keys[i];p.mode=field(scope,"mode").text;
            p.bounds={float(field(scope,"x").number),float(field(scope,"y").number),float(field(scope,"width").number),float(field(scope,"height").number)};
            p.pinned=bool(field(scope,"pinned").integer);p.order=field(scope,"order").integer;
            next->dock.placements.push_back(std::move(p));
        }
    }
    if(staging)rollbacks.push_back([&instance,old]{instance.workspaceCache=old;});
    instance.workspaceCache=std::move(next);return true;
}
void Runtime::prepareDock(const std::shared_ptr<Instance>& instance,const std::vector<std::shared_ptr<Widget>>& children) {
    std::vector<std::shared_ptr<DockItemView>> items;
    for(auto& child:children){auto item=std::dynamic_pointer_cast<DockItemView>(child);check(bool(item),ONEUI_VIEW_TYPE,"DockWorkspace requires DockItem");items.push_back(item);}
    if(!instance->workspaceCache)instance->workspaceCache=std::make_shared<Instance::WorkspaceCache>();
    auto publish=instance->element.as<DockWorkspaceView>()->prepare(instance->workspaceCache->dock,std::move(items));
    if(staging&&instance->published)commits.push_back(std::move(publish));else publish();
}
} // namespace oneui::views
