#include "oneui/plugins/vpn.h"
#include "oneui_generated_plugins.h"
#include <stdio.h>

/* This consumer checks SDK composition. It does not request OS permission or
 * start a VPN. A real application supplies the platform host callbacks. */
static void start(void *context, uint64_t attempt, const char *config) {
    (void)context; (void)attempt; (void)config;
}
static void stop(void *context, uint64_t attempt) { (void)context; (void)attempt; }
#define CHECK(call) do { if ((call) != 0) { fprintf(stderr, "%s failed\n", #call); return 1; } } while (0)
int main(void) {
    OneUiPluginRuntime *runtime = NULL;
    OneUiPluginLease *lease = NULL;
    OneUiPluginServiceV1 service;
    OneUiVpnSession *session = NULL;
    OneUiVpnHostV1 host = {sizeof(host), NULL, start, stop};
    const OneUiVpnPluginApiV1 *api;
    CHECK(oneui_plugin_runtime_create_v1(&runtime));
    CHECK(register_app_plugins(runtime));
    CHECK(oneui_plugin_runtime_start(runtime));
    CHECK(oneui_plugin_runtime_acquire_v1(runtime, ONEUI_VPN_SERVICE_ID, 1, &lease));
    CHECK(oneui_plugin_lease_service_v1(lease, &service));
    api = (const OneUiVpnPluginApiV1 *)service.api;
    if (api->struct_size != sizeof(*api) || api->abi_version != 1) return 1;
    CHECK(api->create(service.instance, &host, &session));
    CHECK(api->destroy(service.instance, session));
    CHECK(oneui_plugin_lease_release(lease));
    CHECK(oneui_plugin_runtime_stop(runtime));
    CHECK(oneui_plugin_runtime_destroy(runtime));
    puts("Independent VPN plugin consumer passed");
    return 0;
}
