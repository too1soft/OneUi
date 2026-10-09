#pragma once
#include "oneui/export.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oneui {
class Application;
namespace platform {
class NotificationBackend;
}
enum class NotificationStatus {
    Accepted,
    PermissionDenied,
    ServiceUnavailable,
    Unsupported,
    Failed
};
enum class NotificationPermission { Unknown, NotDetermined, Granted, Denied, Unsupported };
struct NotificationResult {
    NotificationStatus status = NotificationStatus::Failed;
    std::string id;
    std::string detail;
};
struct NotificationPermissionResult {
    NotificationPermission permission = NotificationPermission::Unknown;
    NotificationStatus status = NotificationStatus::Failed;
    std::string detail;
};
struct NotificationAction {
    std::string id;
    std::wstring title;
};
struct Notification {
    // Stable across processes. Sending the same ID replaces the previous request.
    std::string id;
    std::wstring title, body;
    std::string parameter;
    std::vector<NotificationAction> actions;
};
// UI-thread service. Results are delivered asynchronously on the Application's
// UI thread. Accepted means the OS accepted a request, never proof of display.
// Destruction cancels outstanding callbacks, but does not withdraw notifications.
class ONEUI_API NotificationService final {
  public:
    explicit NotificationService(Application &application);
    ~NotificationService();
    NotificationService(const NotificationService &) = delete;
    NotificationService &operator=(const NotificationService &) = delete;
    using Completion = std::function<void(const NotificationResult &)>;
    using PermissionCompletion = std::function<void(const NotificationPermissionResult &)>;
    bool queryPermission(PermissionCompletion callback);
    bool requestPermission(PermissionCompletion callback);
    bool send(Notification notification, Completion callback);
    bool withdraw(std::string id, Completion callback);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace oneui
