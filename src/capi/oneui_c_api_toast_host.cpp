#include "oneui/controls/toast_host.h"
#include "oneui_c_api_desktop_internal.h"
#include "oneui_c_api_widget_internal.h"
#include <cmath>

using namespace oneui::capi;
namespace {
oneui::ToastHost &host(OneUiWidget *value) {
    auto *host = value ? dynamic_cast<oneui::ToastHost *>(value->widget.get()) : nullptr;
    if (!host)
        throw std::invalid_argument("ToastHost is required");
    return *host;
}
} // namespace
OneUiWidget *oneui_toast_host_create(size_t capacity) {
    try {
        if (capacity < 1 || capacity > 1024)
            throw std::invalid_argument("Toast capacity must be 1..1024");
        auto result = std::make_unique<OneUiWidget>();
        result->widget = std::make_shared<oneui::ToastHost>(capacity);
        return result.release();
    } catch (...) {
        recordCurrentException(__func__);
        return nullptr;
    }
}
int oneui_toast_host_push_utf8(OneUiWidget *widget, const OneUiToastMessageUtf8 *value,
                               OneUiUtf8TextCallback callback, void *data,
                               OneUiDestroyCallback cleanup) {
    return desktopCall(__func__, [&] {
        auto owned = ownDesktopCallback(data, cleanup);
        if (!value || !std::isfinite(value->timeout_ms) || value->timeout_ms < 0)
            throw std::invalid_argument("Invalid toast timeout");
        oneui::ToastMessage message;
        message.id = desktopString(value->id);
        message.title = oneui::unicode::fromUtf8(desktopString(value->title));
        message.message = oneui::unicode::fromUtf8(desktopString(value->message));
        message.primaryAction = oneui::unicode::fromUtf8(desktopString(value->primary_action));
        message.secondaryAction = oneui::unicode::fromUtf8(desktopString(value->secondary_action));
        message.timeoutMs = value->timeout_ms;
        message.onAction = [owned, callback](const std::string &action) {
            if (callback)
                callback(action.data(), action.size(), owned->data);
        };
        return host(widget).push(std::move(message));
    });
}
int oneui_toast_host_dismiss_utf8(OneUiWidget *widget, OneUiUtf8String id) {
    return desktopCall(__func__, [&] { return host(widget).dismiss(desktopString(id)); });
}
int oneui_toast_host_clear(OneUiWidget *widget) {
    return desktopCall(__func__, [&] {
        host(widget).clear();
        return true;
    });
}
int oneui_toast_host_set_active(OneUiWidget *widget, int active) {
    return desktopCall(__func__, [&] {
        host(widget).setActive(active != 0);
        return true;
    });
}
size_t oneui_toast_host_pending_count(OneUiWidget *widget) {
    try {
        return host(widget).pendingCount();
    } catch (...) {
        recordCurrentException(__func__);
        return 0;
    }
}
