#pragma once
#include "oneui/export.h"
#include <stddef.h>

// Optional macOS application service. Calls and callbacks belong to the main
// thread. Bytes are borrowed only for the callback; pass null to unsubscribe.
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*OneUiOpenUriCallback)(const char* bytes, size_t length, void* user_data);
#if defined(__APPLE__)
ONEUI_API int oneui_macos_set_open_uri_callback(OneUiOpenUriCallback callback, void* user_data);
#endif
#ifdef __cplusplus
}
#endif
