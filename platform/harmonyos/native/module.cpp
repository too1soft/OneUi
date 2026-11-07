#include "napi_support.h"
namespace {
napi_value init(napi_env env, napi_value exports) {
    return oneui::harmony::safe(env, [&] {
        oneui::harmony::registerModules(env, exports);
        oneui::harmony::registerPorts(env, exports);
        oneui::harmony::registerSessions(env, exports);
        oneui::harmony::registerBlobFiles(env, exports);
        oneui::harmony::registerNetwork(env, exports);
        oneui::harmony::registerChannels(env, exports);
        return exports;
    });
}
}
#if defined(ONEUI_NODE_TEST)
NAPI_MODULE(oneui_native_services, init)
#else
static napi_module definition{1, 0, nullptr, init, "oneui_native_services", nullptr, {0}};
extern "C" __attribute__((constructor)) void oneui_register_native_services() { napi_module_register(&definition); }
#endif
