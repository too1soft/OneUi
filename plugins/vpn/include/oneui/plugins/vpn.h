#pragma once
#include "oneui/plugin/c_api.h"
#include "oneui/vpn/c_api.h"

#ifdef __cplusplus
extern "C" {
#endif
#define ONEUI_VPN_PLUGIN_ID "oneui.vpn"
#define ONEUI_VPN_SERVICE_ID "oneui.vpn.sessions"
/* Hold the service lease until every created session has been destroyed.
 * Native callbacks and packet engines use the original VPN contract. */
typedef struct OneUiVpnPluginApiV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    OneUiVpnResult (*create)(void *plugin, const OneUiVpnHostV1 *, OneUiVpnSession **);
    OneUiVpnResult (*destroy)(void *plugin, OneUiVpnSession *);
    OneUiVpnResult (*connect)(OneUiVpnSession *, const OneUiVpnConfigV1 *);
    OneUiVpnResult (*disconnect)(OneUiVpnSession *);
    OneUiVpnResult (*report)(OneUiVpnSession *, uint64_t, int32_t, int32_t);
    OneUiVpnResult (*snapshot)(OneUiVpnSession *, OneUiVpnSnapshotV1 *);
} OneUiVpnPluginApiV1;

/* Resident, immutable descriptor; registration copies it. This package is
 * statically linked into the application and works with shared or static SDKs. */
const OneUiPluginDescriptorV1 *oneui_vpn_plugin_descriptor_v1(void);
#ifdef __cplusplus
}
#endif
