#pragma once
#include "oneui/export.h"
#include "oneui/notification.h"
#include "oneui/platform/window.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oneui {
enum class QuitPolicy { LastWindowClosed, MainWindowClosed, Explicit };
struct ApplicationOptions {
    std::string id;
    std::wstring name;
    QuitPolicy quitPolicy = QuitPolicy::LastWindowClosed;
};
struct ApplicationActivation {
    std::string notificationId;
    std::string actionId;
    std::string parameter;
};
// One application per UI thread. Registered windows remain caller-owned.
// Keep Application alive until all registered windows have been destroyed.
class ONEUI_API Application final {
  public:
    static Application *current();
    explicit Application(ApplicationOptions options);
    ~Application();
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    const ApplicationOptions &options() const;
    bool addWindow(Window &window, bool main = false);
    bool removeWindow(Window &window);
    std::vector<Window *> windows() const;
    void setQuitPolicy(QuitPolicy policy);
    int run();
    void requestQuit();
    // A guard can answer asynchronously. No registered window closes until all
    // answers accept. The completion is single-use and must run on the UI thread.
    using CloseGuard = std::function<void(std::function<void(bool)>)>;
    void setCloseGuard(Window &window, CloseGuard guard);
    bool post(std::function<void()> callback);
    // Safe to retain on worker threads. Returns false after application shutdown;
    // callbacks already queued are suppressed when the application is destroyed.
    using Dispatcher = std::function<bool(std::function<void()>)>;
    Dispatcher dispatcher() const;
    bool isPrimaryInstance() const;
    static NotificationResult installDesktopIntegration(const ApplicationOptions &,
                                                        const std::wstring &executable);
    static NotificationResult uninstallDesktopIntegration(const ApplicationOptions &);
    void setOnActivation(std::function<void(const ApplicationActivation &)> callback);
    void deliverActivation(ApplicationActivation activation);
    Window &dispatcherWindow();

  private:
    friend class NotificationService;
    std::shared_ptr<platform::NotificationBackend> notificationBackend() const;
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
} // namespace oneui
