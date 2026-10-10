#include "oneui/oneui_c_api.h"
#include "oneui_c_api_error.h"
#include "oneui_c_api_internal.h"
#include "oneui_c_api_window_internal.h"
#include "oneui_c_api_view_window.h"
using oneui::capi::utf8OrEmpty;
using oneui::capi::wideOrEmpty;

OneUiWindow *oneui_window_create(const OneUiWindowOptions *options) {
    try {
        oneui::WindowOptions nativeOptions;
        nativeOptions.title = options && options->title ? wideOrEmpty(options->title) : L"OneUI";
        nativeOptions.width = options && options->width > 0 ? options->width : 1280;
        nativeOptions.height = options && options->height > 0 ? options->height : 800;
        nativeOptions.visible = options ? options->visible != 0 : false;
        nativeOptions.borderless = options ? options->borderless != 0 : false;
        nativeOptions.fullscreen = options ? options->fullscreen != 0 : false;
        nativeOptions.topmost = options ? options->topmost != 0 : false;
        nativeOptions.resizable = options ? options->resizable != 0 : true;

        auto wrapper = std::make_unique<OneUiWindow>();
        wrapper->window = oneui::Window::create(std::move(nativeOptions));
        if (!wrapper->window) {
            return nullptr;
        }
        return wrapper.release();
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        return nullptr;
    }
}

OneUiWindow *oneui_window_create_utf8(const OneUiWindowOptionsUtf8 *options) {
    try {
        oneui::WindowOptions nativeOptions;
        nativeOptions.title = options ? utf8OrEmpty(options->title) : L"OneUI";
        nativeOptions.width = options && options->width > 0 ? options->width : 1280;
        nativeOptions.height = options && options->height > 0 ? options->height : 800;
        nativeOptions.visible = options ? options->visible != 0 : false;
        nativeOptions.borderless = options ? options->borderless != 0 : false;
        nativeOptions.fullscreen = options ? options->fullscreen != 0 : false;
        nativeOptions.topmost = options ? options->topmost != 0 : false;
        nativeOptions.resizable = options ? options->resizable != 0 : true;

        auto wrapper = std::make_unique<OneUiWindow>();
        wrapper->window = oneui::Window::create(std::move(nativeOptions));
        if (!wrapper->window) {
            return nullptr;
        }
        return wrapper.release();
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        return nullptr;
    }
}

void oneui_window_destroy(OneUiWindow *window) {
    if (oneui::capi::views::closeWindowMounts(window)) delete window;
}

void oneui_window_initialize(OneUiWindow *window) { (void)oneui_window_initialize_checked(window); }

int oneui_window_initialize_checked(OneUiWindow *window) {
    if (!window || !window->window) {
        return 0;
    }
    try {
        window->window->initialize();
        return window->window->nativeHandle() ? 1 : 0;
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        return 0;
    }
}

void oneui_window_show(OneUiWindow *window) {
    if (!window || !window->window) {
        return;
    }
    window->window->show();
}

int oneui_window_center_on_active_monitor(OneUiWindow *window) {
    return window && window->window && window->window->centerOnActiveMonitor() ? 1 : 0;
}

void oneui_window_show_with_fade(OneUiWindow *window, unsigned int duration_ms) {
    if (!window || !window->window) {
        return;
    }
    window->window->showWithFade(duration_ms);
}

int oneui_window_client_area_animations_enabled(OneUiWindow *window) {
    return window && window->window && window->window->clientAreaAnimationsEnabled() ? 1 : 0;
}

void oneui_window_activate(OneUiWindow *window) {
    if (!window || !window->window) {
        return;
    }
    window->window->activate();
}

int oneui_window_run(OneUiWindow *window) {
    if (!window || !window->window) {
        return -1;
    }
    try {
        return window->window->run();
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        return -1;
    }
}

void oneui_window_close(OneUiWindow *window) {
    if (!window || !window->window) {
        return;
    }
    window->window->close();
}

void oneui_window_request_close(OneUiWindow *window) {
    if (!window || !window->window) {
        return;
    }
    window->window->post([window] {
        if (window->window) {
            window->window->requestClose();
        }
    });
}

void oneui_window_minimize(OneUiWindow *window) {
    if (!window || !window->window) {
        return;
    }
    window->window->minimize();
}

void oneui_window_toggle_maximize(OneUiWindow *window) {
    if (!window || !window->window) {
        return;
    }
    window->window->toggleMaximize();
}

int oneui_window_get_placement(OneUiWindow *window, OneUiWindowPlacement *placement) {
    if (!window || !window->window || !placement) {
        return 0;
    }
    oneui::WindowPlacement nativePlacement;
    if (!window->window->getWindowPlacement(nativePlacement)) {
        return 0;
    }
    placement->x = nativePlacement.x;
    placement->y = nativePlacement.y;
    placement->width = nativePlacement.width;
    placement->height = nativePlacement.height;
    placement->maximized = nativePlacement.maximized ? 1 : 0;
    return 1;
}

int oneui_window_set_placement(OneUiWindow *window, const OneUiWindowPlacement *placement) {
    if (!window || !window->window || !placement) {
        return 0;
    }
    return window->window->setWindowPlacement(
               oneui::WindowPlacement{placement->x, placement->y, placement->width,
                                      placement->height, placement->maximized != 0})
               ? 1
               : 0;
}

void oneui_window_set_borderless(OneUiWindow *window, int borderless) {
    if (!window || !window->window) {
        return;
    }
    window->window->setBorderless(borderless != 0);
}

void oneui_window_set_title_bar_drag_metrics(OneUiWindow *window, float title_bar_height,
                                             float reserved_button_width) {
    if (!window || !window->window) {
        return;
    }
    window->window->setTitleBarDragMetrics(title_bar_height, reserved_button_width);
}

void oneui_window_set_title_bar_interactive_insets(OneUiWindow *window, float leading_width,
                                                   float trailing_width) {
    if (!window || !window->window) {
        return;
    }
    window->window->setTitleBarInteractiveInsets(leading_width, trailing_width);
}
