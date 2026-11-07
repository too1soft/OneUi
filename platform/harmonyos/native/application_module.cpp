#include "application_napi.h"
namespace {
napi_value init(napi_env env, napi_value exports) {
    return oneui::harmony::safe(env, [&] {
        oneui::harmony::application::registerApplication(env, exports);
        oneui::harmony::application::registerInput(env, exports); return exports;
    });
}
}
#if defined(ONEUI_NODE_TEST)
NAPI_MODULE(oneui_application, init)
#else
static napi_module definition{1, 0, nullptr, init, "oneui_application", nullptr, {0}};
extern "C" __attribute__((constructor)) void oneui_register_application() { napi_module_register(&definition); }
#endif
