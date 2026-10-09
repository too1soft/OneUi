#include "oneui/application.h"
#include "oneui/notification.h"
#include "../platform/shared/notification_backend.h"
#include <atomic>
#include <map>
#include <stdexcept>
#include <thread>
#include <variant>

namespace oneui {
struct NotificationService::Impl {
    std::weak_ptr<platform::NotificationBackend> backend;
    Application::Dispatcher dispatch;
    std::thread::id thread = std::this_thread::get_id();
    std::atomic<bool> active{true};
    using Value = std::variant<NotificationResult, NotificationPermissionResult>;
    std::map<std::uint64_t, std::function<void(const Value &)>> callbacks;
    std::uint64_t next = 0;
    void check() const {
        if (thread != std::this_thread::get_id())
            throw std::logic_error("NotificationService belongs to its UI thread");
    }
};
NotificationService::NotificationService(Application &app) : impl_(std::make_shared<Impl>()) {
    impl_->backend = app.notificationBackend();
    impl_->dispatch = app.dispatcher();
}
NotificationService::~NotificationService() {
    impl_->active = false;
    impl_->callbacks.clear();
}
namespace {
template <class State, class Callback>
auto completion(const std::shared_ptr<State> &state, Callback cb) {
    const auto token = ++state->next;
    state->callbacks.emplace(token, [cb = std::move(cb)](const typename State::Value &value) {
        std::visit(
            [&](const auto &result) {
                if constexpr (std::is_invocable_v<Callback, decltype(result)>) {
                    if (cb)
                        cb(result);
                }
            },
            value);
    });
    // Only IDs/results cross native worker threads. User closures and their
    // cleanup (including Rust Rc captures) remain owned by the UI-thread map.
    return [weak = std::weak_ptr<State>(state), token](const auto &result) {
        if (auto owner = weak.lock(); owner && owner->active) {
            owner->dispatch([weak, token, result] {
                auto alive = weak.lock();
                if (!alive || !alive->active)
                    return;
                auto found = alive->callbacks.find(token);
                if (found == alive->callbacks.end())
                    return;
                auto callback = std::move(found->second);
                alive->callbacks.erase(found);
                callback(typename State::Value(result));
            });
        }
    };
}
} // namespace
bool NotificationService::queryPermission(PermissionCompletion callback) {
    impl_->check();
    auto backend = impl_->backend.lock();
    if (!backend || impl_->callbacks.size() >= 1024)
        return false;
    backend->permission(false, completion(impl_, std::move(callback)));
    return true;
}
bool NotificationService::requestPermission(PermissionCompletion callback) {
    impl_->check();
    auto backend = impl_->backend.lock();
    if (!backend || impl_->callbacks.size() >= 1024)
        return false;
    backend->permission(true, completion(impl_, std::move(callback)));
    return true;
}
bool NotificationService::send(Notification notification, Completion callback) {
    impl_->check();
    auto backend = impl_->backend.lock();
    if (!backend || impl_->callbacks.size() >= 1024)
        return false;
    auto done = completion(impl_, std::move(callback));
    std::string error;
    if (!platform::validNotification(notification, error)) {
        done(NotificationResult{NotificationStatus::Failed, notification.id, std::move(error)});
        return true;
    }
    backend->send(notification, std::move(done));
    return true;
}
bool NotificationService::withdraw(std::string id, Completion callback) {
    impl_->check();
    auto backend = impl_->backend.lock();
    if (!backend || impl_->callbacks.size() >= 1024)
        return false;
    auto done = completion(impl_, std::move(callback));
    std::string error;
    Notification probe;
    probe.id = id;
    probe.title = L"withdraw";
    if (!platform::validNotification(probe, error)) {
        done(NotificationResult{NotificationStatus::Failed, std::move(id), std::move(error)});
        return true;
    }
    backend->withdraw(id, std::move(done));
    return true;
}
} // namespace oneui
