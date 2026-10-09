#include "internal/runtime_error.h"
#include "internal/unicode.h"
#include "platform/shared/notification_backend.h"
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>

namespace oneui::platform {
namespace {
bool serviceAvailable(GDBusConnection *bus) {
    if (!bus)
        return false;
    auto *reply = g_dbus_connection_call_sync(
        bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
        "NameHasOwner", g_variant_new("(s)", "org.freedesktop.Notifications"),
        G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NO_AUTO_START, 1000, nullptr, nullptr);
    gboolean owned = FALSE;
    if (reply) {
        g_variant_get(reply, "(b)", &owned);
        g_variant_unref(reply);
    }
    if (owned)
        return true;
    reply = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus", "ListActivatableNames", nullptr,
                                        G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NO_AUTO_START,
                                        1000, nullptr, nullptr);
    if (!reply)
        return false;
    GVariantIter *names = nullptr;
    const char *name = nullptr;
    g_variant_get(reply, "(as)", &names);
    while (g_variant_iter_next(names, "&s", &name))
        owned |= g_str_equal(name, "org.freedesktop.Notifications");
    g_variant_iter_free(names);
    g_variant_unref(reply);
    return owned;
}
class GioNotifications final : public NotificationBackend {
    GApplication *app_ = nullptr;
    std::string id_, failure_;
    Activation activation_;
    bool primary_ = true;
    static void activated(GSimpleAction *, GVariant *parameter, gpointer data) noexcept {
        auto &self = *static_cast<GioNotifications *>(data);
        if (!parameter || !g_variant_is_of_type(parameter, G_VARIANT_TYPE_STRING))
            return;
        ApplicationActivation value;
        if (!decodeActivation(g_variant_get_string(parameter, nullptr), value))
            return;
        try {
            if (self.activation_)
                self.activation_(std::move(value));
        } catch (...) {
            detail::recordCurrentException("Notification.activate");
        }
    }
    bool ready() const {
        if (!app_ || !failure_.empty() || !primary_)
            return false;
        auto *desktop = g_desktop_app_info_new((id_ + ".desktop").c_str());
        if (!desktop)
            return false;
        g_object_unref(desktop);
        return serviceAvailable(g_application_get_dbus_connection(app_));
    }

  public:
    GioNotifications(const ApplicationOptions &options, Activation activation)
        : id_(options.id), activation_(std::move(activation)) {
        app_ = g_application_new(id_.c_str(), G_APPLICATION_DEFAULT_FLAGS);
        auto *action = g_simple_action_new("oneui-notification", G_VARIANT_TYPE_STRING);
        g_signal_connect(action, "activate", G_CALLBACK(activated), this);
        g_action_map_add_action(G_ACTION_MAP(app_), G_ACTION(action));
        g_object_unref(action);
        g_signal_connect(app_, "activate", G_CALLBACK(+[](GApplication *, gpointer data) noexcept {
                             auto &self = *static_cast<GioNotifications *>(data);
                             try {
                                 if (self.activation_)
                                     self.activation_({});
                             } catch (...) {
                                 detail::recordCurrentException("Notification.activate");
                             }
                         }),
                         this);
        GError *error = nullptr;
        if (!g_application_register(app_, nullptr, &error)) {
            failure_ = error ? error->message : "GApplication registration failed";
            g_clear_error(&error);
            return;
        }
        primary_ = !g_application_get_is_remote(app_);
        if (!primary_)
            g_application_activate(app_);
    }
    ~GioNotifications() override { g_object_unref(app_); }
    bool primary() const override { return primary_; }
    void permission(bool request, Permission callback) override {
        if (!ready()) {
            callback({NotificationPermission::Unknown, NotificationStatus::ServiceUnavailable,
                      failure_.empty() ? "Install the application desktop/D-Bus entries and start "
                                         "a notification service"
                                       : failure_});
            return;
        }
        callback(
            {NotificationPermission::Unknown,
             request ? NotificationStatus::Unsupported : NotificationStatus::Accepted,
             "This desktop does not expose a portable notification permission query or prompt"});
    }
    void send(const Notification &value, Result callback) override {
        if (!ready()) {
            callback(
                {NotificationStatus::ServiceUnavailable, value.id,
                 "Application registration or the desktop notification service is unavailable"});
            return;
        }
        auto *notification = g_notification_new(unicode::toUtf8(value.title).c_str());
        g_notification_set_body(notification, unicode::toUtf8(value.body).c_str());
        auto *icon = g_themed_icon_new("application-x-executable");
        g_notification_set_icon(notification, icon);
        g_object_unref(icon);
        const auto payload = encodeActivation({value.id, {}, value.parameter});
        g_notification_set_default_action_and_target_value(notification, "app.oneui-notification",
                                                           g_variant_new_string(payload.c_str()));
        for (const auto &action : value.actions) {
            const auto target = encodeActivation({value.id, action.id, value.parameter});
            g_notification_add_button_with_target_value(
                notification, unicode::toUtf8(action.title).c_str(), "app.oneui-notification",
                g_variant_new_string(target.c_str()));
        }
        g_application_send_notification(app_, value.id.c_str(), notification);
        g_object_unref(notification);
        callback({NotificationStatus::Accepted, value.id,
                  "Submitted to GNotification; display is controlled by the desktop"});
    }
    void withdraw(const std::string &id, Result callback) override {
        if (!ready()) {
            callback(
                {NotificationStatus::ServiceUnavailable, id, "Notification service unavailable"});
            return;
        }
        g_application_withdraw_notification(app_, id.c_str());
        callback({NotificationStatus::Accepted, id, "Withdrawal submitted to GNotification"});
    }
};
} // namespace
std::shared_ptr<NotificationBackend>
makeNotificationBackend(const ApplicationOptions &options,
                        NotificationBackend::Activation activation) {
    return std::make_shared<GioNotifications>(options, std::move(activation));
}
} // namespace oneui::platform
