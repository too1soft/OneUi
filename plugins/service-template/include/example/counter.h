#pragma once
#include "oneui/plugin/c_api.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ExampleCounterApiV1 {
    uint32_t struct_size, abi_version;
    OneUiPluginResult (*next)(void *instance, uint64_t *out);
} ExampleCounterApiV1;
const OneUiPluginDescriptorV1 *example_counter_descriptor_v1(void);
#ifdef __cplusplus
}
#endif
