#include "napi_support.h"
#include "oneui/mobile/local_channel.h"
#include <map>
#include <mutex>
#include <limits>

namespace oneui::harmony {
namespace {
using Channel = mobile::LocalChannel;
struct Entry { napi_env owner; std::shared_ptr<Channel> channel; };
struct Registry { std::mutex mutex; uint64_t next = 0; std::map<uint64_t, Entry> channels; };
Registry &registry() { static auto *value = new Registry; return *value; }
std::shared_ptr<Channel> channel(napi_env env, napi_value value) {
    const auto id = handle(env, value); auto &r = registry(); std::lock_guard<std::mutex> lock(r.mutex);
    const auto found = r.channels.find(id);
    if (found == r.channels.end() || found->second.owner != env) throw std::runtime_error("Channel owner or generation");
    return found->second.channel;
}
std::string insert(napi_env env, std::shared_ptr<Channel> channel) {
    if (!channel) return {};
    auto &r = registry(); std::lock_guard<std::mutex> lock(r.mutex);
    if (r.channels.size() >= 16 || r.next == std::numeric_limits<uint64_t>::max()) throw std::runtime_error("Channel capacity");
    const auto id = ++r.next; r.channels.emplace(id, Entry{env, std::move(channel)}); return std::to_string(id);
}
napi_value open(napi_env env, napi_callback_info info, bool server) {
    return safe(env, [&] {
        const auto a = args(env, info, 3); auto directory = text(env, a[0], 4096), name = text(env, a[1], 64), token = text(env, a[2], 64);
        return async(env, [env, directory, name, token, server] {
            return Reply{0, insert(env, server ? Channel::listen(directory, name, token) : Channel::connect(directory, name, token))};
        });
    });
}
napi_value listen(napi_env env, napi_callback_info info) { return open(env, info, true); }
napi_value connect(napi_env env, napi_callback_info info) { return open(env, info, false); }
napi_value accept(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        const auto a = args(env, info, 1); auto value = channel(env, a[0]);
        return async(env, [env, value] { return Reply{0, insert(env, value->accept())}; });
    });
}
napi_value receive(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        const auto a = args(env, info, 1); auto value = channel(env, a[0]);
        return async(env, [value] { std::string data; value->receive(data); return Reply{0, std::move(data)}; });
    });
}
napi_value send(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        const auto a = args(env, info, 2); auto value = channel(env, a[0]); auto bytes = oneui::harmony::bytes(env, a[1], 1024 * 1024);
        auto data = std::shared_ptr<std::string>(new std::string(bytes.begin(), bytes.end()), [](std::string *value) {
            volatile char *p = value->empty() ? nullptr : &(*value)[0]; for (size_t i = 0; i < value->size(); ++i) p[i] = 0; delete value;
        });
        volatile unsigned char *p = bytes.data(); for (size_t i = 0; i < bytes.size(); ++i) p[i] = 0;
        return async(env, [value, data] { value->send(*data); return Reply{0, {}}; });
    });
}
napi_value close(napi_env env, napi_callback_info info) {
    return safe(env, [&] {
        const auto a = args(env, info, 1); const auto id = handle(env, a[0]); auto value = channel(env, a[0]);
        return async(env, [value, id] {
            value->close(); auto &r = registry(); std::lock_guard<std::mutex> lock(r.mutex); r.channels.erase(id); return Reply{0, {}};
        });
    });
}
}
void registerChannels(napi_env env, napi_value exports) {
    const napi_property_descriptor methods[]{
        {"channelListen", nullptr, listen, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"channelConnect", nullptr, connect, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"channelAccept", nullptr, accept, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"channelReceive", nullptr, receive, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"channelSend", nullptr, send, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"channelClose", nullptr, close, nullptr, nullptr, nullptr, napi_default, nullptr}};
    check(napi_define_properties(env, exports, std::size(methods), methods));
}
}
