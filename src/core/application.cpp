#include "oneui/application.h"
#include "../platform/shared/notification_backend.h"
#include <algorithm>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace oneui {
namespace {
thread_local Application *currentApplication = nullptr;
}
Application *Application::current() { return currentApplication; }
struct Application::Impl : std::enable_shared_from_this<Impl> {
    ApplicationOptions options;
    using EventSubscription = Window::EventSubscription;
    struct Entry {
        Window *window;
        bool main;
        EventSubscription closed, request;
        CloseGuard guard;
        bool closing = false;
        std::uint64_t serial = 0;
    };
    std::vector<Entry> entries;
    std::map<Window *, std::function<void(bool)>> quitVotes;
    std::uint64_t nextSerial = 1;
    std::unique_ptr<Window> pump;
    struct DispatchGate {
        std::mutex mutex;
        Window *window = nullptr;
    };
    std::shared_ptr<DispatchGate> gate = std::make_shared<DispatchGate>();
    std::shared_ptr<platform::NotificationBackend> notifications;
    std::thread::id thread = std::this_thread::get_id();
    std::function<void(const ApplicationActivation &)> activation;
    std::deque<ApplicationActivation> pending;
    bool running = false, quitting = false, shutdown = false;
    std::uint64_t quitGeneration = 0;
    void check() const {
        if (thread != std::this_thread::get_id())
            throw std::logic_error("Application belongs to its UI thread");
    }
    void forget(Window *window) {
        auto found = std::find_if(entries.begin(), entries.end(),
                                  [&](auto &e) { return e.window == window; });
        if (found == entries.end())
            return;
        bool main = found->main;
        entries.erase(found);
        if (auto vote = quitVotes.find(window); vote != quitVotes.end()) {
            auto answer = vote->second;
            answer(true);
        }
        if (!shutdown && !quitting &&
            ((options.quitPolicy == QuitPolicy::LastWindowClosed && entries.empty()) ||
             (options.quitPolicy == QuitPolicy::MainWindowClosed && main)))
            requestQuit();
    }
    bool requestOne(Window *window) {
        auto keepAlive = shared_from_this();
        check();
        if (shutdown || quitting)
            return true;
        auto found = std::find_if(entries.begin(), entries.end(),
                                  [&](auto &e) { return e.window == window; });
        if (found == entries.end())
            return false;
        if ((options.quitPolicy == QuitPolicy::MainWindowClosed && found->main) ||
            (options.quitPolicy == QuitPolicy::LastWindowClosed && entries.size() == 1)) {
            requestQuit();
            return true;
        }
        if (found->closing)
            return true;
        if (!found->guard)
            return false;
        found->closing = true;
        auto guard = found->guard;
        auto replied = std::make_shared<bool>(false);
        auto answer = [weak = weak_from_this(), window, serial = found->serial,
                       replied](bool accept) {
            auto self = weak.lock();
            if (!self)
                return;
            self->check();
            if (*replied)
                return;
            *replied = true;
            auto item = std::find_if(self->entries.begin(), self->entries.end(), [&](auto &e) {
                return e.window == window && e.serial == serial;
            });
            if (item == self->entries.end())
                return;
            item->closing = false;
            if (accept)
                window->close();
        };
        try {
            guard(answer);
        } catch (...) {
            answer(false);
        }
        return true;
    }
    void requestQuit() {
        auto keepAlive = shared_from_this();
        check();
        if (quitting || shutdown)
            return;
        quitting = true;
        auto generation = ++quitGeneration;
        struct Pending {
            std::size_t remaining = 0;
            bool rejected = false;
        };
        auto vote = std::make_shared<Pending>();
        const auto snapshot = entries;
        vote->remaining = snapshot.size();
        auto finish = [weak = weak_from_this(), generation, vote] {
            auto self = weak.lock();
            if (!self || self->quitGeneration != generation || !self->quitting || vote->remaining)
                return;
            if (vote->rejected) {
                self->quitting = false;
                return;
            }
            self->shutdown = true;
            self->quitVotes.clear();
            auto windows = self->entries;
            for (auto &entry : windows) {
                auto live = std::find_if(self->entries.begin(), self->entries.end(),
                                         [&](auto &e) { return e.window == entry.window; });
                if (live != self->entries.end()) {
                    entry.window->setCloseToTray(false);
                    entry.window->close();
                }
            }
            self->pump->close();
        };
        if (snapshot.empty()) {
            finish();
            return;
        }
        for (auto &entry : snapshot) {
            auto replied = std::make_shared<bool>(false);
            auto answer = [weak = weak_from_this(), replied, vote, finish,
                           window = entry.window](bool accept) {
                auto self = weak.lock();
                if (!self)
                    return;
                self->check();
                if (*replied)
                    return;
                *replied = true;
                self->quitVotes.erase(window);
                vote->rejected |= !accept;
                --vote->remaining;
                finish();
            };
            auto live = std::find_if(entries.begin(), entries.end(),
                                     [&](auto &e) { return e.window == entry.window; });
            if (live != entries.end() && entry.guard) {
                quitVotes[entry.window] = answer;
                try {
                    entry.guard(answer);
                } catch (...) {
                    answer(false);
                }
            } else
                answer(true);
        }
    }
};
Application::Application(ApplicationOptions options) : impl_(std::make_shared<Impl>()) {
    if (currentApplication)
        throw std::logic_error("Only one Application is allowed per UI thread");
    if (!platform::validApplicationId(options.id))
        throw std::invalid_argument(
            "Application requires a reverse-DNS identifier of at most 128 ASCII bytes");
    impl_->options = std::move(options);
    WindowOptions pump;
    pump.title = impl_->options.name;
    pump.width = pump.height = 1;
    impl_->pump = Window::create(pump);
    auto dispatch = dispatcher();
    std::weak_ptr<Impl> weak = impl_;
    struct Startup {
        std::mutex mutex;
        bool ready = false;
        std::vector<ApplicationActivation> pending;
    };
    auto startup = std::make_shared<Startup>();
    auto receive = [weak, dispatch, startup](ApplicationActivation value) {
        {
            std::lock_guard<std::mutex> lock(startup->mutex);
            if (!startup->ready) {
                if (startup->pending.size() < 64)
                    startup->pending.push_back(std::move(value));
                return;
            }
        }
        dispatch([weak, value = std::move(value)] {
            auto self = weak.lock();
            if (!self || self->shutdown)
                return;
            if (auto callback = self->activation)
                callback(value);
            else {
                if (self->pending.size() == 64)
                    self->pending.pop_front();
                self->pending.push_back(value);
            }
        });
    };
    // Install the native delegate/COM server before macOS finishLaunching and
    // before any OS startup event can enter the window event loop.
    impl_->notifications = platform::makeNotificationBackend(impl_->options, receive);
    impl_->pump->initialize();
    impl_->gate->window = impl_->pump.get();
    std::vector<ApplicationActivation> early;
    {
        std::lock_guard<std::mutex> lock(startup->mutex);
        startup->ready = true;
        early.swap(startup->pending);
    }
    for (auto &value : early)
        receive(std::move(value));
    currentApplication = this;
}
Application::~Application() {
    if (currentApplication == this)
        currentApplication = nullptr;
    impl_->check();
    impl_->shutdown = true;
    impl_->quitVotes.clear();
    impl_->entries.clear();
    impl_->activation = {};
    impl_->pending.clear();
    {
        std::lock_guard<std::mutex> lock(impl_->gate->mutex);
        impl_->gate->window = nullptr;
    }
    impl_->notifications.reset();
    impl_->pump->setCloseToTray(false);
    impl_->pump->close();
}
const ApplicationOptions &Application::options() const { return impl_->options; }
bool Application::addWindow(Window &window, bool main) {
    impl_->check();
    if (impl_->shutdown || impl_->quitting || window.closed() || &window == impl_->pump.get())
        return false;
    if (std::any_of(impl_->entries.begin(), impl_->entries.end(),
                    [&](auto &e) { return e.window == &window; }))
        return false;
    if (main)
        for (auto &entry : impl_->entries)
            entry.main = false;
    std::weak_ptr<Impl> weak = impl_;
    auto closed = window.observeClosed([weak, address = &window] {
        if (auto self = weak.lock())
            self->forget(address);
    });
    auto request = window.interceptCloseRequest([weak, address = &window](bool) {
        auto self = weak.lock();
        return self && self->requestOne(address);
    });
    impl_->entries.push_back(
        {&window, main, std::move(closed), std::move(request), {}, false, impl_->nextSerial++});
    return true;
}
bool Application::removeWindow(Window &window) {
    auto self = impl_; // A quit vote can synchronously close windows and destroy Application.
    self->check();
    auto size = self->entries.size();
    self->entries.erase(std::remove_if(self->entries.begin(), self->entries.end(),
                                       [&](auto &e) { return e.window == &window; }),
                        self->entries.end());
    const bool removed = size != self->entries.size();
    if (auto vote = self->quitVotes.find(&window); vote != self->quitVotes.end()) {
        auto answer = vote->second;
        answer(true);
    }
    return removed;
}
std::vector<Window *> Application::windows() const {
    impl_->check();
    std::vector<Window *> result;
    for (auto &entry : impl_->entries)
        result.push_back(entry.window);
    return result;
}
void Application::setQuitPolicy(QuitPolicy policy) {
    impl_->check();
    impl_->options.quitPolicy = policy;
}
int Application::run() {
    impl_->check();
    if (impl_->running)
        throw std::logic_error("Application loop is already running");
    if (impl_->shutdown)
        return 0;
    impl_->running = true;
    auto self = impl_;
    struct Running {
        Impl &state;
        ~Running() { state.running = false; }
    } running{*self};
    return self->pump->run();
}
void Application::requestQuit() { impl_->requestQuit(); }
void Application::setCloseGuard(Window &window, CloseGuard guard) {
    impl_->check();
    for (auto &e : impl_->entries)
        if (e.window == &window) {
            e.guard = std::move(guard);
            return;
        }
    throw std::invalid_argument("Window is not registered");
}
bool Application::post(std::function<void()> callback) { return dispatcher()(std::move(callback)); }
Application::Dispatcher Application::dispatcher() const {
    std::weak_ptr<Impl::DispatchGate> weak = impl_->gate;
    return [weak](std::function<void()> callback) {
        auto gate = weak.lock();
        if (!gate || !callback)
            return false;
        std::lock_guard<std::mutex> lock(gate->mutex);
        if (!gate->window)
            return false;
        return gate->window->post([weak, callback = std::move(callback)] {
            auto live = weak.lock();
            if (!live)
                return;
            {
                std::lock_guard<std::mutex> lock(live->mutex);
                if (!live->window)
                    return;
            }
            callback();
        });
    };
}
bool Application::isPrimaryInstance() const {
    return !impl_->notifications || impl_->notifications->primary();
}
std::shared_ptr<platform::NotificationBackend> Application::notificationBackend() const {
    impl_->check();
    return impl_->notifications;
}
NotificationResult Application::installDesktopIntegration(const ApplicationOptions &options,
                                                          const std::wstring &executable) {
    if (!platform::validApplicationId(options.id))
        return {NotificationStatus::Failed, {}, "Invalid application identity"};
    return platform::registerDesktopApplication(options, executable);
}
NotificationResult Application::uninstallDesktopIntegration(const ApplicationOptions &options) {
    if (!platform::validApplicationId(options.id))
        return {NotificationStatus::Failed, {}, "Invalid application identity"};
    return platform::unregisterDesktopApplication(options);
}
Window &Application::dispatcherWindow() { return *impl_->pump; }
void Application::setOnActivation(std::function<void(const ApplicationActivation &)> callback) {
    auto self = impl_;
    self->check();
    self->activation = std::move(callback);
    while (!self->shutdown && self->activation && !self->pending.empty()) {
        auto value = std::move(self->pending.front());
        self->pending.pop_front();
        auto deliver = self->activation;
        deliver(value);
    }
}
void Application::deliverActivation(ApplicationActivation value) {
    impl_->check();
    if (impl_->shutdown)
        return;
    if (auto callback = impl_->activation)
        callback(value);
    else {
        if (impl_->pending.size() == 64)
            impl_->pending.pop_front();
        impl_->pending.push_back(std::move(value));
    }
}
} // namespace oneui
