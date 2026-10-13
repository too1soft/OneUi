#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui_c_api_window_internal.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <cwchar>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace oneui::capi;

struct OneUiOwnedCallback {
    OneUiVoidCallback callback = nullptr;
    OneUiDestroyCallback cleanup = nullptr;
    void* userData = nullptr;
    bool invoked = false;

    ~OneUiOwnedCallback() {
        if (!invoked && cleanup) {
            cleanup(userData);
        }
    }

    void run() {
        invoked = true;
        if (callback) {
            callback(userData);
        }
    }
};

void oneui_window_set_on_client_size_changed(
    OneUiWindow* window,
    OneUiClientSizeChangedCallback callback,
    void* user_data) {
    if (!window || !window->window) {
        return;
    }
    if (!callback) {
        window->window->setClientSizeChangedHandler({});
        return;
    }
    window->window->setClientSizeChangedHandler(
        [callback, user_data](oneui::Size size) {
            callback(size.width, size.height, user_data);
        });
}

void oneui_window_set_on_activation_changed(
    OneUiWindow* window,
    OneUiWindowActivationChangedCallback callback,
    void* user_data) {
    if (!window || !window->window) {
        return;
    }
    if (!callback) {
        window->window->setActivationChangedHandler({});
        return;
    }
    window->window->setActivationChangedHandler(
        [callback, user_data](bool active) {
            callback(active ? 1 : 0, user_data);
        });
}

void oneui_window_set_on_file_drop(
    OneUiWindow* window,
    OneUiFileDropCallback callback,
    void* user_data) {
    if (!window || !window->window) {
        return;
    }
    if (!callback) {
        window->window->setFileDropHandler({});
        return;
    }
    window->window->setFileDropHandler(
        [callback, user_data](std::vector<std::wstring> paths, oneui::Point point) {
            std::vector<std::string> encoded;
            encoded.reserve(paths.size());
            for (const auto& path : paths) {
                encoded.push_back(utf8FromWide(path));
            }
            std::vector<OneUiUtf8String> views;
            views.reserve(encoded.size());
            for (const auto& path : encoded) {
                views.push_back(OneUiUtf8String{path.data(), path.size()});
            }
            callback(views.data(), views.size(), point.x, point.y, user_data);
        });
}

void oneui_window_set_corner_radius(OneUiWindow* window, float radius) {
    if (!window || !window->window) {
        return;
    }
    window->window->setCornerRadius(radius);
}

void oneui_window_post(OneUiWindow* window, OneUiVoidCallback callback, void* user_data) {
    (void)oneui_window_post_owned(window, callback, user_data, nullptr);
}

int oneui_window_post_owned(
    OneUiWindow* window,
    OneUiVoidCallback callback,
    void* user_data,
    OneUiDestroyCallback cleanup) {
    if (!window || !window->window || !callback) {
        if (cleanup) {
            cleanup(user_data);
        }
        return 0;
    }
    auto ownedCallback = std::make_shared<OneUiOwnedCallback>();
    ownedCallback->callback = callback;
    ownedCallback->cleanup = cleanup;
    ownedCallback->userData = user_data;
    return window->window->post([ownedCallback] {
        ownedCallback->run();
    }) ? 1 : 0;
}

void oneui_window_request_animation_frame(OneUiWindow* window, OneUiFrameCallback callback, void* user_data) {
    if (!window || !window->window || !callback) {
        return;
    }
    window->window->requestAnimationFrame([callback, user_data](double nowMs) {
        callback(nowMs, user_data);
    });
}

void oneui_window_set_minimum_client_size(OneUiWindow* window, float width, float height) {
    if (!window || !window->window) {
        return;
    }
    window->window->setMinimumClientSize(oneui::Size{
        std::max(0.0f, width),
        std::max(0.0f, height)});
}

