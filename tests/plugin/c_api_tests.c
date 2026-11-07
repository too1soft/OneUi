#include "oneui/plugin/c_api.h"
#include <stdio.h>

typedef struct CounterApiV1 { int (*value)(void *instance); } CounterApiV1;
static int answer(void *instance) { return *(int *)instance; }
static const CounterApiV1 api = {answer};
static int data = 42, cleaned = 0;
static OneUiPluginResult attach(const OneUiPluginContextV1 *ctx, void *user, void **instance) {
    OneUiPluginServiceV1 service = {sizeof(service), 1, &api, user};
    *instance = user;
    return ctx->publish(ctx->token, "example.counter", &service);
}
static OneUiPluginResult detach(void *instance) {
    if (instance != &data) return ONEUI_PLUGIN_FAILED;
    ++cleaned;
    return ONEUI_PLUGIN_OK;
}
#define REQUIRE(test) do { if (!(test)) { fprintf(stderr, "C ABI failure: %s\n", #test); return 1; } } while (0)
int main(void) {
    OneUiPluginRuntime *r = NULL;
    OneUiPluginLease *lease = NULL;
    OneUiPluginServiceV1 service;
    OneUiPluginDescriptorV1 d = {
        sizeof(d), ONEUI_PLUGIN_ABI_V1, "example.counter", {1, 0, 0},
        NULL, 0, &data, attach, detach
    };
    REQUIRE(oneui_plugin_abi_version() == ONEUI_PLUGIN_ABI_V1);
    REQUIRE(oneui_plugin_runtime_create_v1(&r) == ONEUI_PLUGIN_OK);
    REQUIRE(oneui_plugin_runtime_register_v1(r, &d) == ONEUI_PLUGIN_OK);
    REQUIRE(oneui_plugin_runtime_start(r) == ONEUI_PLUGIN_OK);
    REQUIRE(oneui_plugin_runtime_acquire_v1(r, "example.counter", 1, &lease) == ONEUI_PLUGIN_OK);
    REQUIRE(oneui_plugin_lease_service_v1(lease, &service) == ONEUI_PLUGIN_OK);
    REQUIRE(((const CounterApiV1 *)service.api)->value(service.instance) == 42);
    REQUIRE(oneui_plugin_lease_release(lease) == ONEUI_PLUGIN_OK);
    REQUIRE(oneui_plugin_runtime_stop(r) == ONEUI_PLUGIN_OK);
    REQUIRE(cleaned == 1);
    REQUIRE(oneui_plugin_runtime_destroy(r) == ONEUI_PLUGIN_OK);
    return 0;
}
