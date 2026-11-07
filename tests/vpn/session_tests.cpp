#include "oneui/vpn/session.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while (false)

namespace {
struct Host {
    int starts = 0, stops = 0;
    uint64_t attempt = 0;
    std::string json;
    OneUiVpnHostV1 api() {
        return {sizeof(OneUiVpnHostV1), this,
            [](void *p, uint64_t id, const char *c) {
                auto &h = *static_cast<Host *>(p); ++h.starts; h.attempt = id; h.json = c;
            }, [](void *p, uint64_t) { ++static_cast<Host *>(p)->stops; }};
    }
};
struct Config {
    OneUiVpnPrefixV1 ip{4, {100, 80, 0, 2}, 32};
    OneUiVpnPrefixV1 route{4, {100, 80, 0, 3}, 32};
    OneUiVpnConfigV1 value{sizeof(OneUiVpnConfigV1), "VPN \"test\"", 1280, 0,
        &ip, 1, &route, 1, nullptr, 0, nullptr, 0};
};
OneUiVpnSnapshotV1 snapshot(oneui::vpn::Session &s) {
    OneUiVpnSnapshotV1 out{}; out.struct_size = sizeof out;
    CHECK(s.snapshot(out) == ONEUI_VPN_OK); return out;
}
void lifecycle() {
    Host h; Config c; oneui::vpn::Session s(h.api());
    CHECK(s.connect(c.value) == ONEUI_VPN_OK);
    CHECK(h.starts == 1 && h.json.find("\\\"test\\\"") != std::string::npos);
    const auto first = h.attempt;
    CHECK(snapshot(s).state == ONEUI_VPN_AUTHORIZING);
    CHECK(s.connect(c.value) == ONEUI_VPN_INVALID_STATE);
    CHECK(s.report(first, ONEUI_VPN_ENGINE_CONNECTED, 0) == ONEUI_VPN_INVALID_STATE);
    CHECK(s.report(first, ONEUI_VPN_AUTHORIZED, 0) == ONEUI_VPN_OK);
    CHECK(s.report(first, ONEUI_VPN_TUNNEL_ESTABLISHED, 0) == ONEUI_VPN_OK);
    CHECK(snapshot(s).state == ONEUI_VPN_TUNNEL_READY);
    CHECK(s.report(first, ONEUI_VPN_ENGINE_CONNECTED, 0) == ONEUI_VPN_OK);
    CHECK(s.disconnect() == ONEUI_VPN_OK && h.stops == 1);
    CHECK(s.disconnect() == ONEUI_VPN_OK && h.stops == 1);
    CHECK(s.report(first, ONEUI_VPN_ENGINE_CONNECTED, 0) == ONEUI_VPN_INVALID_STATE);
    CHECK(s.destroyable() == ONEUI_VPN_INVALID_STATE);
    CHECK(s.report(first, ONEUI_VPN_STOPPED, 0) == ONEUI_VPN_OK);
    CHECK(snapshot(s).state == ONEUI_VPN_IDLE);
    CHECK(s.connect(c.value) == ONEUI_VPN_OK);
    CHECK(h.attempt > first);
    CHECK(s.report(first, ONEUI_VPN_PERMISSION_REVOKED, 7) == ONEUI_VPN_STALE_ATTEMPT);
    CHECK(s.report(h.attempt, ONEUI_VPN_AUTHORIZATION_DENIED, 9) == ONEUI_VPN_OK);
    CHECK(snapshot(s).state == ONEUI_VPN_ERROR && snapshot(s).error_code == 9);
    CHECK(s.connect(c.value) == ONEUI_VPN_INVALID_STATE); // Native cleanup is not optional.
    CHECK(s.report(h.attempt, ONEUI_VPN_STOPPED, 0) == ONEUI_VPN_OK);
    CHECK(snapshot(s).error_code == 9);
    CHECK(s.destroyable() == ONEUI_VPN_OK);
}
void cancellationAndFailure() {
    Host h; Config c; oneui::vpn::Session s(h.api());
    CHECK(s.connect(c.value) == ONEUI_VPN_OK);
    CHECK(s.disconnect() == ONEUI_VPN_OK);
    CHECK(s.report(h.attempt, ONEUI_VPN_AUTHORIZED, 0) == ONEUI_VPN_INVALID_STATE);
    CHECK(s.report(h.attempt, ONEUI_VPN_TUNNEL_ESTABLISHED, 0) == ONEUI_VPN_INVALID_STATE);
    CHECK(s.report(h.attempt, ONEUI_VPN_STOPPED, 0) == ONEUI_VPN_OK);
    CHECK(s.connect(c.value) == ONEUI_VPN_OK);
    CHECK(s.report(h.attempt, ONEUI_VPN_STOPPED, 88) == ONEUI_VPN_OK);
    CHECK(snapshot(s).state == ONEUI_VPN_ERROR && snapshot(s).error_code == 88);
    CHECK(s.connect(c.value) == ONEUI_VPN_OK);
    CHECK(s.report(h.attempt, ONEUI_VPN_ENGINE_FAILED, 42) == ONEUI_VPN_OK);
    CHECK(s.report(h.attempt, ONEUI_VPN_STOPPED, 0) == ONEUI_VPN_OK);
    CHECK(snapshot(s).error_code == 42);
    CHECK(s.disconnect() == ONEUI_VPN_OK);
    CHECK(snapshot(s).state == ONEUI_VPN_IDLE && snapshot(s).error_code == 0);
    CHECK(s.connect(c.value) == ONEUI_VPN_OK);
    CHECK(s.disconnect() == ONEUI_VPN_OK);
    CHECK(s.report(h.attempt, ONEUI_VPN_STOPPED, 0) == ONEUI_VPN_OK);
    CHECK(s.destroyable() == ONEUI_VPN_OK);
}
void validation() {
    Config c;
    CHECK(oneui_vpn_validate_config_v1(&c.value) == ONEUI_VPN_OK);
    c.route.prefix_length = 0;
    CHECK(oneui_vpn_validate_config_v1(&c.value) == ONEUI_VPN_INVALID_ARGUMENT);
    c.route.address[0] = c.route.address[1] = c.route.address[2] = c.route.address[3] = 0;
    CHECK(oneui_vpn_validate_config_v1(&c.value) == ONEUI_VPN_INVALID_ARGUMENT);
    c.value.allow_default_route = 1;
    CHECK(oneui_vpn_validate_config_v1(&c.value) == ONEUI_VPN_OK);
    c.value.label = "\xed\xa0\x80"; // UTF-8 surrogate.
    CHECK(oneui_vpn_validate_config_v1(&c.value) == ONEUI_VPN_INVALID_ARGUMENT);
    c.value.label = "VPN"; c.value.mtu = 1279;
    CHECK(oneui_vpn_validate_config_v1(&c.value) == ONEUI_VPN_INVALID_ARGUMENT);
    c.value.mtu = 1280; c.ip.address[4] = 1;
    CHECK(oneui_vpn_validate_config_v1(&c.value) == ONEUI_VPN_INVALID_ARGUMENT);
    c.ip.address[4] = 0;
    size_t needed = 0; char shortBuffer[] = "unchanged";
    CHECK(oneui_vpn_config_json_v1(&c.value, shortBuffer, 1, &needed) == ONEUI_VPN_BUFFER_TOO_SMALL);
    CHECK(std::string(shortBuffer) == "unchanged" && needed > 1);
    std::vector<char> buffer(needed);
    CHECK(oneui_vpn_config_json_v1(&c.value, buffer.data(), buffer.size(), &needed) == ONEUI_VPN_OK);
    CHECK(std::string(buffer.data()).find("\"allowDefaultRoute\":true") != std::string::npos);
    c.ip = {6, {0xfd, 0x12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2}, 128};
    c.route = {6, {0xfd, 0x12}, 64};
    CHECK(oneui_vpn_validate_config_v1(&c.value) == ONEUI_VPN_OK);
}
void failedCleanupRetainsOwnership() {
    Host h; Config c; oneui::vpn::Session s(h.api());
    CHECK(s.connect(c.value) == ONEUI_VPN_OK);
    CHECK(s.disconnect() == ONEUI_VPN_OK);
    CHECK(s.report(h.attempt, ONEUI_VPN_ENGINE_FAILED, 2) == ONEUI_VPN_OK);
    CHECK(s.destroyable() == ONEUI_VPN_INVALID_STATE);
    CHECK(s.connect(c.value) == ONEUI_VPN_INVALID_STATE);
    CHECK(s.report(h.attempt, ONEUI_VPN_STOPPED, 0) == ONEUI_VPN_OK);
    OneUiVpnSnapshotV1 state{}; state.struct_size = sizeof(state);
    CHECK(s.snapshot(state) == ONEUI_VPN_OK && state.state == ONEUI_VPN_IDLE);
    CHECK(s.destroyable() == ONEUI_VPN_OK);
}
void threadAndAbi() {
    Host h; Config c; oneui::vpn::Session s(h.api());
    OneUiVpnResult result = ONEUI_VPN_OK;
    std::thread t([&] { result = s.connect(c.value); }); t.join();
    CHECK(result == ONEUI_VPN_WRONG_THREAD && h.starts == 0);
    OneUiVpnSession *raw = nullptr;
    auto host = h.api(); CHECK(oneui_vpn_create_v1(&host, &raw) == ONEUI_VPN_OK);
    CHECK(oneui_vpn_connect_v1(raw, &c.value) == ONEUI_VPN_OK);
    CHECK(oneui_vpn_destroy(raw) == ONEUI_VPN_INVALID_STATE);
    CHECK(oneui_vpn_disconnect(raw) == ONEUI_VPN_OK);
    CHECK(oneui_vpn_report_v1(raw, h.attempt, ONEUI_VPN_STOPPED, 0) == ONEUI_VPN_OK);
    CHECK(oneui_vpn_destroy(raw) == ONEUI_VPN_OK);
}
}
int main() { lifecycle(); cancellationAndFailure(); validation(); failedCleanupRetainsOwnership(); threadAndAbi(); }
