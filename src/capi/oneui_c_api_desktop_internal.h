#pragma once
#include "../internal/unicode.h"
#include "oneui_c_api_error.h"
#include "oneui_c_api_window_internal.h"
#include <stdexcept>

namespace oneui::capi {
struct DesktopCallbackData {
    void *data;
    OneUiDestroyCallback destroy;
    ~DesktopCallbackData() {
        if (destroy)
            destroy(data);
    }
};
inline std::shared_ptr<DesktopCallbackData> ownDesktopCallback(void *data,
                                                               OneUiDestroyCallback cleanup) {
    try {
        auto value = std::make_shared<DesktopCallbackData>();
        value->data = data;
        value->destroy = cleanup;
        return value;
    } catch (...) {
        if (cleanup)
            cleanup(data);
        throw;
    }
}
inline std::string desktopString(OneUiUtf8String value) {
    if (!value.data && value.length)
        throw std::invalid_argument("Null UTF-8 data");
    std::string result(value.data ? value.data : "", value.length);
    if (result.find('\0') != std::string::npos ||
        unicode::toUtf8(unicode::fromUtf8(result)) != result)
        throw std::invalid_argument("Invalid UTF-8");
    return result;
}
inline Window &desktopWindow(OneUiWindow *window) {
    if (!window || !window->window)
        throw std::invalid_argument("Window is required");
    return *window->window;
}
template <class F> int desktopCall(const char *operation, F action) noexcept {
    try {
        return action() ? 1 : 0;
    } catch (...) {
        recordCurrentException(operation);
        return 0;
    }
}
} // namespace oneui::capi
