#include "oneui/vpn/json_api.h"
#include <cassert>
#include <string>

int main() {
    int started = 0;
    OneUiVpnHostV1 host{sizeof(host), &started,
        [](void *value, uint64_t, const char *) { ++*static_cast<int *>(value); }, [](void *, uint64_t) {}};
    OneUiVpnSession *session = nullptr;
    assert(oneui_vpn_create_v1(&host, &session) == ONEUI_VPN_OK);
    const std::string valid = R"({"version":1,"label":"VPN","mtu":1280,"allowDefaultRoute":false,"addresses":[{"address":"2001:db8::1","prefix":128}],"routes":[{"address":"2001:db8::1","prefix":128}],"dnsServers":[],"dnsDomains":[]})";
    auto invalid = valid;
    invalid.replace(invalid.find("2001:db8::1"), 11, "not-an-ip");
    assert(oneui_vpn_connect_json_v1(session, invalid.data(), invalid.size()) == ONEUI_VPN_INVALID_ARGUMENT);
    assert(started == 0);
    assert(oneui_vpn_connect_json_v1(session, valid.data(), valid.size()) == ONEUI_VPN_OK);
    assert(started == 1);
    OneUiVpnSnapshotV1 snapshot{}; snapshot.struct_size = sizeof(snapshot);
    assert(oneui_vpn_snapshot_v1(session, &snapshot) == ONEUI_VPN_OK);
    assert(oneui_vpn_disconnect(session) == ONEUI_VPN_OK);
    assert(oneui_vpn_report_v1(session, snapshot.attempt, ONEUI_VPN_STOPPED, 0) == ONEUI_VPN_OK);
    invalid = valid;
    invalid.replace(invalid.find("\"VPN\""), 5, "\"V\\u0000PN\"");
    assert(oneui_vpn_connect_json_v1(session, invalid.data(), invalid.size()) == ONEUI_VPN_INVALID_ARGUMENT);
    assert(started == 1);
    assert(oneui_vpn_destroy(session) == ONEUI_VPN_OK);
}
