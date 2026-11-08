#include "oneui/plugins/uuid.h"
#include "oneui_generated_plugins.h"
#include <stdio.h>

#define REQUIRE(call) do { if ((call) != ONEUI_PLUGIN_OK) return 1; } while (0)
int main(void) {
    OneUiPluginRuntime *runtime = NULL;
    OneUiPluginLease *lease = NULL;
    OneUiPluginServiceV1 service;
    char value[ONEUI_UUID_BUFFER_SIZE];
    REQUIRE(oneui_plugin_runtime_create_v1(&runtime));
    REQUIRE(register_uuid_plugins(runtime));
    REQUIRE(oneui_plugin_runtime_start(runtime));
    REQUIRE(oneui_plugin_runtime_acquire_v1(runtime, ONEUI_UUID_SERVICE_V1, 1, &lease));
    REQUIRE(oneui_plugin_lease_service_v1(lease, &service));
    {
        const OneUiUuidApiV1 *api = (const OneUiUuidApiV1 *)service.api;
        if (!api || api->struct_size < sizeof(*api) || api->abi_version != 1) return 1;
        REQUIRE(api->generate(service.instance, value, sizeof(value)));
    }
    printf("UUIDv4: %s\n", value);
    REQUIRE(oneui_plugin_lease_release(lease));
    REQUIRE(oneui_plugin_runtime_stop(runtime));
    REQUIRE(oneui_plugin_runtime_destroy(runtime));
    return 0;
}
