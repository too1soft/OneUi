#include "oneui/notification.h"
#include "oneui_c_api_application_internal.h"
#include "oneui_c_api_desktop_internal.h"

using namespace oneui::capi;
struct OneUiNotificationService {
    std::unique_ptr<oneui::NotificationService> impl;
};
namespace {
oneui::NotificationService &service(OneUiNotificationService *value) {
    if (!value || !value->impl)
        throw std::invalid_argument("Notification service is required");
    return *value->impl;
}
auto resultCallback(OneUiNotificationResultCallback callback, void *data,
                    OneUiDestroyCallback cleanup) {
    auto owned = ownDesktopCallback(data, cleanup);
    return [owned, callback](const oneui::NotificationResult &value) {
        if (callback)
            callback(int(value.status), {value.id.data(), value.id.size()},
                     {value.detail.data(), value.detail.size()}, owned->data);
    };
}
} // namespace
OneUiNotificationService *oneui_notification_service_create(OneUiApplication *app) {
    try {
        auto value = std::make_unique<OneUiNotificationService>();
        value->impl = std::make_unique<oneui::NotificationService>(desktopApplication(app));
        return value.release();
    } catch (...) {
        recordCurrentException(__func__);
        return nullptr;
    }
}
void oneui_notification_service_destroy(OneUiNotificationService *value) { delete value; }
int oneui_notification_permission(OneUiNotificationService *value, int request,
                                  OneUiNotificationPermissionCallback callback, void *data,
                                  OneUiDestroyCallback cleanup) {
    return desktopCall(__func__, [&] {
        auto owned = ownDesktopCallback(data, cleanup);
        auto done = [owned, callback](const oneui::NotificationPermissionResult &result) {
            if (callback)
                callback(int(result.permission), int(result.status),
                         {result.detail.data(), result.detail.size()}, owned->data);
        };
        return request ? service(value).requestPermission(done)
                       : service(value).queryPermission(done);
    });
}
int oneui_notification_send_utf8(OneUiNotificationService *value,
                                 const OneUiNotificationUtf8 *message,
                                 OneUiNotificationResultCallback callback, void *data,
                                 OneUiDestroyCallback cleanup) {
    return desktopCall(__func__, [&] {
        auto done = resultCallback(callback, data, cleanup);
        if (!message || message->action_count > 2 || (message->action_count && !message->actions))
            throw std::invalid_argument("Invalid notification or action array");
        oneui::Notification notification;
        notification.id = desktopString(message->id);
        notification.parameter = desktopString(message->parameter);
        notification.title = oneui::unicode::fromUtf8(desktopString(message->title));
        notification.body = oneui::unicode::fromUtf8(desktopString(message->body));
        for (std::size_t i = 0; i < message->action_count; ++i)
            notification.actions.push_back(
                {desktopString(message->actions[i].id),
                 oneui::unicode::fromUtf8(desktopString(message->actions[i].title))});
        return service(value).send(std::move(notification), std::move(done));
    });
}
int oneui_notification_withdraw_utf8(OneUiNotificationService *value, OneUiUtf8String id,
                                     OneUiNotificationResultCallback callback, void *data,
                                     OneUiDestroyCallback cleanup) {
    return desktopCall(__func__, [&] {
        auto done = resultCallback(callback, data, cleanup);
        return service(value).withdraw(desktopString(id), std::move(done));
    });
}
int oneui_application_is_primary(OneUiApplication *app) {
    return desktopCall(__func__, [&] { return desktopApplication(app).isPrimaryInstance(); });
}
int oneui_application_register_desktop_utf8(OneUiUtf8String id, OneUiUtf8String name,
                                            OneUiUtf8String executable, int install,
                                            OneUiNotificationResultCallback callback, void *data,
                                            OneUiDestroyCallback cleanup) {
    return desktopCall(__func__, [&] {
        auto done = resultCallback(callback, data, cleanup);
        oneui::ApplicationOptions options{desktopString(id),
                                          oneui::unicode::fromUtf8(desktopString(name))};
        const auto result = install
                                ? oneui::Application::installDesktopIntegration(
                                      options, oneui::unicode::fromUtf8(desktopString(executable)))
                                : oneui::Application::uninstallDesktopIntegration(options);
        done(result);
        return true;
    });
}
