#include "oneui_c_api_view_internal.h"
#include "../views/components/registry.h"

using namespace oneui::capi::views;
namespace {
void registryThread(OneUiComponentRegistry *registry) {
    check(registry,ONEUI_VIEW_INVALID_ARGUMENT,"Component registry required");
    check(registry->thread==std::this_thread::get_id(),ONEUI_VIEW_WRONG_THREAD,"Component registry thread");
    check(!registry->busy,ONEUI_VIEW_REENTRANT,"Component registry callback");
}
struct Operation {
    OneUiComponentRegistry &registry;
    explicit Operation(OneUiComponentRegistry &r):registry(r) { r.busy=true; }
    ~Operation() { registry.busy=false; }
};
}
uint32_t oneui_component_abi_version(void) { return ONEUI_COMPONENT_ABI_V1; }
OneUiViewResult oneui_component_registry_create_v1(OneUiComponentRegistry **out) {
    if(out) *out=nullptr;
    return call(__func__,[&] { check(out,ONEUI_VIEW_INVALID_ARGUMENT,"Registry output required"); *out=new OneUiComponentRegistry; });
}
OneUiViewResult oneui_component_registry_register_v1(OneUiComponentRegistry *registry,const OneUiComponentFactoryV1 *api) {
    return call(__func__,[&] {
        registryThread(registry);
        check(api && api->struct_size==sizeof(*api) && api->abi_version==ONEUI_COMPONENT_ABI_V1,
              ONEUI_VIEW_VERSION,"Component factory ABI");
        check(api->create && api->set && api->release_instance && bool(api->retain_data)==bool(api->release_data),
              ONEUI_VIEW_INVALID_ARGUMENT,"Component callbacks required");
        check(api->contract.data && api->contract.length && api->contract.length<=1024*1024,
              ONEUI_VIEW_INVALID_ARGUMENT,"Component contract required");
        auto factory=std::make_shared<oneui::views::components::Factory>();
        factory->document.assign(api->contract.data,api->contract.length);
        auto contracts=oneview::parseComponentExtensions(factory->document);
        check(contracts.size()==1,ONEUI_VIEW_INVALID_ARGUMENT,"Factory requires exactly one component contract");
        factory->contract=contracts.begin()->second;
        check(registry->value->factories.size()<256 && !registry->value->factories.count(factory->contract.name),
              ONEUI_VIEW_INVALID_ARGUMENT,"Duplicate/excessive component factory");
        Operation operation(*registry);
        factory->api=*api;factory->api.contract={factory->document.data(),factory->document.size()};
        if(api->retain_data) { factory->retained=true;api->retain_data(api->data); }
        auto next=std::make_shared<oneui::views::components::Registry>(*registry->value);
        next->contracts.emplace(factory->contract.name,factory->contract);
        next->factories.emplace(factory->contract.name,factory);
        registry->value=std::move(next);
    });
}
OneUiViewResult oneui_component_registry_destroy(OneUiComponentRegistry *registry) {
    return call(__func__,[&] { registryThread(registry);registry->busy=true;delete registry; });
}
OneUiViewResult oneui_view_mount_set_components_v1(OneUiViewMount *handle,OneUiComponentRegistry *registry) {
    return call(__func__,[&] {
        check(handle && handle->value,ONEUI_VIEW_INVALID_ARGUMENT,"View mount required");
        auto mount=handle->value;mount->check();
        check(callbackPhase==Phase::Idle && !mount->reserved && mount->active.expired(),ONEUI_VIEW_BUSY,"Set components before instantiate");
        if(registry) registryThread(registry);
        mount->components=registry ? registry->value : nullptr;
    });
}
