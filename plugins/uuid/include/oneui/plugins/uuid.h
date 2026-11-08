#pragma once
#include "oneui/plugin/c_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ONEUI_UUID_SERVICE_V1 "oneui.uuid.generator"
#define ONEUI_UUID_BUFFER_SIZE 37u

typedef struct OneUiUuidApiV1 {
    uint32_t struct_size;
    uint32_t abi_version;
    /* Creating runtime's thread only. Writes a lowercase UUIDv4 and NUL.
     * capacity includes NUL and must be >= 37. No allocation is transferred.
     * Errors clear out[0] whenever out != NULL and capacity != 0.
     * LIMIT means insufficient capacity; FAILED means system RNG failure.
     * instance and this table are valid only while the service lease exists. */
    OneUiPluginResult (*generate)(void *instance, char *out, size_t capacity);
} OneUiUuidApiV1;

const OneUiPluginDescriptorV1 *oneui_uuid_plugin_descriptor_v1(void);

#ifdef __cplusplus
}
#endif
