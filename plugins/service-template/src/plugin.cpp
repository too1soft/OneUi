#include "example/counter.h"
#include <new>
#include <thread>
namespace {
struct Counter {std::thread::id owner=std::this_thread::get_id();uint64_t value=0;};
OneUiPluginResult next(void *opaque,uint64_t *out) {
    if(out)*out=0;
    if(!opaque || !out)return ONEUI_PLUGIN_INVALID_ARGUMENT;
    auto &c=*static_cast<Counter*>(opaque);
    if(c.owner!=std::this_thread::get_id())return ONEUI_PLUGIN_WRONG_THREAD;
    if(c.value==UINT64_MAX)return ONEUI_PLUGIN_LIMIT;
    *out=++c.value;return ONEUI_PLUGIN_OK;
}
const ExampleCounterApiV1 api{sizeof(api),1,next};
OneUiPluginResult attach(const OneUiPluginContextV1 *context,void *,void **out) {
    *out=new(std::nothrow) Counter;
    if(!*out)return ONEUI_PLUGIN_FAILED;
    const OneUiPluginServiceV1 service{sizeof(service),1,&api,*out};
    return context->publish(context->token,"example.counter.value",&service);
}
OneUiPluginResult detach(void *opaque) {delete static_cast<Counter*>(opaque);return ONEUI_PLUGIN_OK;}
const OneUiPluginDescriptorV1 descriptor{sizeof(descriptor),1,"example.counter",{0,1,0},nullptr,0,nullptr,attach,detach};
}
const OneUiPluginDescriptorV1 *example_counter_descriptor_v1(void) {return &descriptor;}
