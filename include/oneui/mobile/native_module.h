#ifndef ONEUI_MOBILE_NATIVE_MODULE_H
#define ONEUI_MOBILE_NATIVE_MODULE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Opaque application command ABI. Execute once on a worker, never on the UI
 * thread. Request/response are owned by the caller for this call only. Return
 * response byte count (0..capacity) or a negative transport error. Command IDs,
 * payload schema and business errors belong to the application. Maximum request
 * and response are 1 MiB. A handler must not retain these buffer pointers. */
typedef int32_t (*OneUiNativeModuleCallV1)(int32_t command, void *request,
    int32_t request_length, void *response, int32_t response_capacity);
#ifdef __cplusplus
}
#endif
#endif
