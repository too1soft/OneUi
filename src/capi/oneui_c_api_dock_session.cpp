#include "oneui/layout/dock_session.h"
#include "oneui_c_api_application_internal.h"
#include "oneui_c_api_desktop_internal.h"
#include "oneui_c_api_widget_internal.h"

using namespace oneui::capi;
struct OneUiDockSession {
    oneui::DockSession value;
    explicit OneUiDockSession(oneui::Application &a) : value(a) {}
};
namespace {
oneui::DockSession &session(OneUiDockSession *value) {
    if (!value)
        throw std::invalid_argument("DockSession is required");
    return value->value;
}
oneui::DockDestination destination(OneUiDockDestination value) {
    return {value.host, value.pane, oneui::DockPosition(value.position), value.index};
}
template <class F> int operation(const char *name, F callback) {
    try {
        return int(callback());
    } catch (...) {
        recordCurrentException(name);
        return -1;
    }
}
void ids(const std::vector<oneui::DockId> &source, OneUiDockIdsCallback callback, void *data) {
    if (!callback)
        throw std::invalid_argument("IDs callback is required");
    std::vector<unsigned long long> result(source.begin(), source.end());
    callback(result.data(), result.size(), data);
}
} // namespace
OneUiDockSession *oneui_dock_session_create(OneUiApplication *app) {
    try {
        return new OneUiDockSession(desktopApplication(app));
    } catch (...) {
        recordCurrentException(__func__);
        return nullptr;
    }
}
void oneui_dock_session_destroy(OneUiDockSession *value) { delete value; }
int oneui_dock_session_set_style_sheet(OneUiDockSession *value,OneUiStyleSheet *sheet) {
    return operation(__func__,[&]{return session(value).setStyleSheet(sheet?sheet->sheet:nullptr);});
}
unsigned long long oneui_dock_session_attach_host(OneUiDockSession *value, OneUiWindow *window,
                                                  OneUiWidget *panel, void *data,
                                                  OneUiDestroyCallback cleanup) {
    try {
        auto owner = ownDesktopCallback(data, cleanup);
        auto surface = panel ? std::dynamic_pointer_cast<oneui::Panel>(panel->widget) : nullptr;
        return session(value).attachHost(desktopWindow(window), std::move(surface),
                                         std::move(owner));
    } catch (...) {
        recordCurrentException(__func__);
        return 0;
    }
}
int oneui_dock_session_add_pane_utf8(OneUiDockSession *value, unsigned long long id,
                                     OneUiUtf8String title, OneUiWidget *widget, float minWidth,
                                     float minHeight, OneUiDockDestination dest,
                                     OneUiDockTransferCallback callback, void *data,
                                     OneUiDestroyCallback cleanup) {
    return operation(__func__, [&] {
        auto owner = ownDesktopCallback(data, cleanup);
        oneui::DockPane pane;
        pane.id = id;
        pane.title = oneui::unicode::fromUtf8(desktopString(title));
        pane.content = widget ? widget->widget : nullptr;
        pane.minimum = {minWidth, minHeight};
        pane.owner = owner;
        if (callback)
            pane.transfer = [owner, callback](oneui::Window &source, oneui::Window &target,
                                              oneui::DockTransferPhase phase) {
                return callback(source.nativeHandle(), target.nativeHandle(), int(phase),
                                owner->data) != 0;
            };
        return session(value).addPane(std::move(pane), destination(dest));
    });
}
int oneui_dock_session_remove_pane(OneUiDockSession *value, unsigned long long pane) {
    return operation(__func__, [&] { return session(value).removePane(pane); });
}
int oneui_dock_session_move_pane(OneUiDockSession *value, unsigned long long pane,
                                 OneUiDockDestination dest) {
    return operation(__func__, [&] { return session(value).movePane(pane, destination(dest)); });
}
int oneui_dock_session_select_pane(OneUiDockSession *value, unsigned long long pane) {
    return operation(__func__, [&] { return session(value).selectPane(pane); });
}
int oneui_dock_session_detach_pane_utf8(OneUiDockSession *value, unsigned long long pane,
                                        OneUiUtf8String title, int width, int height,
                                        unsigned long long *host) {
    if (host)
        *host = 0;
    return operation(__func__, [&] {
        oneui::DockId id = 0;
        if (width <= 0 || height <= 0)
            throw std::invalid_argument("Invalid detached window size");
        auto result = session(value).detachPane(
            pane, {oneui::unicode::fromUtf8(desktopString(title)), width, height}, &id);
        if (host)
            *host = id;
        return result;
    });
}
unsigned long long oneui_dock_session_begin_drag(OneUiDockSession *value, unsigned long long pane) {
    try {
        return session(value).beginDrag(pane);
    } catch (...) {
        recordCurrentException(__func__);
        return 0;
    }
}
int oneui_dock_session_commit_drag(OneUiDockSession *value, unsigned long long token,
                                   OneUiDockDestination dest) {
    return operation(__func__, [&] { return session(value).commitDrag(token, destination(dest)); });
}
int oneui_dock_session_cancel_drag(OneUiDockSession *value, unsigned long long token) {
    return desktopCall(__func__, [&] { return session(value).cancelDrag(token); });
}
int oneui_dock_session_native_drag_supported(OneUiDockSession* value, unsigned long long host) {
    return desktopCall(__func__, [&] { return session(value).nativeDragSupported(host); });
}
int oneui_dock_session_start_native_drag(OneUiDockSession* value, unsigned long long pane) {
    return desktopCall(__func__, [&] { return session(value).startNativeDrag(pane); });
}
OneUiDockPaneLocation oneui_dock_session_location(OneUiDockSession *value,
                                                  unsigned long long pane) {
    try {
        auto p = session(value).location(pane);
        return {p.host, p.group, p.index, p.selected};
    } catch (...) {
        recordCurrentException(__func__);
        return {0, 0, -1, 0};
    }
}
int oneui_dock_session_panes(OneUiDockSession *value, unsigned long long host,
                             OneUiDockIdsCallback callback, void *data) {
    return desktopCall(__func__, [&] {
        ids(session(value).panes(host), callback, data);
        return true;
    });
}
int oneui_dock_session_hosts(OneUiDockSession *value, OneUiDockIdsCallback callback, void *data) {
    return desktopCall(__func__, [&] {
        ids(session(value).hosts(), callback, data);
        return true;
    });
}
int oneui_dock_session_set_on_changed(OneUiDockSession *value, OneUiVoidCallback callback,
                                      void *data, OneUiDestroyCallback cleanup) {
    return desktopCall(__func__, [&] {
        auto owner = ownDesktopCallback(data, cleanup);
        session(value).setOnChanged(
            callback ? std::function<void()>([owner, callback] { callback(owner->data); })
                     : nullptr);
        return true;
    });
}

int oneui_dock_session_native_drag_limitation_utf8(OneUiDockSession *value, unsigned long long host, OneUiUtf8TextCallback callback, void *data) {
    return desktopCall(__func__, [&] {
        if (!callback) throw std::invalid_argument("Text callback is required");
        auto text = session(value).nativeDragLimitation(host);
        callback(text.data(), text.size(), data);
        return true;
    });
}
