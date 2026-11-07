#include "napi_support.h"
#include "oneui/mobile/atomic_blob_files.h"

namespace oneui::harmony {
namespace {
napi_value read(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 2); auto directory = text(env, a[0], 4096), key = text(env, a[1], 64);
        return async(env, [directory, key] {
            const auto result = mobile::AtomicBlobFiles::read(directory, key);
            return Reply{0, {result.begin(), result.end()}};
        });
    });
}
napi_value write(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 3); auto directory = text(env, a[0], 4096), key = text(env, a[1], 64); auto data = bytes(env, a[2], 1024 * 1024 + 64);
        return async(env, [directory, key, data = std::move(data)] { mobile::AtomicBlobFiles::write(directory, key, data); return Reply{0, {}}; });
    });
}
}
void registerBlobFiles(napi_env env, napi_value exports) {
    const napi_property_descriptor methods[]{
        {"blobRead", nullptr, read, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"blobWrite", nullptr, write, nullptr, nullptr, nullptr, napi_default, nullptr}};
    check(napi_define_properties(env, exports, std::size(methods), methods));
}
}
