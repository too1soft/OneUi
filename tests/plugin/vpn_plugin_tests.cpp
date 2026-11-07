#include "oneui/plugins/vpn.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
struct Host { uint64_t attempt = 0; int starts = 0, stops = 0; };
void start(void *data, uint64_t attempt, const char *) {
    auto &host = *static_cast<Host *>(data);
    host.attempt = attempt; ++host.starts;
}
void stop(void *data, uint64_t) { ++static_cast<Host *>(data)->stops; }
}
int main() {
    try {
        OneUiPluginRuntime *runtime = nullptr;
        require(oneui_plugin_runtime_create_v1(&runtime) == ONEUI_PLUGIN_OK, "create runtime");
        require(oneui_plugin_runtime_register_v1(runtime, oneui_vpn_plugin_descriptor_v1()) == ONEUI_PLUGIN_OK, "register VPN plugin");
        require(oneui_plugin_runtime_start(runtime) == ONEUI_PLUGIN_OK, "start plugin");
        OneUiPluginLease *lease = nullptr;
        require(oneui_plugin_runtime_acquire_v1(runtime, ONEUI_VPN_SERVICE_ID, 1, &lease) == ONEUI_PLUGIN_OK, "acquire VPN service");
        OneUiPluginServiceV1 service{};
        require(oneui_plugin_lease_service_v1(lease, &service) == ONEUI_PLUGIN_OK, "get service");
        const auto &api = *static_cast<const OneUiVpnPluginApiV1 *>(service.api);
        require(api.struct_size == sizeof(api) && api.abi_version == 1, "VPN table contract");
        Host host;
        OneUiVpnHostV1 callbacks{sizeof(callbacks), &host, start, stop};
        OneUiVpnSession *session = nullptr;
        require(api.create(service.instance, &callbacks, &session) == ONEUI_VPN_OK, "create tracked session");
        OneUiVpnPrefixV1 address{4, {10, 42, 0, 1}, 24};
        OneUiVpnPrefixV1 route{4, {10, 42, 0, 0}, 24};
        OneUiVpnConfigV1 config{sizeof(config), "Plugin contract", 1400, 0, &address, 1, &route, 1, nullptr, 0, nullptr, 0};
        require(api.connect(session, &config) == ONEUI_VPN_OK && host.starts == 1, "existing VPN state machine");
        require(api.destroy(service.instance, session) == ONEUI_VPN_INVALID_STATE, "live native resources retained");
        require(oneui_plugin_runtime_stop(runtime) == ONEUI_PLUGIN_BUSY && host.stops == 0, "consumer lease prevents stop");
        require(oneui_plugin_lease_release(lease) == ONEUI_PLUGIN_OK, "release consumer lease");
        require(oneui_plugin_runtime_stop(runtime) == ONEUI_PLUGIN_BUSY && host.stops == 1, "cancel native session and wait for cleanup");
        require(oneui_plugin_runtime_destroy(runtime) == ONEUI_PLUGIN_BUSY, "retain plugin until native cleanup");
        // Simulate the native host's asynchronous cleanup acknowledgment using
        // the original public VPN ABI; no plugin service lease is needed by it.
        require(oneui_vpn_report_v1(session, host.attempt, ONEUI_VPN_STOPPED, 0) == ONEUI_VPN_OK, "native cleanup receipt");
        require(oneui_plugin_runtime_stop(runtime) == ONEUI_PLUGIN_OK, "retry cleanup frees tracked session");
        require(oneui_plugin_runtime_destroy(runtime) == ONEUI_PLUGIN_OK, "destroy stopped runtime");
        std::cout << "VPN plugin lifecycle passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return EXIT_FAILURE; }
}
