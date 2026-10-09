#pragma once
#include "oneui/application.h"
#include "oneui/notification.h"

namespace oneui::platform {
class NotificationBackend {
  public:
    using Result = NotificationService::Completion;
    using Permission = NotificationService::PermissionCompletion;
    using Activation = std::function<void(ApplicationActivation)>;
    virtual ~NotificationBackend() = default;
    virtual void permission(bool request, Permission callback) = 0;
    virtual void send(const Notification &, Result callback) = 0;
    virtual void withdraw(const std::string &id, Result callback) = 0;
    virtual bool primary() const { return true; }
};
std::shared_ptr<NotificationBackend> makeNotificationBackend(const ApplicationOptions &,
                                                             NotificationBackend::Activation);
NotificationResult registerDesktopApplication(const ApplicationOptions &,
                                              const std::wstring &executable);
NotificationResult unregisterDesktopApplication(const ApplicationOptions &);
// Strict, bounded validation shared by all native backends and language bindings.
bool validNotification(const Notification &, std::string &error);
bool validApplicationId(const std::string &);
std::string encodeActivation(const ApplicationActivation &);
bool decodeActivation(const std::string &, ApplicationActivation &);
} // namespace oneui::platform
