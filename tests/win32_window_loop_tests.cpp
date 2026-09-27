#include "oneui/platform/window.h"

#include <windows.h>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

namespace {

int failures = 0;

void expectTrue(const char* name, bool value) {
    if (!value) {
        std::cerr << name << ": expected true\n";
        ++failures;
    }
}

auto hiddenWindow() {
    oneui::WindowOptions options;
    options.visible = false;
    auto window = oneui::Window::create(options);
    window->initialize();
    return window;
}

void testDestroyWithoutRunDoesNotQuitNextWindow() {
    for (int cycle = 0; cycle < 10; ++cycle) {
        { auto unused = hiddenWindow(); }
        MSG message{};
        const bool staleQuit = PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) != FALSE;
        expectTrue("Destroying an unpumped window does not enqueue WM_QUIT", !staleQuit);

        auto next = hiddenWindow();
        bool completed = false;
        std::thread worker([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            next->post([&] { completed = true; next->close(); });
        });
        expectTrue("Next window runs normally", next->run() == 0);
        worker.join();
        expectTrue("Next window waits for asynchronous work", completed);
    }
}

// The timeout is only a failure escape hatch: a broken loop must not hang the
// native suite. Every window still gets destroyed on its owning thread.
template <typename Body>
void expectWorkerFinishes(const char* name, Body body) {
    std::promise<DWORD> threadId;
    std::promise<bool> result;
    auto id = threadId.get_future();
    auto done = result.get_future();
    std::thread worker([&] {
        auto window = hiddenWindow();
        threadId.set_value(GetCurrentThreadId());
        result.set_value(body(*window));
    });
    const DWORD owner = id.get();
    if (done.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
        expectTrue(name, false);
        PostThreadMessageW(owner, WM_QUIT, 1, 0);
    }
    expectTrue(name, done.get());
    worker.join();
}

void testOtherThreadDoesNotPreventExit() {
    auto otherThreadWindow = hiddenWindow();
    expectWorkerFinishes("Another UI thread does not prevent loop exit", [](oneui::Window& window) {
        bool closed = false;
        window.post([&] { closed = true; window.close(); });
        return window.run() == 0 && closed;
    });
}

void testAlreadyClosedWindowReturns() {
    expectWorkerFinishes("Running an already closed window returns", [](oneui::Window& window) {
        window.close();
        return window.run() == 0;
    });
}

void testNestedLoopPropagatesQuit() {
    expectWorkerFinishes("Nested loop preserves the outer exit request", [](oneui::Window& outer) {
        int innerCode = -1;
        outer.post([&] {
            auto inner = hiddenWindow();
            inner->post([&] { outer.close(); inner->close(); });
            innerCode = inner->run();
        });
        return outer.run() == 0 && innerCode == 0;
    });
}

void testExplicitQuitIsPreserved() {
    auto window = hiddenWindow();
    PostQuitMessage(23);
    expectTrue("Explicit application quit code is preserved", window->run() == 23);
    window->close();
    MSG message{};
    expectTrue("Destroy after loop exit does not leave another quit",
               PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) == FALSE);
}

} // namespace

int main() {
    testDestroyWithoutRunDoesNotQuitNextWindow();
    testOtherThreadDoesNotPreventExit();
    testAlreadyClosedWindowReturns();
    testNestedLoopPropagatesQuit();
    testExplicitQuitIsPreserved();
    return failures == 0 ? 0 : 1;
}
