#include "oneui/vpn/session.h"
#include <stdexcept>
#include <vector>

namespace oneui::vpn {
Session::Session(OneUiVpnHostV1 host) : host_(host), owner_(std::this_thread::get_id()) {
    if (host.struct_size != sizeof(host) || !host.start || !host.stop)
        throw std::invalid_argument("OneUI VPN host callbacks are required");
    state_.struct_size = sizeof(state_);
}
OneUiVpnResult Session::checkThread() const {
    return owner_ == std::this_thread::get_id() ? ONEUI_VPN_OK : ONEUI_VPN_WRONG_THREAD;
}
void Session::change(int32_t state, int32_t error) {
    state_.state = state;
    state_.error_code = error;
    ++state_.revision;
}
OneUiVpnResult Session::destroyable() const {
    if (checkThread() != ONEUI_VPN_OK) return ONEUI_VPN_WRONG_THREAD;
    return !cleanupPending_ && (state_.state == ONEUI_VPN_IDLE || state_.state == ONEUI_VPN_ERROR)
        ? ONEUI_VPN_OK : ONEUI_VPN_INVALID_STATE;
}
OneUiVpnResult Session::connect(const OneUiVpnConfigV1 &config) {
    const auto ready = destroyable();
    if (ready != ONEUI_VPN_OK) return ready;
    size_t size = 0;
    auto result = oneui_vpn_config_json_v1(&config, nullptr, 0, &size);
    if (result != ONEUI_VPN_BUFFER_TOO_SMALL) return result;
    std::vector<char> json(size);
    result = oneui_vpn_config_json_v1(&config, json.data(), json.size(), &size);
    if (result != ONEUI_VPN_OK) return result;
    ++state_.attempt;
    cleanupPending_ = true;
    stopRequested_ = false;
    change(ONEUI_VPN_AUTHORIZING);
    host_.start(host_.context, state_.attempt, json.data());
    return ONEUI_VPN_OK;
}
OneUiVpnResult Session::disconnect() {
    if (checkThread() != ONEUI_VPN_OK) return ONEUI_VPN_WRONG_THREAD;
    if (!cleanupPending_) {
        // An explicit stop acknowledges a retained error after native teardown.
        // Preserve the failure until then, while allowing the user to retry.
        if (state_.state == ONEUI_VPN_ERROR) change(ONEUI_VPN_IDLE);
        return ONEUI_VPN_OK;
    }
    if (state_.state == ONEUI_VPN_STOPPING) return ONEUI_VPN_OK;
    change(ONEUI_VPN_STOPPING);
    stopRequested_ = true;
    host_.stop(host_.context, state_.attempt);
    return ONEUI_VPN_OK;
}
OneUiVpnResult Session::report(uint64_t attempt, int32_t event, int32_t error) {
    if (checkThread() != ONEUI_VPN_OK) return ONEUI_VPN_WRONG_THREAD;
    if (!attempt || attempt != state_.attempt) return ONEUI_VPN_STALE_ATTEMPT;
    if (!cleanupPending_) return ONEUI_VPN_INVALID_STATE;
    switch (event) {
    case ONEUI_VPN_AUTHORIZED:
        if (state_.state != ONEUI_VPN_AUTHORIZING) return ONEUI_VPN_INVALID_STATE;
        change(ONEUI_VPN_ESTABLISHING); break;
    case ONEUI_VPN_TUNNEL_ESTABLISHED:
        if (state_.state != ONEUI_VPN_ESTABLISHING) return ONEUI_VPN_INVALID_STATE;
        change(ONEUI_VPN_TUNNEL_READY); break;
    case ONEUI_VPN_ENGINE_CONNECTED:
        if (state_.state != ONEUI_VPN_TUNNEL_READY) return ONEUI_VPN_INVALID_STATE;
        change(ONEUI_VPN_CONNECTED); break;
    case ONEUI_VPN_AUTHORIZATION_DENIED:
        if (state_.state != ONEUI_VPN_AUTHORIZING) return ONEUI_VPN_INVALID_STATE;
        change(ONEUI_VPN_ERROR, error);
        host_.stop(host_.context, attempt); break;
    case ONEUI_VPN_PERMISSION_REVOKED:
    case ONEUI_VPN_ENGINE_FAILED:
        if (state_.state == ONEUI_VPN_STOPPING) { change(ONEUI_VPN_ERROR, error); break; }
        if (state_.state == ONEUI_VPN_ERROR)
            return ONEUI_VPN_INVALID_STATE;
        change(ONEUI_VPN_ERROR, error);
        host_.stop(host_.context, attempt); break;
    case ONEUI_VPN_STOPPED:
        // Unexpected native teardown is an error, never a successful connection.
        cleanupPending_ = false;
        if (state_.state == ONEUI_VPN_STOPPING || stopRequested_) change(ONEUI_VPN_IDLE);
        else if (state_.state != ONEUI_VPN_ERROR) change(ONEUI_VPN_ERROR, error);
        break;
    default: return ONEUI_VPN_INVALID_ARGUMENT;
    }
    return ONEUI_VPN_OK;
}
OneUiVpnResult Session::snapshot(OneUiVpnSnapshotV1 &out) const {
    if (checkThread() != ONEUI_VPN_OK) return ONEUI_VPN_WRONG_THREAD;
    if (out.struct_size != sizeof(out)) return ONEUI_VPN_INVALID_ARGUMENT;
    out = state_;
    return ONEUI_VPN_OK;
}
} // namespace oneui::vpn
