#include "native_bridge.h"
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace { std::mutex mutex; uint64_t next = 100; std::map<uint64_t, OneUiVpnNativeCallbacks> hosts; }
extern "C" uint32_t oneui_vpn_go_abi_version() { return 1; }
extern "C" uint64_t oneui_vpn_go_host_create(char *, int, int, OneUiVpnNativeCallbacks *callbacks) {
    if (!callbacks || callbacks->abi_version != 1 || callbacks->struct_size != sizeof(*callbacks)) return 0;
    std::lock_guard<std::mutex> lock(mutex); hosts[++next] = *callbacks; return next;
}
extern "C" int oneui_vpn_go_packet_inject(uint64_t, void *, int, int) { return 0; }
extern "C" int oneui_vpn_go_packet_receive(uint64_t, void *, int, int timeout) { std::this_thread::sleep_for(std::chrono::milliseconds(timeout)); return -3; }
extern "C" int oneui_vpn_go_network_changed(uint64_t id) {
    OneUiVpnNativeCallbacks cb;
    { std::lock_guard<std::mutex> lock(mutex); if (!hosts.count(id)) return -1; cb = hosts.at(id); }
    char state[4096]; return cb.read_network_state(cb.context, state, sizeof(state)) > 0 ? 0 : -4;
}
extern "C" int oneui_vpn_go_host_close(uint64_t id) {
    OneUiVpnNativeCallbacks cb;
    { std::lock_guard<std::mutex> lock(mutex); if (!hosts.count(id)) return -1; cb = hosts.at(id); }
    const char config[] = "{\"addresses\":[],\"routes\":[]}";
    const int code = cb.apply_network(cb.context, config, sizeof(config) - 1);
    if (code != 0) return code;
    std::lock_guard<std::mutex> lock(mutex); hosts.erase(id); return 0;
}
extern "C" int oneui_fixture_call(int command, void *input, int length, void *output, int capacity) {
    if (command == 2) return capacity + 1;
    if (command == 3) return -17;
    const uint64_t id = std::stoull(std::string(static_cast<char *>(input), static_cast<size_t>(length)));
    OneUiVpnNativeCallbacks cb;
    { std::lock_guard<std::mutex> lock(mutex); if (!hosts.count(id)) return -1; cb = hosts.at(id); }
    const char config[] = "{\"addresses\":[],\"routes\":[]}";
    const unsigned char data[]{0, 255, 42, 0, 10};
    if (cb.apply_network(cb.context, config, sizeof(config) - 1) != 0 ||
        cb.protect_socket(cb.context, 17) != 0 ||
        cb.secure_write(cb.context, "fixture-key", data, sizeof(data)) != 0 ||
        cb.read_network_state(cb.context, output, capacity) <= 0) return -4;
    return cb.secure_read(cb.context, "fixture-key", output, capacity);
}
