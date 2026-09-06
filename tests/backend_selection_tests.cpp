#include "oneui/oneui_c_api.h"
#include <string_view>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    auto* window = oneui_window_create_utf8(nullptr);
    if (std::string_view(argv[1]) == "failure") {
        if (!window) return 0;
        oneui_window_destroy(window);
        return 2; // Explicit selection must not silently switch to X11.
    }
    if (!window || !oneui_window_initialize_checked(window)) return 3;
    const auto backend = oneui_window_backend(window);
    oneui_window_destroy(window);
    return backend == OneUiWindowBackendX11 ? 0 : 4;
}
