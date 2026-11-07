#include "napi_support.h"
#include "oneui/vpn/c_api.h"
#include "oneui/vpn/json_api.h"
#include <cstring>
#include <deque>
#include <map>
#include <thread>

namespace oneui::harmony {
namespace {
struct Command { uint64_t attempt; int operation; std::string json; };
struct Session {
    napi_env owner;
    std::thread::id thread = std::this_thread::get_id();
    OneUiVpnSession *value = nullptr;
    std::deque<Command> commands;
    static void start(void *self, uint64_t attempt, const char *json) {
        static_cast<Session *>(self)->commands.push_back({attempt, 1, json});
    }
    static void stop(void *self, uint64_t attempt) { static_cast<Session *>(self)->commands.push_back({attempt, 2, {}}); }
};
struct Registry { uint64_t next = 0; std::map<uint64_t, std::unique_ptr<Session>> values; std::mutex mutex; };
Registry &registry() { static auto *value = new Registry; return *value; }
Session &session(napi_env env, napi_value id) {
    const auto found = registry().values.find(handle(env, id));
    if (found == registry().values.end() || found->second->owner != env || found->second->thread != std::this_thread::get_id())
        throw std::runtime_error("Session thread or generation");
    return *found->second;
}
napi_value create(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        args(env, info, 0); auto &r = registry(); std::lock_guard<std::mutex> lock(r.mutex);
        if (r.values.size() >= 64) throw std::runtime_error("Session capacity");
        auto value = std::make_unique<Session>(); value->owner = env;
        OneUiVpnHostV1 host{sizeof(host), value.get(), Session::start, Session::stop};
        if (oneui_vpn_create_v1(&host, &value->value) != ONEUI_VPN_OK) throw std::runtime_error("Session creation");
        const auto id = ++r.next; r.values.emplace(id, std::move(value)); return string(env, std::to_string(id));
    });
}
napi_value connect(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 2); std::lock_guard<std::mutex> lock(registry().mutex); auto &value = session(env, a[0]);
        if (value.commands.size() >= 3) return number(env, ONEUI_VPN_INVALID_STATE);
        const auto json = text(env, a[1], 65536);
        return number(env, oneui_vpn_connect_json_v1(value.value, json.data(), json.size()));
    });
}
napi_value disconnect(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); std::lock_guard<std::mutex> lock(registry().mutex);
        return number(env, oneui_vpn_disconnect(session(env, a[0]).value));
    });
}
napi_value report(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 4); std::lock_guard<std::mutex> lock(registry().mutex);
        return number(env, oneui_vpn_report_v1(session(env, a[0]).value, handle(env, a[1]), integer(env, a[2]), integer(env, a[3])));
    });
}
napi_value snapshot(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); std::lock_guard<std::mutex> lock(registry().mutex);
        OneUiVpnSnapshotV1 state{}; state.struct_size = sizeof(state);
        if (oneui_vpn_snapshot_v1(session(env, a[0]).value, &state) != ONEUI_VPN_OK) throw std::runtime_error("Session snapshot");
        napi_value result; check(napi_create_object(env, &result));
        property(env, result, "state", number(env, state.state)); property(env, result, "errorCode", number(env, state.error_code));
        property(env, result, "attempt", string(env, std::to_string(state.attempt))); property(env, result, "revision", string(env, std::to_string(state.revision)));
        return result;
    });
}
napi_value take(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); std::lock_guard<std::mutex> lock(registry().mutex); auto &value = session(env, a[0]);
        if (value.commands.empty()) return undefined(env);
        auto command = std::move(value.commands.front()); value.commands.pop_front();
        napi_value result; check(napi_create_object(env, &result));
        property(env, result, "attempt", string(env, std::to_string(command.attempt))); property(env, result, "operation", number(env, command.operation));
        property(env, result, "configuration", string(env, command.json)); return result;
    });
}
napi_value destroy(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); std::lock_guard<std::mutex> lock(registry().mutex); const auto id = handle(env, a[0]);
        const auto code = oneui_vpn_destroy(session(env, a[0]).value);
        if (code == ONEUI_VPN_OK) registry().values.erase(id);
        return number(env, code);
    });
}
}
void registerSessions(napi_env env, napi_value exports) {
    const napi_property_descriptor methods[]{
        {"sessionCreate", nullptr, create, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sessionConnect", nullptr, connect, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sessionDisconnect", nullptr, disconnect, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sessionReport", nullptr, report, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sessionSnapshot", nullptr, snapshot, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sessionTake", nullptr, take, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"sessionDestroy", nullptr, destroy, nullptr, nullptr, nullptr, napi_default, nullptr}};
    check(napi_define_properties(env, exports, std::size(methods), methods));
}
}
