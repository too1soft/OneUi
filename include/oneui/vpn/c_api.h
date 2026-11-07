#pragma once
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && !defined(ONEUI_VPN_STATIC)
#if defined(ONEUI_VPN_BUILD)
#define ONEUI_VPN_API __declspec(dllexport)
#else
#define ONEUI_VPN_API __declspec(dllimport)
#endif
#else
#define ONEUI_VPN_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OneUiVpnSession OneUiVpnSession;
typedef enum OneUiVpnResult {
    ONEUI_VPN_OK = 0,
    ONEUI_VPN_INVALID_ARGUMENT = 1,
    ONEUI_VPN_INVALID_STATE = 2,
    ONEUI_VPN_WRONG_THREAD = 3,
    ONEUI_VPN_STALE_ATTEMPT = 4,
    ONEUI_VPN_BUFFER_TOO_SMALL = 5,
    ONEUI_VPN_FAILED = 6
} OneUiVpnResult;
typedef enum OneUiVpnState {
    ONEUI_VPN_IDLE = 0,
    ONEUI_VPN_AUTHORIZING = 1,
    ONEUI_VPN_ESTABLISHING = 2,
    ONEUI_VPN_TUNNEL_READY = 3,
    ONEUI_VPN_CONNECTED = 4,
    ONEUI_VPN_STOPPING = 5,
    ONEUI_VPN_ERROR = 6
} OneUiVpnState;
typedef enum OneUiVpnEvent {
    ONEUI_VPN_AUTHORIZED = 1,
    ONEUI_VPN_AUTHORIZATION_DENIED = 2,
    ONEUI_VPN_TUNNEL_ESTABLISHED = 3,
    ONEUI_VPN_ENGINE_CONNECTED = 4,
    ONEUI_VPN_ENGINE_FAILED = 5,
    ONEUI_VPN_STOPPED = 6,
    ONEUI_VPN_PERMISSION_REVOKED = 7
} OneUiVpnEvent;

/* IP bytes are in network order. IPv4 uses the first four bytes. No textual
 * address parser, renderer or product-specific network protocol is required. */
typedef struct OneUiVpnPrefixV1 {
    uint32_t family; /* 4 or 6 */
    uint8_t address[16];
    uint32_t prefix_length;
} OneUiVpnPrefixV1;
typedef struct OneUiVpnConfigV1 {
    uint32_t struct_size;
    const char *label; /* UTF-8, 1..128 bytes */
    uint32_t mtu;      /* 1280..9000 */
    uint32_t allow_default_route; /* Explicit opt-in, 0 or 1. */
    const OneUiVpnPrefixV1 *addresses;
    size_t address_count; /* 1..16 */
    const OneUiVpnPrefixV1 *routes;
    size_t route_count;   /* 1..256 */
    const OneUiVpnPrefixV1 *dns_servers; /* /32 or /128 */
    size_t dns_server_count; /* 0..8 */
    const char *const *dns_domains; /* ASCII domain suffixes, 0..16 */
    size_t dns_domain_count;
} OneUiVpnConfigV1;

/* Callbacks run on the creating thread and must return promptly. start copies
 * config_json before returning and reports actual OS/engine events later.
 * stop must release resources even while authorization/start is pending.
 * Platform callbacks must dispatch back to the owner thread. Neither a start
 * request nor a TUN descriptor means that encrypted connectivity is ready.
 * Exactly one system tunnel belongs to a session; multiplex logical networks
 * above the packet engine rather than creating competing system VPNs. */
typedef struct OneUiVpnHostV1 {
    uint32_t struct_size;
    void *context;
    void (*start)(void *context, uint64_t attempt, const char *config_json);
    void (*stop)(void *context, uint64_t attempt);
} OneUiVpnHostV1;
typedef struct OneUiVpnSnapshotV1 {
    uint32_t struct_size;
    int32_t state;
    int32_t error_code; /* Platform code only; never credentials or raw logs. */
    uint64_t attempt;
    uint64_t revision;
} OneUiVpnSnapshotV1;

ONEUI_VPN_API OneUiVpnResult oneui_vpn_validate_config_v1(const OneUiVpnConfigV1 *);
/* Includes trailing NUL in required_size. Does not write a partial JSON object. */
ONEUI_VPN_API OneUiVpnResult oneui_vpn_config_json_v1(
    const OneUiVpnConfigV1 *, char *buffer, size_t capacity, size_t *required_size);
ONEUI_VPN_API OneUiVpnResult oneui_vpn_create_v1(const OneUiVpnHostV1 *, OneUiVpnSession **);
/* Destroy only after IDLE/ERROR and confirmed OS cleanup. */
ONEUI_VPN_API OneUiVpnResult oneui_vpn_destroy(OneUiVpnSession *);
ONEUI_VPN_API OneUiVpnResult oneui_vpn_connect_v1(OneUiVpnSession *, const OneUiVpnConfigV1 *);
ONEUI_VPN_API OneUiVpnResult oneui_vpn_disconnect(OneUiVpnSession *);
ONEUI_VPN_API OneUiVpnResult oneui_vpn_report_v1(
    OneUiVpnSession *, uint64_t attempt, int32_t event, int32_t error_code);
ONEUI_VPN_API OneUiVpnResult oneui_vpn_snapshot_v1(OneUiVpnSession *, OneUiVpnSnapshotV1 *);
#ifdef __cplusplus
}
#endif
