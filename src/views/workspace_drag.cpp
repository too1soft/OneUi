#include "workspace_cache.h"
#include "oneui/layout/workspace_drag.h"
#include <cmath>
namespace oneui::views {
struct Runtime::WorkspaceDomains {std::map<std::string,std::shared_ptr<WorkspaceDragDomain>> values;};
namespace {
std::shared_ptr<WorkspaceDragDomain> domain(Runtime& runtime,const std::string& name) {
    if(name.empty())return {};
    if(!runtime.workspaceDomains)runtime.workspaceDomains=std::make_shared<Runtime::WorkspaceDomains>();
    auto& value=runtime.workspaceDomains->values[name];if(!value)value=std::make_shared<WorkspaceDragDomain>();return value;
}
Value string(std::string value){Value v;v.tag=4;v.text=std::move(value);return v;}
Value number(double value){Value v;v.tag=3;v.number=value;return v;}
Value integer(int64_t value){Value v;v.tag=2;v.integer=value;return v;}
}
bool Runtime::dragScalar(Instance& instance,const std::string& name,const Value& value) {
    if(instance.element.component=="DragHandle") {
        auto w=instance.element.as<DragHandle>();
        if(name=="source")w->source=value.text;
        else if(name=="domain")w->domain=value.text;
        else if(name=="kind") {
            check(value.text=="pane"||value.text=="dock",ONEUI_VIEW_TYPE,"Invalid drag kind");w->kind=value.text;
        } else if(name=="threshold") {
            check(std::isfinite(value.number)&&value.number>=0,ONEUI_VIEW_TYPE,"Invalid drag threshold");w->threshold=float(value.number);
        } else return false;
        return true;
    }
    if(instance.element.component=="DropSurface") {
        auto w=instance.element.as<DropSurface>();
        if(name=="domain")w->domain=value.text;
        else if(name=="role") {check(value.text=="tab-strip",ONEUI_VIEW_TYPE,"Invalid drop role");w->role=value.text;}
        else return false;
        return true;
    }
    if(instance.element.component!="Tabs")return false;
    if(name!="drag-domain"&&name!="drag-kind"&&name!="reorderable")return false;
    auto& cache=instance.workspaceCache;if(!cache)cache=std::make_shared<Instance::WorkspaceCache>();
    if(name=="drag-domain")cache->domain=value.text;
    if(name=="drag-kind") {
        check(value.text=="single-pane-tab",ONEUI_VIEW_TYPE,"Invalid tab drag kind");cache->dragKind=value.text;
    }
    if(name=="reorderable")instance.element.as<Tabs>()->setReorderEnabled(bool(value.integer));
    return true;
}
bool Runtime::dragList(Instance& instance,const std::string& name,ListState& state) {
    if(instance.element.component!="Tabs" || name!="drag-sources")return false;
    auto& cache=instance.workspaceCache;if(!cache)cache=std::make_shared<Instance::WorkspaceCache>();
    std::map<std::string,Instance::WorkspaceCache::TabSource> values;
    for(size_t i=0;i<state.items.size();++i) {
        auto& item=state.items[i];auto& model=image.schema.model(item->table.model_id);
        values.emplace(state.keys[i],Instance::WorkspaceCache::TabSource{read(item,model.properties.at("pane").id).text,bool(read(item,model.properties.at("enabled").id).integer)});
    }
    cache->tabSources=std::move(values);return true;
}
std::function<void()> Runtime::prepareDragGrid(const std::shared_ptr<Instance>& instance) {
    auto cache=instance->workspaceCache;
    if(cache)if(auto d=domain(*this,cache->domain)) {
        auto commit=d->prepareGrid(instance->element.as<PaneGrid>());
        return [d,commit=std::move(commit)]{commit();};
    }
    return []{};
}
void Runtime::wireDrag(const std::shared_ptr<Instance>& instance) {
    std::weak_ptr<Instance> weak=instance;
    if(instance->element.component=="DropSurface") {
        auto surface=instance->element.as<DropSurface>();
        if(auto d=domain(*this,surface->domain))d->registerTabs(image.strings[image.records[oneview::NODE][instance->node][11]],surface);
    } else if(instance->element.component=="DragHandle") {
        instance->element.as<DragHandle>()->setOnDrag([weak](int phase,Point point) {
            auto item=weak.lock();auto owner=item?item->owner.lock():nullptr;if(!owner||owner->closing)return;
            auto w=item->element.as<DragHandle>();
            if(w->kind=="pane")if(auto d=domain(*owner,w->domain)) {
                if(phase==0)d->begin(w->source,point);
                else if(phase==1)d->move(point);else if(phase==2)d->release(point);else d->cancel();
            }
            if(w->kind=="dock") {
                auto parent=item->parent.lock();
                while(parent&&parent->element.component!="DockWorkspace")parent=parent->parent.lock();
                if(parent) {
                    auto id=owner->image.strings[owner->image.records[oneview::NODE][parent->node][11]];
                    if(w->domain.empty()||w->domain==id)parent->element.as<DockWorkspaceView>()->drag(w->source,phase,point);
                }
            }
            emitComponentEvent(weak,"dragStateChanged",{string(w->source),integer(phase)});
        });
    } else if(instance->element.component=="PaneHeader") {
        instance->element.as<PaneHeader>()->setOnDrag([weak](int phase,Point point) {
            auto item=weak.lock();auto owner=item?item->owner.lock():nullptr;if(!owner||owner->closing)return;
            auto parent=item->parent.lock();while(parent && parent->element.component!="PaneGrid")parent=parent->parent.lock();
            if(!parent || !parent->workspaceCache)return;
            auto d=domain(*owner,parent->workspaceCache->domain);if(!d)return;
            if(phase==0)d->begin(item->element.as<PaneHeader>()->config().pane,point);
            else if(phase==1)d->move(point);else if(phase==2)d->release(point);else d->cancel();
        });
    } else if(instance->element.component=="Tabs") {
        auto tabs=instance->element.as<Tabs>();
        if(instance->workspaceCache)if(auto d=domain(*this,instance->workspaceCache->domain))
            d->registerTabs(image.strings[image.records[oneview::NODE][instance->node][11]],tabs);
        tabs->setOnReorderRequested([weak](int from,int to) {
            auto item=weak.lock();if(!item)return;
            auto source=narrow(item->element.tabs->key(from));
            auto before=narrow(item->element.tabs->key(to>from?to+1:to));
            emitComponentEvent(weak,"reorderRequested",{string(source),string(before)});
        });
        auto draggedKey=std::make_shared<std::string>();
        tabs->setOnTabDragOut([weak,draggedKey](int index,int phase,Point point) {
            auto item=weak.lock();auto owner=item?item->owner.lock():nullptr;if(!owner||owner->closing)return;
            if(phase==0)*draggedKey=narrow(item->element.tabs->key(index));
            auto key=*draggedKey;auto cache=item->workspaceCache;
            if(cache)if(auto d=domain(*owner,cache->domain)) {
                auto source=cache->tabSources.find(key);
                if(phase==0) {if(source!=cache->tabSources.end() && source->second.enabled)d->begin(source->second.pane,point,true);}
                else if(phase==1)d->move(point);else if(phase==2)d->release(point);else d->cancel();
            }
            emitComponentEvent(weak,"dragOut",{string(key),integer(phase),number(point.x),number(point.y)});
            if(phase==2||phase==3)draggedKey->clear();
        });
    }
}
} // namespace oneui::views
