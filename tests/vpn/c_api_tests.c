#include "oneui/vpn/c_api.h"
#include <string.h>
static void start(void *context, uint64_t attempt, const char *json) { (void)context; (void)attempt; (void)json; }
static void stop(void *context, uint64_t attempt) { (void)context; (void)attempt; }
int main(void) {
    OneUiVpnSession *session = 0;
    OneUiVpnHostV1 host = {sizeof(host), 0, start, stop};
    OneUiVpnSnapshotV1 state;
    memset(&state, 0, sizeof(state)); state.struct_size = sizeof(state);
    if (oneui_vpn_create_v1(&host, &session) != ONEUI_VPN_OK) return 1;
    if (oneui_vpn_snapshot_v1(session, &state) != ONEUI_VPN_OK || state.state != ONEUI_VPN_IDLE) return 2;
    if (oneui_vpn_destroy(session) != ONEUI_VPN_OK) return 3;
    host.struct_size = 0; session = (OneUiVpnSession *)(uintptr_t)1;
    if (oneui_vpn_create_v1(&host, &session) != ONEUI_VPN_INVALID_ARGUMENT || session != 0) return 4;
    return 0;
}
