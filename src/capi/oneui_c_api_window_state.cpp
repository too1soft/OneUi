#include "oneui/modal_session.h"
#include "oneui_c_api_desktop_internal.h"
using namespace oneui::capi;
struct OneUiWindowSubscription {
    oneui::Window::EventSubscription value;
};
struct OneUiModalSession {
    std::unique_ptr<oneui::ModalSession> value;
};
namespace {
OneUiWindowState snapshot(const oneui::WindowState &value) {
    return {value.knownFields,
            (value.visible ? 1u : 0u) | (value.active ? 2u : 0u) | (value.minimized ? 4u : 0u) |
                (value.maximized ? 8u : 0u) | (value.fullscreen ? 16u : 0u) |
                (value.topmost ? 32u : 0u) | (value.resizable ? 64u : 0u),
            value.clientSize.width,
            value.clientSize.height,
            value.pixelSize.width,
            value.pixelSize.height,
            value.dpiScale};
}
} // namespace
int oneui_window_get_state(OneUiWindow *window, OneUiWindowState *state) {
    return desktopCall(__func__, [&] {
        if (!state)
            return false;
        *state = snapshot(desktopWindow(window).state());
        return true;
    });
}
int oneui_window_hide(OneUiWindow *w) {
    return desktopCall(__func__, [&] { return desktopWindow(w).hide(); });
}
int oneui_window_restore(OneUiWindow *w) {
    return desktopCall(__func__, [&] { return desktopWindow(w).restore(); });
}
int oneui_window_maximize(OneUiWindow *w) {
    return desktopCall(__func__, [&] { return desktopWindow(w).maximize(); });
}
int oneui_window_set_topmost(OneUiWindow *w, int v) {
    return desktopCall(__func__, [&] { return desktopWindow(w).setTopmost(v != 0); });
}
int oneui_window_set_resizable(OneUiWindow *w, int v) {
    return desktopCall(__func__, [&] { return desktopWindow(w).setResizable(v != 0); });
}
int oneui_window_set_client_size(OneUiWindow *w, float x, float y) {
    return desktopCall(__func__, [&] { return desktopWindow(w).setClientSize({x, y}); });
}
int oneui_window_set_maximum_client_size(OneUiWindow *w, float x, float y) {
    return desktopCall(__func__, [&] { return desktopWindow(w).setMaximumClientSize({x, y}); });
}
int oneui_window_set_owner(OneUiWindow *w, OneUiWindow *p) {
    return desktopCall(__func__,
                       [&] { return desktopWindow(w).setOwner(p ? &desktopWindow(p) : nullptr); });
}
OneUiWindowSubscription *oneui_window_observe_state(OneUiWindow *window,
                                                    OneUiWindowStateCallback callback, void *data,
                                                    OneUiDestroyCallback cleanup) {
    try {
        auto owned = ownDesktopCallback(data, cleanup);
        if (!callback)
            return nullptr;
        auto value = std::make_unique<OneUiWindowSubscription>();
        value->value =
            desktopWindow(window).observeState([owned, callback](const oneui::WindowState &state) {
                const auto value = snapshot(state);
                callback(&value, owned->data);
            });
        return value.release();
    } catch (...) {
        recordCurrentException(__func__);
        return nullptr;
    }
}
void oneui_window_subscription_destroy(OneUiWindowSubscription *value) { delete value; }
OneUiModalSession *oneui_modal_show(OneUiWindow *child, OneUiWindow *parent,
                                    OneUiIntCallback callback, void *data,
                                    OneUiDestroyCallback cleanup) {
    try {
        auto owned = ownDesktopCallback(data, cleanup);
        auto value = std::make_unique<OneUiModalSession>();
        value->value = oneui::ModalSession::show(desktopWindow(child), desktopWindow(parent),
                                                 [owned, callback](int result) {
                                                     if (callback)
                                                         callback(result, owned->data);
                                                 });
        return value->value ? value.release() : nullptr;
    } catch (...) {
        recordCurrentException(__func__);
        return nullptr;
    }
}
void oneui_modal_finish(OneUiModalSession *value, int result) {
    desktopCall(__func__, [&] {
        if (value)
            value->value->finish(result);
        return true;
    });
}
void oneui_modal_destroy(OneUiModalSession *value) { delete value; }
