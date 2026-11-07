#include "oneui/vpn/session.h"
#include <new>
struct OneUiVpnSession { oneui::vpn::Session value; explicit OneUiVpnSession(OneUiVpnHostV1 h) : value(h) {} };
namespace {
template<class F> OneUiVpnResult guarded(F f) noexcept {
    try { return f(); } catch (...) { return ONEUI_VPN_FAILED; }
}
}
OneUiVpnResult oneui_vpn_create_v1(const OneUiVpnHostV1 *host, OneUiVpnSession **out) {
    if (!out) return ONEUI_VPN_INVALID_ARGUMENT;
    *out = nullptr;
    if (!host || host->struct_size != sizeof(*host) || !host->start || !host->stop)
        return ONEUI_VPN_INVALID_ARGUMENT;
    return guarded([&] { *out = new OneUiVpnSession(*host); return ONEUI_VPN_OK; });
}
OneUiVpnResult oneui_vpn_destroy(OneUiVpnSession *s) {
    if (!s) return ONEUI_VPN_INVALID_ARGUMENT;
    const auto result = s->value.destroyable();
    if (result != ONEUI_VPN_OK) return result;
    delete s;
    return ONEUI_VPN_OK;
}
OneUiVpnResult oneui_vpn_connect_v1(OneUiVpnSession *s, const OneUiVpnConfigV1 *c) {
    if (!s || !c) return ONEUI_VPN_INVALID_ARGUMENT;
    return guarded([&] { return s->value.connect(*c); });
}
OneUiVpnResult oneui_vpn_disconnect(OneUiVpnSession *s) {
    if (!s) return ONEUI_VPN_INVALID_ARGUMENT;
    return guarded([&] { return s->value.disconnect(); });
}
OneUiVpnResult oneui_vpn_report_v1(OneUiVpnSession *s, uint64_t attempt, int32_t event, int32_t error) {
    if (!s) return ONEUI_VPN_INVALID_ARGUMENT;
    return guarded([&] { return s->value.report(attempt, event, error); });
}
OneUiVpnResult oneui_vpn_snapshot_v1(OneUiVpnSession *s, OneUiVpnSnapshotV1 *out) {
    if (!s || !out) return ONEUI_VPN_INVALID_ARGUMENT;
    return s->value.snapshot(*out);
}
