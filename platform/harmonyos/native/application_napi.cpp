#include "application_napi.h"
#include "oneui/mobile/native_module_posix.h"
#include <cmath>
#include <map>
#include <cstring>
#include "oneui/view.h"
#include "oneui/unicode.h"

namespace oneui::harmony::application {
namespace {
struct Registry { uint64_t next = 0; std::mutex mutex; std::map<uint64_t, std::shared_ptr<Host>> hosts; };
Registry &registry() { static auto *value = new Registry; return *value; }
napi_value create(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1);
        using Factory = mobile::Application *(*)(mobile::ApplicationServices *);
        auto factory = reinterpret_cast<Factory>(mobile::NativeModulePosix::resolve(text(env, a[0]), "oneui_create_mobile_application"));
        if (!factory) throw std::runtime_error("Packaged Widget factory");
        auto &r = registry(); std::lock_guard<std::mutex> lock(r.mutex);
        if (r.hosts.size() >= 4) throw std::runtime_error("Application host capacity");
        auto value = std::make_shared<Host>(env, [factory](mobile::ApplicationServices &services) {
            return std::unique_ptr<mobile::Application>(factory(&services));
        });
        const auto id = ++r.next; r.hosts.emplace(id, value); return string(env, std::to_string(id));
    });
}
napi_value viewport(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 9); auto value = host(env, a[0]);
        OneUiMobileViewportV1 v{static_cast<float>(real(env, a[1])), static_cast<float>(real(env, a[2])), static_cast<float>(real(env, a[3])),
            static_cast<float>(real(env, a[4])), static_cast<float>(real(env, a[5])), static_cast<float>(real(env, a[6])),
            static_cast<float>(real(env, a[7])), static_cast<float>(real(env, a[8]))};
        return boolean(env, value->surface.viewport(v));
    });
}
napi_value frame(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); auto value = host(env, a[0]);
        if (!value->surface.paint()) return undefined(env);
        napi_value result, owner, rgba; void *pixels; check(napi_create_object(env, &result));
        const auto &source = value->surface.pixels();
        check(napi_create_arraybuffer(env, source.size(), &pixels, &owner)); std::memcpy(pixels, source.data(), source.size());
        check(napi_create_typedarray(env, napi_uint8_array, source.size(), owner, 0, &rgba));
        property(env, result, "width", number(env, value->surface.width())); property(env, result, "height", number(env, value->surface.height()));
        property(env, result, "rgba", rgba); return result;
    });
}
napi_value pointer(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 7); auto value = host(env, a[0]);
        OneUiMobilePointerV1 event{integer(env, a[1]), integer(env, a[2]), integer(env, a[3]),
            static_cast<float>(real(env, a[4])), static_cast<float>(real(env, a[5])), static_cast<float>(real(env, a[6])), mobile::harmony::clockNs(), 0};
        return number(env, value->surface.core.pointer(event));
    });
}
napi_value lifecycle(napi_env env, napi_callback_info info) {
    return safe(env, [&] { auto a = args(env, info, 2); return number(env, host(env, a[0])->surface.core.lifecycle(integer(env, a[1]))); });
}
napi_value cancelPointers(napi_env env, napi_callback_info info) {
    return safe(env, [&] { auto a = args(env, info, 1); return number(env, host(env, a[0])->surface.core.cancelPointers()); });
}
napi_value theme(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 4); auto value = host(env, a[0]); const int appearance = integer(env, a[1]);
        value->surface.core.application().appearance().observe(appearance, true);
        value->surface.core.application().typography(static_cast<float>(real(env, a[2])), static_cast<float>(real(env, a[3])));
        value->surface.core.syncAppearance(); value->surface.core.runtime().invalidate(); return undefined(env);
    });
}
napi_value systemBars(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1);
        OneUiMobileAppearanceStateV1 state{};
        if (host(env, a[0])->surface.core.application().appearance().state(state) != ONEUI_MOBILE_OK)
            throw std::runtime_error("Application appearance");
        napi_value result, status, navigation;
        check(napi_create_object(env, &result));
        check(napi_create_uint32(env, state.status_argb, &status));
        check(napi_create_uint32(env, state.navigation_argb, &navigation));
        property(env, result, "revision", string(env, std::to_string(state.revision)));
        property(env, result, "themeMode", number(env, state.options.theme_mode));
        property(env, result, "statusColor", status);
        property(env, result, "navigationColor", navigation);
        property(env, result, "lightStatusIcons", boolean(env, state.status_icons == ONEUI_MOBILE_ICONS_LIGHT));
        property(env, result, "lightNavigationIcons", boolean(env, state.navigation_icons == ONEUI_MOBILE_ICONS_LIGHT));
        return result;
    });
}
napi_value acknowledgeSystemBars(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 2);
        auto &appearance = host(env, a[0])->surface.core.application().appearance();
        return boolean(env, appearance.acknowledge(handle(env, a[1])) == ONEUI_MOBILE_OK);
    });
}
napi_value take(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); auto value = host(env, a[0]); mobile::ApplicationRequest request;
        if (!value->surface.core.services().take(request)) return undefined(env);
        if (value->requests.size() >= 16) { value->surface.core.reply(request.id, -2, {}); return undefined(env); }
        value->requests.insert(request.id); napi_value result; check(napi_create_object(env, &result));
        property(env, result, "id", string(env, std::to_string(request.id))); property(env, result, "action", string(env, request.action));
        property(env, result, "payload", buffer(env, request.payload));
        volatile char *data = request.payload.empty() ? nullptr : &request.payload[0]; for (size_t i = 0; i < request.payload.size(); ++i) data[i] = 0;
        return result;
    });
}
napi_value reply(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 4); auto value = host(env, a[0]); auto requestText = text(env, a[1], 20);
        const auto request = requestText == "0" ? 0 : handle(env, a[1]);
        if (request && value->requests.find(request) == value->requests.end()) return boolean(env, false);
        const auto code = integer(env, a[2]);
        auto payload = bytes(env, a[3], 1024 * 1024); std::string data(payload.begin(), payload.end());
        if (request && value->requests.erase(request) != 1) return boolean(env, false);
        value->surface.core.reply(request, code, data);
        volatile unsigned char *p = payload.data(); for (size_t i = 0; i < payload.size(); ++i) p[i] = 0;
        volatile char *q = data.empty() ? nullptr : &data[0]; for (size_t i = 0; i < data.size(); ++i) q[i] = 0;
        return boolean(env, true);
    });
}
napi_value back(napi_env env, napi_callback_info info) {
    return safe(env, [&] { auto a = args(env, info, 1); return boolean(env, host(env, a[0])->surface.core.back()); });
}
#if defined(ONEUI_NODE_TEST)
// Read-only portable-host inspection. Deliberately excludes editor values,
// descriptions and service payloads; input tests use the redacting IME API.
napi_value inspect(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); auto value = host(env, a[0]);
        value->surface.paint();
        napi_value result; check(napi_create_array(env, &result));
        uint32_t count = 0;
        std::function<void(const std::shared_ptr<Widget>&)> walk;
        walk = [&](const std::shared_ptr<Widget>& widget) {
            if (!widget || !widget->visible() || count >= 512) return;
            const auto info = widget->accessibilityInfo();
            if (!info.name.empty()) {
                napi_value item; check(napi_create_object(env, &item));
                property(env,item,"name",string(env,unicode::toUtf8(info.name)));
                property(env,item,"role",number(env,static_cast<int>(info.role)));
                property(env,item,"disabled",boolean(env,widget->disabled()));
                property(env,item,"selected",boolean(env,info.state.selected));
                const auto r = widget->frame();
                property(env,item,"x",number(env,static_cast<int>(r.x)));
                property(env,item,"y",number(env,static_cast<int>(r.y)));
                property(env,item,"width",number(env,static_cast<int>(r.width)));
                property(env,item,"height",number(env,static_cast<int>(r.height)));
                check(napi_set_element(env,result,count++,item));
            }
            if (auto view = std::dynamic_pointer_cast<View>(widget))
                for (const auto& child : view->children()) walk(child);
        };
        walk(value->surface.core.application().root()); return result;
    });
}
#endif
napi_value destroy(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); auto value = host(env, a[0]); const auto id = handle(env, a[0]);
        std::lock_guard<std::mutex> lock(registry().mutex); registry().hosts.erase(id); return undefined(env);
    });
}
}
std::shared_ptr<Host> host(napi_env env, napi_value value) {
    auto &r = registry(); std::lock_guard<std::mutex> lock(r.mutex); const auto found = r.hosts.find(handle(env, value));
    if (found == r.hosts.end() || found->second->owner != env || found->second->thread != std::this_thread::get_id())
        throw std::runtime_error("Application thread or generation");
    return found->second;
}
double real(napi_env env, napi_value value) {
    double result; check(napi_get_value_double(env, value, &result)); if (!std::isfinite(result)) throw std::runtime_error("Finite coordinate"); return result;
}
void registerApplication(napi_env env, napi_value exports) {
    const napi_property_descriptor methods[]{
        {"create", nullptr, create, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"viewport", nullptr, viewport, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"frame", nullptr, frame, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"pointer", nullptr, pointer, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"cancelPointers", nullptr, cancelPointers, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"lifecycle", nullptr, lifecycle, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"theme", nullptr, theme, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"systemBars", nullptr, systemBars, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"acknowledgeSystemBars", nullptr, acknowledgeSystemBars, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"take", nullptr, take, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"reply", nullptr, reply, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"back", nullptr, back, nullptr, nullptr, nullptr, napi_default, nullptr},
        #if defined(ONEUI_NODE_TEST)
        {"inspect", nullptr, inspect, nullptr, nullptr, nullptr, napi_default, nullptr},
#endif
        {"destroy", nullptr, destroy, nullptr, nullptr, nullptr, napi_default, nullptr}};
    check(napi_define_properties(env, exports, std::size(methods), methods));
}
}
