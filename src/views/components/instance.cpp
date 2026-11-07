#include "../runtime.h"
#include "instance.h"
#include "internal/widget_handle.h"

namespace oneui::views::components {
void initializeHost(InstanceState &);
Factory::~Factory() { if(retained && api.release_data) try { api.release_data(api.data); } catch(...) {} }
InstanceState::~InstanceState() {
    disconnect();
    if(attempted && factory->api.release_instance) try { factory->api.release_instance(instance); } catch(...) {}
}
void InstanceState::disconnect() noexcept {
    if(disconnected) return;
    disconnected=true;
    if(attempted && factory->api.disconnect) try {
        auto item=node.lock();auto runtime=item?item->owner.lock():nullptr;
        if(runtime) { Callback guard(*runtime,Phase::Cleanup);factory->api.disconnect(instance); }
        else factory->api.disconnect(instance);
    } catch(...) {}
}
void InstanceState::applyStyle(const StyleSheet &value,const StyleNode &node) {
    sheet=std::make_shared<StyleSheet>(value); styleNode=node;
    if(!disconnected && factory->api.style_changed) {
        auto item=this->node.lock();auto runtime=item?item->owner.lock():nullptr;
        if(runtime) { Callback guard(*runtime,Phase::Read);factory->api.style_changed(instance);runtime->alive(); }
    }
}
bool create(Runtime &runtime,const std::shared_ptr<Instance> &node,const std::string &name,
            const std::vector<ui::Element> &children,const std::string &classes) {
    if(!runtime.componentRegistry) return false;
    auto found=runtime.componentRegistry->factories.find(name);
    if(found==runtime.componentRegistry->factories.end()) return false;
    auto state=std::make_shared<InstanceState>();state->factory=found->second;state->node=node;
    node->owner=runtime.weak_from_this();initializeHost(*state);
    std::vector<OneUiWidget> borrowed(children.size());std::vector<OneUiWidget*> handles;
    for(size_t i=0;i<children.size();++i) { borrowed[i].widget=children[i].widget;handles.push_back(&borrowed[i]); }
    OneUiWidget *output=nullptr;
    std::unique_ptr<OneUiWidget> owned;
    state->attempted=true;
    int result=ONEUI_VIEW_FOREIGN_FAILURE;
    try {
        Callback callback(runtime,Phase::Read);
        result=state->factory->api.create(state->factory->api.data,&state->host,handles.data(),handles.size(),&state->instance,&output);
        owned.reset(output);
    } catch(...) { owned.reset(output);throw; }
    check(result==0,result<0?result:ONEUI_VIEW_FOREIGN_FAILURE,"Component factory failed");
    check(owned && owned->widget && !owned->widget->hasCompositionOwner() && !owned->widget->hasExtensionOwner(),
          ONEUI_VIEW_INVALID_ARGUMENT,"Component factory must return a fresh unattached widget");
    runtime.alive();
    auto widget=owned->widget;
    auto weak=std::weak_ptr<InstanceState>(state);
    widget->attachExtensionOwner(state,[weak](const auto &sheet,const auto &node) { if(auto state=weak.lock())state->applyStyle(sheet,node); });
    node->extension=state;
    node->element=ui::Element(ui::Node(widget),name);node->element.classes=classes;
    runtime.styles->addFresh(node->element);
    auto &states=*runtime.componentInstances;
    if(states.size()%256==0) states.erase(std::remove_if(states.begin(),states.end(),[](const auto &p){return p.expired();}),states.end());
    states.push_back(state);
    return true;
}
bool set(const std::shared_ptr<Instance> &node,uint32_t id,const OneUiValue &value) {
    if(!node->extension || id<1000) return false;
    auto &state=*node->extension;
    auto runtime=node->owner.lock();check(bool(runtime),ONEUI_VIEW_CLOSED,"Component view closed");
    Callback callback(*runtime,Phase::Read);
    const auto result=state.factory->api.set(state.instance,id,&value);
    check(result==0,result<0?result:ONEUI_VIEW_FOREIGN_FAILURE,"Component property rejected");
    runtime->alive();
    return true;
}
void closeScope(Runtime &runtime,const std::shared_ptr<Scope> &scope) {
    const auto instances=*runtime.componentInstances;
    for(const auto &weak:instances) if(auto state=weak.lock()) {
        auto node=state->node.lock();
        if(node && node->owner.lock().get()==&runtime && std::find(node->scopes.begin(),node->scopes.end(),scope)!=node->scopes.end()) state->disconnect();
    }
}
}
namespace oneui::views { Instance::~Instance() { if(extension) extension->disconnect(); } }
