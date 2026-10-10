#include "oneui/platform/window.h"
#include "oneui/controls/button.h"
#include "oneui/view.h"
#include "support/recording_canvas.h"

#include <windows.h>
#include <gl/GL.h>
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

void testSettingChangeHasDefinedResult() {
    auto window = hiddenWindow();
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    expectTrue("Settings change returns the default window result",
        SendMessageW(hwnd, WM_SETTINGCHANGE, SPI_SETCLIENTAREAANIMATION, 0) == 0);
    bool completed = false;
    window->post([&] { completed = true; window->close(); });
    expectTrue("Settings change leaves the message loop usable", window->run() == 0 && completed);
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

void testSecondaryWindowAnimation() {
    expectWorkerFinishes("Secondary window animates and survives primary close", [](oneui::Window& primary) {
        auto secondary = hiddenWindow();
        int frames = 0;
        std::function<void(double)> next;
        next = [&](double) {
            ++frames;
            if (frames == 2) primary.close();
            if (frames == 4) secondary->close();
            else secondary->requestAnimationFrame(next);
        };
        secondary->requestAnimationFrame(next);
        return primary.run() == 0 && frames == 4;
    });
}

void testCloseRestoresOtherWindowsGpuContext() {
    auto primary = hiddenWindow();
    primary->show();
    const auto primaryContext = wglGetCurrentContext();
    const auto primaryDC = wglGetCurrentDC();
    auto secondary = hiddenWindow();
    secondary->show();
    if (primaryContext && wglGetCurrentContext()) {
        wglMakeCurrent(primaryDC, primaryContext);
        secondary->close();
        expectTrue("Closing secondary preserves primary GL context",
                   wglGetCurrentContext() == primaryContext);
    } else {
        secondary->close();
    }
    primary->close();
}

void testPreMountStyleAnimationResumes() {
    expectWorkerFinishes("Pre-mount style animation reaches its target", [](oneui::Window& window) {
        oneui::setMotionEnabled(true);
        auto root = std::make_shared<oneui::View>();
        auto button = std::make_shared<oneui::Button>(L"pre-mount");
        root->add(button);
        button->setFrame({0, 0, 100, 32});
        oneui::ButtonStyleOverride style;
        style.normal.emplace();
        style.normal->border = oneui::Color{198, 208, 200};
        style.normal->transition = oneui::TransitionSpec{100};
        // The child can schedule through its parent, but the parent has no
        // window yet. That request cannot reach the native frame pump.
        button->setStyleOverride(style);
        window.setContent(root);
        const auto start = std::chrono::steady_clock::now();
        bool settled = false;
        std::function<void(double)> next;
        next = [&](double) {
            if (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(350)) {
                window.requestAnimationFrame(next);
                return;
            }
            oneui::test_support::RecordingCanvas canvas;
            button->paint(canvas);
            const auto color = canvas.strokeRects.front().color;
            settled = color.r == 198 && color.g == 208 && color.b == 200;
            window.close();
        };
        window.requestAnimationFrame(next);
        return window.run() == 0 && settled;
    });
}

} // namespace

int main() {
    testDestroyWithoutRunDoesNotQuitNextWindow();
    testSettingChangeHasDefinedResult();
    testOtherThreadDoesNotPreventExit();
    testAlreadyClosedWindowReturns();
    testNestedLoopPropagatesQuit();
    testExplicitQuitIsPreserved();
    testSecondaryWindowAnimation();
    testCloseRestoresOtherWindowsGpuContext();
    testPreMountStyleAnimationResumes();
    return failures == 0 ? 0 : 1;
}