void oneui_window_set_fullscreen(OneUiWindow* window, int fullscreen) {
    if (!window || !window->window) {
        return;
    }
    window->window->setFullscreen(fullscreen != 0);
}

int oneui_window_is_fullscreen(OneUiWindow* window) {
    if (!window || !window->window) {
        return 0;
    }
    return window->window->isFullscreen() ? 1 : 0;
}

void oneui_window_set_title(OneUiWindow* window, const wchar_t* title) {
    if (!window || !window->window) {
        return;
    }
    window->window->setTitle(title ? std::wstring(title) : std::wstring());
}

void oneui_window_set_title_utf8(OneUiWindow* window, OneUiUtf8String title) {
    if (!window || !window->window) {
        return;
    }
    window->window->setTitle(utf8OrEmpty(title));
}

void* oneui_window_native_handle(OneUiWindow* window) {
    if (!window || !window->window) {
        return nullptr;
    }
    return window->window->nativeHandle();
}

int oneui_window_client_width(OneUiWindow* window) {
    if (!window || !window->window) {
        return 0;
    }
    return static_cast<int>(window->window->clientSize().width);
}

int oneui_window_client_height(OneUiWindow* window) {
    if (!window || !window->window) {
        return 0;
    }
    return static_cast<int>(window->window->clientSize().height);
}

int oneui_window_client_pixel_width(OneUiWindow* window) {
    if (!window || !window->window) {
        return 0;
    }
    return static_cast<int>(window->window->clientPixelSize().width);
}

int oneui_window_client_pixel_height(OneUiWindow* window) {
    if (!window || !window->window) {
        return 0;
    }
    return static_cast<int>(window->window->clientPixelSize().height);
}

float oneui_window_dpi_scale(OneUiWindow* window) {
    if (!window || !window->window) {
        return 1.0f;
    }
    return window->window->dpiScale();
}

void oneui_window_set_content_scale(OneUiWindow* window, float scale) {
    if (window && window->window) window->window->setContentScale(scale);
}

unsigned int oneui_window_backend(OneUiWindow* window) {
    return window && window->window ? static_cast<unsigned int>(window->window->backend()) : 0;
}

unsigned int oneui_window_capabilities(OneUiWindow* window) {
    return window && window->window ? window->window->capabilities() : 0;
}

void oneui_window_set_content(OneUiWindow* window, OneUiWidget* widget) {
    if (!window || !window->window || !widget || !widget->widget) {
        return;
    }
    if (window->styleSheet) {
        applyStyleSheet(widget, window->styleSheet);
    }
    window->rootContent = widget->widget;
    window->window->setContent(widget->widget);
}

int oneui_window_request_focus(OneUiWindow* window, OneUiWidget* widget, int focus_visible) {
    if (!window || !window->window || !widget || !widget->widget) {
        return 0;
    }
    return window->window->requestFocus(widget->widget.get(), focus_visible != 0) ? 1 : 0;
}

void oneui_window_set_on_raw_key(
    OneUiWindow* window,
    OneUiWindowRawKeyCallback callback,
    void* user_data) {
    if (!window || !window->window) {
        return;
    }
    if (!callback) {
        window->window->setRawKeyHandler({});
        return;
    }
    window->window->setRawKeyHandler([callback, user_data](const oneui::KeyEvent& event) {
        const OneUiRawKeyEvent native{
            event.virtualKey,
            event.scanCode,
            event.pressed ? 1 : 0,
            event.repeat ? 1 : 0,
            event.extended ? 1 : 0,
            event.alt ? 1 : 0,
            event.control ? 1 : 0,
            event.shift ? 1 : 0,
            event.win ? 1 : 0,
        };
        return callback(&native, user_data) != 0;
    });
}

void oneui_window_set_default_font_family_utf8(
    OneUiWindow* window,
    OneUiUtf8String family) {
    if (!window || !window->window) {
        return;
    }
    window->window->setDefaultFontFamily(utf8OrEmpty(family));
}
