#include "napi_support.h"
#include "oneui/mobile/native_module_posix.h"

namespace oneui::harmony {
namespace {
napi_value open(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 2);
        const auto id = mobile::NativeModulePosix::open(text(env, a[0]), text(env, a[1]));
        if (!id) throw std::runtime_error("Packaged module unavailable");
        return string(env, std::to_string(id));
    });
}
napi_value call(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 3); const auto id = handle(env, a[0]); const auto command = integer(env, a[1]);
        auto request = bytes(env, a[2], 1024 * 1024);
        return async(env, [id, command, request = std::move(request)]() mutable {
            std::vector<unsigned char> reply;
            int code = mobile::NativeModulePosix::call(id, command, std::move(request), reply);
            std::string result(reply.begin(), reply.end());
            volatile unsigned char *data = reply.data(); for (size_t i = 0; i < reply.size(); ++i) data[i] = 0;
            return Reply{code, std::move(result)};
        }, true);
    });
}
napi_value close(napi_env env, napi_callback_info info) {
    return safe(env, [&] { auto a = args(env, info, 1); mobile::NativeModulePosix::release(handle(env, a[0])); return undefined(env); });
}
napi_value interrupt(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 3); std::vector<unsigned char> reply;
        const int code = mobile::NativeModulePosix::call(handle(env, a[0]), integer(env, a[1]), bytes(env, a[2], 4096), reply);
        if (code != 0 || reply.size() > 4096) throw std::runtime_error("Native interrupt response");
        std::string output(reply.begin(), reply.end());
        volatile unsigned char *data = reply.data(); for (size_t i = 0; i < reply.size(); ++i) data[i] = 0;
        auto result = buffer(env, output);
        volatile char *p = output.empty() ? nullptr : &output[0]; for (size_t i = 0; i < output.size(); ++i) p[i] = 0;
        return result;
    });
}
}
void registerModules(napi_env env, napi_value exports) {
    const napi_property_descriptor methods[]{
        {"moduleOpen", nullptr, open, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"moduleCall", nullptr, call, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"moduleInterrupt", nullptr, interrupt, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"moduleClose", nullptr, close, nullptr, nullptr, nullptr, napi_default, nullptr}};
    check(napi_define_properties(env, exports, std::size(methods), methods));
}
}
