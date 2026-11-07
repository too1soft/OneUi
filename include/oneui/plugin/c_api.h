#pragma once
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && !defined(ONEUI_PLUGIN_STATIC)
#if defined(ONEUI_PLUGIN_BUILD)
#define ONEUI_PLUGIN_API __declspec(dllexport)
#else
#define ONEUI_PLUGIN_API __declspec(dllimport)
#endif
#else
#define ONEUI_PLUGIN_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ONEUI_PLUGIN_ABI_V1 1u
typedef struct OneUiPluginRuntime OneUiPluginRuntime;
typedef struct OneUiPluginLease OneUiPluginLease;
typedef enum OneUiPluginResult {
    ONEUI_PLUGIN_OK = 0,
    ONEUI_PLUGIN_INVALID_ARGUMENT = 1,
    ONEUI_PLUGIN_WRONG_THREAD = 2,
    ONEUI_PLUGIN_INVALID_STATE = 3,
    ONEUI_PLUGIN_DUPLICATE = 4,
    ONEUI_PLUGIN_NOT_FOUND = 5,
    ONEUI_PLUGIN_INCOMPATIBLE = 6,
    ONEUI_PLUGIN_DEPENDENCY_CYCLE = 7,
    ONEUI_PLUGIN_BUSY = 8,
    ONEUI_PLUGIN_REENTRANT = 9,
    ONEUI_PLUGIN_FAILED = 10,
    ONEUI_PLUGIN_LIMIT = 11
} OneUiPluginResult;

typedef enum OneUiPluginState {
    ONEUI_PLUGIN_REGISTERING = 0,
    ONEUI_PLUGIN_STARTING = 1,
    ONEUI_PLUGIN_ACTIVE = 2,
    ONEUI_PLUGIN_STOPPING = 3,
    ONEUI_PLUGIN_STOPPED = 4
} OneUiPluginState;

typedef struct OneUiPluginVersionV1 {
    uint32_t major, minor, patch;
} OneUiPluginVersionV1;
typedef struct OneUiPluginDependencyV1 {
    const char *id;
    OneUiPluginVersionV1 minimum;
    /* Exclusive upper bound; all zero means no upper bound. */
    OneUiPluginVersionV1 before;
} OneUiPluginDependencyV1;

/* Service-specific headers define a versioned C function table. instance and
 * api are borrowed until the consumer releases its lease. No STL, exception,
 * language-owned string or compiler-specific class crosses this boundary. */
typedef struct OneUiPluginServiceV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    const void *api;
    void *instance;
} OneUiPluginServiceV1;

typedef struct OneUiPluginContextV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    void *token;
    /* Only callable synchronously inside this plugin's attach callback. */
    OneUiPluginResult (*publish)(void *token, const char *service_id,
                                const OneUiPluginServiceV1 *service);
    /* Only this plugin's declared dependencies are visible. Borrowed services
     * outlive this plugin's successful detach; all calls use the owner thread. */
    OneUiPluginResult (*dependency)(void *token, const char *service_id,
                                   uint32_t abi_version, OneUiPluginServiceV1 *out);
} OneUiPluginContextV1;

typedef struct OneUiPluginDescriptorV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    const char *id; /* 1..128 ASCII bytes: lowercase letters, digits, . _ - */
    OneUiPluginVersionV1 version;
    const OneUiPluginDependencyV1 *dependencies; /* at most 64 */
    size_t dependency_count;
    void *user_data;
    /* Called in dependency order. Set instance before acquiring resources.
     * detach is called for EVERY attempted attach, including a failed attach
     * with a NULL instance. user_data and callback code must remain resident. */
    OneUiPluginResult (*attach)(const OneUiPluginContextV1 *context,
                               void *user_data, void **instance);
    /* Called in reverse order. OK means all resources/callbacks are gone and
     * instance may be freed. On failure retain ownership; stop can be retried.
     * Must handle partial initialization, return promptly, and never throw. */
    OneUiPluginResult (*detach)(void *instance);
} OneUiPluginDescriptorV1;

ONEUI_PLUGIN_API uint32_t oneui_plugin_abi_version(void);
/* One runtime per application/engine, owned by its creating thread. It has no
 * renderer dependency or process-global registry. NULL output is never valid. */
ONEUI_PLUGIN_API OneUiPluginResult oneui_plugin_runtime_create_v1(OneUiPluginRuntime **out);
/* Copies descriptor strings/dependencies. Registration closes at start.
 * At most 256 plugins, 4096 service interfaces and 65536 leases per runtime. */
ONEUI_PLUGIN_API OneUiPluginResult oneui_plugin_runtime_register_v1(
    OneUiPluginRuntime *runtime, const OneUiPluginDescriptorV1 *descriptor);
/* Validates the complete dependency graph BEFORE invoking any plugin code.
 * Start is one-shot. Failure rolls back attempted plugins; failed cleanup
 * leaves STOPPING and retains the dependency chain for an explicit stop retry. */
ONEUI_PLUGIN_API OneUiPluginResult oneui_plugin_runtime_start(OneUiPluginRuntime *runtime);
/* BUSY while any consumer lease exists. Failed detach leaves its dependencies
 * attached. Repeated stop after successful cleanup is safe. */
ONEUI_PLUGIN_API OneUiPluginResult oneui_plugin_runtime_stop(OneUiPluginRuntime *runtime);
/* Refuses active/partially attached runtimes and outstanding leases. */
ONEUI_PLUGIN_API OneUiPluginResult oneui_plugin_runtime_destroy(OneUiPluginRuntime *runtime);
ONEUI_PLUGIN_API OneUiPluginResult oneui_plugin_runtime_state(
    OneUiPluginRuntime *runtime, OneUiPluginState *out);
/* Returns an owner-thread borrowed diagnostic, valid until the next mutation.
 * NULL runtime or wrong thread returns a fixed diagnostic. Never log payloads. */
ONEUI_PLUGIN_API const char *oneui_plugin_runtime_error(OneUiPluginRuntime *runtime);
ONEUI_PLUGIN_API OneUiPluginResult oneui_plugin_runtime_acquire_v1(
    OneUiPluginRuntime *runtime, const char *service_id, uint32_t abi_version,
    OneUiPluginLease **out);
ONEUI_PLUGIN_API OneUiPluginResult oneui_plugin_lease_service_v1(
    OneUiPluginLease *lease, OneUiPluginServiceV1 *out);
/* The caller must stop all use of the service before releasing its lease. */
ONEUI_PLUGIN_API OneUiPluginResult oneui_plugin_lease_release(OneUiPluginLease *lease);

#ifdef __cplusplus
}
#endif
