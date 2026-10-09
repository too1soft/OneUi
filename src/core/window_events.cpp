#include "internal/runtime_error.h"
#include "oneui/platform/window.h"
#include <algorithm>
#include <stdexcept>
#include <thread>

namespace oneui {
struct Window::Events {
    template <class F> struct Observer {
        F callback;
    };
    using StateObserver = Observer<std::function<void(const WindowState &)>>;
    using CloseObserver = Observer<std::function<void()>>;
    std::vector<std::weak_ptr<StateObserver>> state;
    std::vector<std::weak_ptr<CloseObserver>> close;
    std::thread::id thread = std::this_thread::get_id();
    using RequestObserver = Observer<std::function<bool(bool)>>;
    std::weak_ptr<RequestObserver> request;
    bool closed = false, stateQueued = false;
    void check() const {
        if (thread != std::this_thread::get_id())
            throw std::logic_error("Window lifecycle belongs to its UI thread");
    }
};
Window::Window() : events_(std::make_shared<Events>()) {}
Window::~Window() { publishClosed(); }
bool Window::closed() const {
    events_->check();
    return events_->closed;
}
WindowState Window::state() const {
    WindowState result;
    result.clientSize = clientSize();
    result.pixelSize = clientPixelSize();
    result.dpiScale = dpiScale();
    return result;
}
Window::EventSubscription Window::observeState(std::function<void(const WindowState &)> callback) {
    events_->check();
    auto entry = std::make_shared<Events::StateObserver>();
    entry->callback = std::move(callback);
    events_->state.push_back(entry);
    return entry;
}
Window::EventSubscription Window::observeClosed(std::function<void()> callback) {
    events_->check();
    auto entry = std::make_shared<Events::CloseObserver>();
    entry->callback = std::move(callback);
    if (events_->closed) {
        if (entry->callback)
            entry->callback();
    } else
        events_->close.push_back(entry);
    return entry;
}
Window::EventSubscription Window::interceptCloseRequest(std::function<bool(bool)> callback) {
    events_->check();
    if (!events_->request.expired())
        throw std::logic_error("Window already belongs to an application");
    auto entry = std::make_shared<Events::RequestObserver>();
    entry->callback = std::move(callback);
    events_->request = entry;
    return entry;
}
bool Window::dispatchCloseRequest(bool forced) {
    events_->check();
    if (events_->closed)
        return true;
    if (auto entry = events_->request.lock())
        if (auto callback = entry->callback)
            return callback(forced);
    return false;
}
void Window::queueStateChange() {
    auto events = events_;
    events->check();
    if (events->closed || events->stateQueued || events->state.empty())
        return;
    events->stateQueued = true;
    if (!post([this, weak = std::weak_ptr<Events>(events)] {
            auto e = weak.lock();
            if (!e || e->closed)
                return;
            e->stateQueued = false;
            publishState();
        }))
        events->stateQueued = false;
}
void Window::publishState() {
    auto events = events_;
    events->check();
    if (events->closed)
        return;
    const auto snapshot = state();
    const auto observers = events->state;
    events->state.erase(std::remove_if(events->state.begin(), events->state.end(),
                                       [](auto &item) { return item.expired(); }),
                        events->state.end());
    for (auto &weak : observers) {
        if (events->closed)
            break;
        if (auto entry = weak.lock()) {
            auto callback = entry->callback;
            if (callback)
                try {
                    callback(snapshot);
                } catch (...) {
                    detail::recordCurrentException("Window.state");
                }
        }
    }
}
void Window::publishClosed() {
    auto events = events_;
    if (events->closed)
        return;
    events->closed = true;
    auto observers = std::move(events->close);
    events->state.clear();
    for (auto &weak : observers)
        if (auto entry = weak.lock())
            if (auto callback = entry->callback)
                try {
                    callback();
                } catch (...) {
                    detail::recordCurrentException("Window.closed");
                }
}
} // namespace oneui
