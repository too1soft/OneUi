#include "../runtime.h"
#include "instance.h"
#include "../event_dispatch.h"
#include "oneui/ui_theme.h"

namespace oneui::views::components {
namespace {
template<class F> int call(InstanceState *state,F action) noexcept {
    try {
        check(state,ONEUI_VIEW_INVALID_ARGUMENT,"Component context required");
        check(state->thread==std::this_thread::get_id(),ONEUI_VIEW_WRONG_THREAD,"Component context thread");
        check(state && !state->disconnected,ONEUI_VIEW_CLOSED,"Component is disconnected");
        auto node = state->node.lock(); auto runtime = node ? node->owner.lock() : nullptr;
        check(bool(runtime),ONEUI_VIEW_CLOSED,"Component view is closed");
        runtime->onThread(); runtime->alive();
        return action(*state,*node,*runtime);
    } catch(const Failure &e) { return e.result; }
      catch(const std::bad_alloc &) { return ONEUI_VIEW_OUT_OF_MEMORY; }
      catch(...) { return ONEUI_VIEW_FOREIGN_FAILURE; }
}
int emit(void *token,uint32_t id,const OneUiValue *values,size_t count) {
    return call(static_cast<InstanceState*>(token),[&](auto &state,auto &node,auto &runtime) {
        check(callbackPhase == Phase::Idle && !runtime.building && !runtime.staging && !node.applying,
              ONEUI_VIEW_REENTRANT,"Component event during update");
        const auto &event = state.factory->contract.event(id);
        check(count == event.args.size() && (!count || values),ONEUI_VIEW_TYPE,"Component event argument count");
        std::vector<Value> args;
        for(size_t i=0;i<count;++i) { auto v=Value::copy(values[i]); check(v.tag==event.args[i].tag,ONEUI_VIEW_TYPE,"Component event argument type"); args.push_back(std::move(v)); }
        const auto sequence = runtime.error.sequence;
        eventDispatcher(state.node,id)(std::move(args));
        return runtime.error.sequence == sequence ? ONEUI_VIEW_OK : runtime.error.result;
    });
}
int change(void *token,uint32_t id,const OneUiValue *value) {
    return call(static_cast<InstanceState*>(token),[&](auto &state,auto &node,auto &runtime) {
        check(callbackPhase==Phase::Idle && !runtime.building && !runtime.staging && !node.applying,
              ONEUI_VIEW_REENTRANT,"Component model change during update");
        check(value && id==state.factory->contract.modelProperty,ONEUI_VIEW_TYPE,"Not a model property");
        auto copied=Value::copy(*value);
        check(copied.tag==state.factory->contract.property(id).tag,ONEUI_VIEW_TYPE,"Model property type");
        for(const auto &binding:node.bindings)
            if(binding.kind==oneview::BIND && binding.record[1]==id && (binding.record[3]>>16)==2)
                if(auto scope=binding.scope.lock()) { runtime.set(scope,binding.record[2],copied); return int(ONEUI_VIEW_OK); }
        return int(ONEUI_VIEW_NOT_FOUND);
    });
}
uint32_t rgba(Color c) { return uint32_t(c.r)<<24 | uint32_t(c.g)<<16 | uint32_t(c.b)<<8 | c.a; }
int style(void *token,uint32_t states,OneUiComponentStyleV1 *out) {
    return call(static_cast<InstanceState*>(token),[&](auto &state,auto &,auto &) {
        check(out && out->struct_size==sizeof(*out) && state.sheet,ONEUI_VIEW_INVALID_ARGUMENT,"Component style output");
        auto node=state.styleNode; node.state=states;
        auto box=state.sheet->resolve(node);auto palette=ui::nativePalette(*state.sheet);
        *out={sizeof(*out),rgba(box.foreground.value_or(palette.ink)),rgba(box.background.color.value_or(palette.surface)),
              rgba(box.borderColor.value_or(palette.line)),box.fontSize.value_or(13),box.radius.value_or(0),box.borderWidth.value_or(0)};
        return int(ONEUI_VIEW_OK);
    });
}
}
void initializeHost(InstanceState &state) { state.host={sizeof(state.host),ONEUI_COMPONENT_ABI_V1,&state,emit,change,style}; }
}
