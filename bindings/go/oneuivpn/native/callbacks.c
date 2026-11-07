#include "native_bridge.h"
int oneui_vpn_call_apply(OneUiVpnNativeCallbacks c, const char *json, int length) {
    return c.apply_network ? c.apply_network(c.context, json, length) : -1;
}
int oneui_vpn_call_protect(OneUiVpnNativeCallbacks c, int descriptor) {
    return c.protect_socket ? c.protect_socket(c.context, descriptor) : -1;
}
int oneui_vpn_call_secure_read(OneUiVpnNativeCallbacks c, const char *key, void *buffer, int capacity) {
    return c.secure_read ? c.secure_read(c.context, key, buffer, capacity) : -1;
}
int oneui_vpn_call_secure_write(OneUiVpnNativeCallbacks c, const char *key, const void *bytes, int length) {
    return c.secure_write ? c.secure_write(c.context, key, bytes, length) : -1;
}
int oneui_vpn_call_network_state(OneUiVpnNativeCallbacks c, void *buffer, int capacity) {
    return c.read_network_state ? c.read_network_state(c.context, buffer, capacity) : -1;
}
