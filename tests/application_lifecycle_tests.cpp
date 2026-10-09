#include "oneui/application.h"
#include "oneui/modal_session.h"
#include "oneui/controls/toast_host.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <atomic>

using namespace oneui;
void applicationModalRegressions();
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void queuedActivation() {
    Application app({"org.oneui.tests.activation", L"Activation"});
    app.deliverActivation({"one", "open", "first"});
    app.deliverActivation({"two", "dismiss", "second"});
    std::vector<std::string> received;
    app.setOnActivation([&](const ApplicationActivation& value) { received.push_back(value.notificationId + ":" + value.parameter); });
    require(received == std::vector<std::string>{"one:first", "two:second"}, "Activation queue lost ordering or payload");
    app.requestQuit();
}
void atomicQuit() {
    Application app({"org.oneui.tests.quit", L"Quit"});
    auto first = Window::create(L"First", 400, 300), second = Window::create(L"Second", 400, 300);
    require(app.addWindow(*first, true) && app.addWindow(*second), "Registration failed");
    require(!app.addWindow(*first), "Duplicate registration accepted");
    first->initialize(); second->initialize();
    std::function<void(bool)> firstAnswer, secondAnswer;
    app.setCloseGuard(*first, [&](auto answer) { firstAnswer = std::move(answer); });
    app.setCloseGuard(*second, [&](auto answer) { secondAnswer = std::move(answer); });
    app.requestQuit(); firstAnswer(true);
    require(!first->closed() && !second->closed(), "Quit closed a window before all confirmations");
    secondAnswer(false);
    require(!first->closed() && !second->closed(), "Rejected quit changed window lifetime");
    app.requestQuit(); auto oldAnswer = firstAnswer; firstAnswer(true); oldAnswer(false);
    require(!first->closed(), "Duplicate guard reply committed quit");
    secondAnswer(true);
    require(first->closed() && second->closed() && app.windows().empty(), "Accepted quit did not close and unregister all windows");
}
void policyAndModal() {
    Application app({"org.oneui.tests.modal", L"Modal", QuitPolicy::Explicit});
    auto parent = Window::create(L"Parent", 400, 300), child = Window::create(L"Child", 300, 200);
    app.addWindow(*parent, true); app.addWindow(*child);
    bool completed = false; int result = -1;
    auto session = ModalSession::show(*child, *parent, [&](int value) { completed = true; result = value; });
    require(bool(session), "Native owner/modal support unavailable");
    app.post([&] {
        session->finish(7);
        // requestClose may be native-message based. Check after it is processed.
        app.post([&] {
            require(completed && result == 7 && !session->active(), "Modal result was not delivered once");
            require(!parent->closed(), "Closing child also closed parent");
            parent->close();
            require(app.windows().empty(), "Closed windows were not unregistered");
            app.requestQuit();
        });
    });
    require(app.run() == 0, "Application event loop failed");
}
void closedDuringConfirmation() {
    Application app({"org.oneui.tests.retiredvote", L"Retired vote", QuitPolicy::Explicit});
    auto first = Window::create(L"First", 400, 300), second = Window::create(L"Second", 400, 300);
    first->initialize(); second->initialize(); app.addWindow(*first); app.addWindow(*second);
    std::function<void(bool)> firstAnswer, secondAnswer;
    app.setCloseGuard(*first, [&](auto reply) { firstAnswer = std::move(reply); });
    app.setCloseGuard(*second, [&](auto reply) { secondAnswer = std::move(reply); });
    app.requestQuit();
    auto late = Window::create(L"Late", 300, 200);
    require(!app.addWindow(*late), "Window entered an in-progress quit without a confirmation vote");
    second->close();
    firstAnswer(true);
    require(first->closed() && app.windows().empty(), "Closed guard owner left quit waiting forever");
    secondAnswer(false);
    require(first->closed(), "Late reply reversed a completed quit");
}
void toastQueue() {
    ToastHost host(2);
    ToastMessage first; first.id = "a"; first.title = L"First"; first.timeoutMs = 100;
    require(host.push(first), "Toast insertion failed");
    first.title = L"Updated"; require(host.push(first) && host.pendingCount() == 1, "ID update duplicated toast");
    ToastMessage second; second.id = "b"; second.timeoutMs = 0; host.push(second);
    ToastMessage third; third.id = "c"; require(!host.push(third), "Toast queue capacity ignored");
    host.tickAnimations(0); host.tickAnimations(50); host.setActive(false);
    host.tickAnimations(10000); require(host.pendingCount() == 2, "Inactive toast expired");
    host.setActive(true); host.tickAnimations(20000); host.tickAnimations(20060);
    require(host.pendingCount() == 1, "Toast timeout did not advance queue");
    require(host.dismiss("b") && host.pendingCount() == 0, "Toast withdrawal failed");
}
void notificationLifetime() {
    Application::Dispatcher dispatch;
    const auto uiThread = std::this_thread::get_id();
    {
        Application app({"org.oneui.tests.notifications", L"Notification lifetime", QuitPolicy::Explicit});
        dispatch = app.dispatcher();
        NotificationService notifications(app);
        bool delivered = false, workerDelivered = false;
        Notification invalid; // Empty ID is rejected asynchronously, without contacting the OS.
        require(notifications.send(invalid, [&](const NotificationResult& result) {
            require(std::this_thread::get_id() == uiThread, "Notification callback ran off the UI thread");
            require(result.status == NotificationStatus::Failed, "Invalid notification was accepted");
            delivered = true;
        }), "Notification operation was not queued");
        require(!delivered, "Notification result was delivered synchronously");
        auto retained = std::make_shared<int>(7); std::weak_ptr<int> weak = retained;
        {
            NotificationService cancelled(app);
            cancelled.send(invalid, [retained](const NotificationResult&) { throw std::runtime_error("Cancelled callback ran"); });
            retained.reset();
        }
        require(weak.expired(), "Destroyed service retained a user callback");
        std::thread worker([&] {
            require(dispatch([&] {
                workerDelivered = true;
                require(std::this_thread::get_id() == uiThread && delivered, "Dispatcher ordering/thread mismatch");
                app.requestQuit();
            }), "Worker dispatch rejected");
        });
        worker.join(); app.run();
        require(delivered && workerDelivered, "Queued work was lost");
    }
    require(!dispatch([] { throw std::runtime_error("Callback survived Application"); }), "Expired dispatcher accepted work");
}
}
int main() {
    try { queuedActivation(); atomicQuit(); policyAndModal(); closedDuringConfirmation(); toastQueue(); notificationLifetime(); applicationModalRegressions(); std::cout << "Application lifecycle tests passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
