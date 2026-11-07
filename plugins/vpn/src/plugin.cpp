#include "oneui/plugins/vpn.h"
#include <memory>
#include <set>
#include <thread>

namespace {
struct VpnPlugin {
    const std::thread::id owner = std::this_thread::get_id();
    std::set<OneUiVpnSession *> sessions;
    bool stopping = false;
};
OneUiVpnResult createSession(void *instance, const OneUiVpnHostV1 *host, OneUiVpnSession **out) {
    if (!out) return ONEUI_VPN_INVALID_ARGUMENT;
    *out = nullptr;
    if (!instance) return ONEUI_VPN_INVALID_ARGUMENT;
    auto &plugin = *static_cast<VpnPlugin *>(instance);
    if (plugin.owner != std::this_thread::get_id()) return ONEUI_VPN_WRONG_THREAD;
    if (plugin.stopping || plugin.sessions.size() >= 16) return ONEUI_VPN_INVALID_STATE;
    OneUiVpnSession *session = nullptr;
    const auto result = oneui_vpn_create_v1(host, &session);
    if (result != ONEUI_VPN_OK) return result;
    try { plugin.sessions.insert(session); }
    catch (...) { oneui_vpn_destroy(session); return ONEUI_VPN_FAILED; }
    *out = session;
    return ONEUI_VPN_OK;
}
OneUiVpnResult destroySession(void *instance, OneUiVpnSession *session) {
    if (!instance || !session) return ONEUI_VPN_INVALID_ARGUMENT;
    auto &plugin = *static_cast<VpnPlugin *>(instance);
    if (plugin.owner != std::this_thread::get_id()) return ONEUI_VPN_WRONG_THREAD;
    const auto found = plugin.sessions.find(session);
    if (found == plugin.sessions.end()) return ONEUI_VPN_INVALID_ARGUMENT;
    const auto result = oneui_vpn_destroy(session);
    if (result == ONEUI_VPN_OK) plugin.sessions.erase(found);
    return result;
}
const OneUiVpnPluginApiV1 api{
    sizeof(OneUiVpnPluginApiV1), 1, createSession, destroySession,
    oneui_vpn_connect_v1, oneui_vpn_disconnect, oneui_vpn_report_v1, oneui_vpn_snapshot_v1
};
OneUiPluginResult attach(const OneUiPluginContextV1 *context, void *, void **instance) {
    if (!instance) return ONEUI_PLUGIN_INVALID_ARGUMENT;
    *instance = nullptr;
    if (!context || context->struct_size != sizeof(*context) || context->abi_version != 1 || !context->publish)
        return ONEUI_PLUGIN_INCOMPATIBLE;
    try {
        auto plugin = std::make_unique<VpnPlugin>();
        *instance = plugin.release();
        const OneUiPluginServiceV1 service{sizeof(service), 1, &api, *instance};
        return context->publish(context->token, ONEUI_VPN_SERVICE_ID, &service);
    } catch (...) { return ONEUI_PLUGIN_FAILED; }
}
OneUiPluginResult detach(void *instance) {
    if (!instance) return ONEUI_PLUGIN_OK;
    auto &plugin = *static_cast<VpnPlugin *>(instance);
    if (plugin.owner != std::this_thread::get_id()) return ONEUI_PLUGIN_WRONG_THREAD;
    plugin.stopping = true;
    for (auto i = plugin.sessions.begin(); i != plugin.sessions.end();) {
        auto *session = *i;
        const auto disconnect = oneui_vpn_disconnect(session);
        if (disconnect != ONEUI_VPN_OK) return ONEUI_PLUGIN_FAILED;
        const auto destroy = oneui_vpn_destroy(session);
        if (destroy == ONEUI_VPN_INVALID_STATE) { ++i; continue; }
        if (destroy != ONEUI_VPN_OK) return ONEUI_PLUGIN_FAILED;
        i = plugin.sessions.erase(i);
    }
    if (!plugin.sessions.empty()) return ONEUI_PLUGIN_BUSY; // Wait for real native STOPPED callbacks.
    delete &plugin;
    return ONEUI_PLUGIN_OK;
}
const OneUiPluginDescriptorV1 descriptor{
    sizeof(descriptor), ONEUI_PLUGIN_ABI_V1, ONEUI_VPN_PLUGIN_ID, {0, 1, 0},
    nullptr, 0, nullptr, attach, detach
};
}
const OneUiPluginDescriptorV1 *oneui_vpn_plugin_descriptor_v1(void) { return &descriptor; }
