#ifndef ONEUI_VPN_GO_NATIVE_BRIDGE_H
#define ONEUI_VPN_GO_NATIVE_BRIDGE_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/* Synchronous callbacks may run on engine threads. Return zero only after the
 * native operation succeeds. Never call host_close reentrantly from a callback.
 * context must remain alive until a successful host_close. Set struct_size to
 * sizeof(OneUiVpnNativeCallbacks) and abi_version to 1. host_create copies the
 * descriptor; it does not retain the pointer passed by the caller.
 * Android requires read_network_state (the version-1 interface JSON schema)
 * because modern app sandboxes prohibit Go netlink interface enumeration. */
typedef struct OneUiVpnNativeCallbacks {
    uint32_t struct_size;
    uint32_t abi_version;
    uintptr_t context;
    int (*apply_network)(uintptr_t context, const char *json, int length);
    int (*protect_socket)(uintptr_t context, int descriptor);
    /* Secure OS storage, maximum blob size 1 MiB. Read returns byte count,
     * zero for absent, negative on failure. Write returns zero on success.
     * Native implementation must encrypt/authenticate and atomically replace.
     * No raw bytes or keys may be retained beyond the callback. */
    int (*secure_read)(uintptr_t context, const char *key, void *buffer, int capacity);
    int (*secure_write)(uintptr_t context, const char *key, const void *bytes, int length);
    int (*read_network_state)(uintptr_t context, void *buffer, int capacity);
} OneUiVpnNativeCallbacks;

/* Host handles are opaque, monotonically allocated and single generation.
 * Flags: bit 0 default route opt-in; bit 1 actual native split-DNS support.
 * All packet functions copy bytes. A positive receive result is the byte count.
 * Errors: -1 closed/unknown, -2 argument, -3 timeout, -4 native operation failed.
 * Calls with timeout_ms require 1..30000; receive capacity must be >=65535.
 * Product library must import the native Go package when building c-shared or
 * c-archive. Link this header without exposing generated Go/runtime types. */
uint64_t oneui_vpn_go_host_create(char *label, int mtu, int flags, OneUiVpnNativeCallbacks *callbacks);
uint32_t oneui_vpn_go_abi_version(void);
int oneui_vpn_go_packet_inject(uint64_t host, void *packet, int length, int timeout_ms);
int oneui_vpn_go_packet_receive(uint64_t host, void *buffer, int capacity, int timeout_ms);
int oneui_vpn_go_host_close(uint64_t host);
int oneui_vpn_go_network_changed(uint64_t host);

int oneui_vpn_call_apply(OneUiVpnNativeCallbacks callbacks, const char *json, int length);
int oneui_vpn_call_protect(OneUiVpnNativeCallbacks callbacks, int descriptor);
int oneui_vpn_call_secure_read(OneUiVpnNativeCallbacks callbacks, const char *key, void *buffer, int capacity);
int oneui_vpn_call_secure_write(OneUiVpnNativeCallbacks callbacks, const char *key, const void *bytes, int length);
int oneui_vpn_call_network_state(OneUiVpnNativeCallbacks callbacks, void *buffer, int capacity);
#ifdef __cplusplus
}
#endif
#endif
