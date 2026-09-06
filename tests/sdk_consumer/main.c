#include "oneui/oneui_c_api.h"
static void close_window(void* window) { oneui_window_close((OneUiWindow*)window); }
int main(void) {
    if (oneui_utf8_abi_version() != ONEUI_UTF8_ABI_VERSION) return 1;
    OneUiWindow* window = oneui_window_create_utf8(0);
    if (!window || !oneui_window_initialize_checked(window)) return 2;
    if (oneui_window_backend(window) == OneUiWindowBackendUnknown) return 3;
    oneui_window_show(window);
    oneui_window_post(window, close_window, window);
    int result = oneui_window_run(window);
    oneui_window_destroy(window);
    return result;
}
