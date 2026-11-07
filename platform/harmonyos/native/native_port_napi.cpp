#include "native_port.h"
#include <map>
#include <limits>

namespace oneui::harmony {
namespace {
struct Registry { std::mutex mutex; uint64_t next = 0; std::map<uint64_t, std::shared_ptr<NativePort>> values; };
// Unclosed ports remain alive until process exit; foreign Go callbacks must
// never outlive their context due to JS environment teardown.
Registry &registry() { static auto *value = new Registry; return *value; }
napi_value create(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 4); auto label = text(env, a[1]);
        const int mtu = integer(env, a[2]), flags = integer(env, a[3]);
        if (label.empty() || mtu < 1280 || mtu > 9000 || flags < 0 || flags > 3) throw std::runtime_error("Host configuration");
        auto &r = registry(); std::lock_guard<std::mutex> lock(r.mutex);
        if (r.values.size() >= 64 || r.next == std::numeric_limits<uint64_t>::max()) throw std::runtime_error("Host capacity");
        auto value = std::make_shared<NativePort>(); value->owner = env; value->backend = vpn::loadBackend(text(env, a[0]));
        if (!value->backend) throw std::runtime_error("Backend ABI");
        auto callbacks = NativePort::callbacks(*value);
        value->host = value->backend->create(label.data(), mtu, flags, &callbacks);
        if (!value->host) throw std::runtime_error("Host creation");
        const auto id = ++r.next; r.values.emplace(id, value);
        napi_value result; check(napi_create_object(env, &result));
        property(env, result, "port", string(env, std::to_string(id))); property(env, result, "host", string(env, std::to_string(value->host)));
        return result;
    });
}
napi_value poll(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); auto value = port(env, a[0]); mobile::NativeServiceRequest request;
        if (!value->services.take(request)) return undefined(env);
        napi_value result; check(napi_create_object(env, &result));
        property(env, result, "id", string(env, std::to_string(request.id))); property(env, result, "operation", number(env, request.operation));
        property(env, result, "payload", buffer(env, request.payload)); wipe(request.payload); return result;
    });
}
napi_value respond(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 4); auto value = port(env, a[0]); auto data = bytes(env, a[3]);
        std::string payload(data.begin(), data.end());
        volatile unsigned char *p = data.data(); for (size_t i = 0; i < data.size(); ++i) p[i] = 0;
        const int code = integer(env, a[2]);
        if (code > 0) throw std::runtime_error("Native status must be zero or negative");
        return boolean(env, value->services.complete(handle(env, a[1]), code, std::move(payload)));
    });
}
napi_value replace(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 2); auto value = port(env, a[0]); const int fd = integer(env, a[1]);
        // Executed only in the VPN extension. Joining has a bounded 200 ms
        // receive/poll wait and does not consume another N-API worker while a
        // Go worker is waiting for this OS callback's acknowledgment.
        std::lock_guard<std::mutex> lock(value->lifecycle);
        if (value->closed || (value->closing && fd >= 0)) throw std::runtime_error("Port closing");
        value->packets.stop();
        if (fd >= 0 && !value->packets.start(*value->backend, value->host, fd)) throw std::runtime_error("Packet descriptor");
        return undefined(env);
    });
}
napi_value changed(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); auto value = port(env, a[0]);
        return async(env, [value] { return Reply{value->backend->changed(value->host), {}}; }, true);
    });
}
napi_value close(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); const auto id = handle(env, a[0]); auto value = port(env, a[0]);
        return async(env, [value, id] {
            {
                std::lock_guard<std::mutex> lock(value->lifecycle);
                if (value->closed || value->closing) return Reply{-1, {}};
                value->closing = true; value->packets.stop();
            }
            // Keep servicing OS callbacks while Go joins and rolls back routes.
            const int code = value->backend->close(value->host);
            {
                std::lock_guard<std::mutex> lock(value->lifecycle);
                value->closed = code == 0; value->closing = false;
            }
            if (code == 0) {
                value->services.close(); auto &r = registry(); std::lock_guard<std::mutex> lock(r.mutex); r.values.erase(id);
            }
            return Reply{code, {}};
        }, true);
    });
}
napi_value status(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        auto a = args(env, info, 1); auto value = port(env, a[0]);
        napi_value result; check(napi_create_object(env, &result));
        property(env, result, "running", boolean(env, value->packets.running()));
        property(env, result, "error", number(env, value->packets.error())); return result;
    });
}
}
std::shared_ptr<NativePort> port(napi_env env, napi_value id) {
    auto &r = registry(); std::lock_guard<std::mutex> lock(r.mutex); const auto found = r.values.find(handle(env, id));
    if (found == r.values.end() || found->second->owner != env) throw std::runtime_error("Unknown native port");
    return found->second;
}
void registerPorts(napi_env env, napi_value exports) {
    const napi_property_descriptor methods[]{
        {"portCreate", nullptr, create, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"portPoll", nullptr, poll, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"portRespond", nullptr, respond, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"portReplace", nullptr, replace, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"portChanged", nullptr, changed, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"portStatus", nullptr, status, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"portClose", nullptr, close, nullptr, nullptr, nullptr, napi_default, nullptr}};
    check(napi_define_properties(env, exports, std::size(methods), methods));
}
}
