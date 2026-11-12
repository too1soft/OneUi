#include "oneui/platform/window.h"
#include "oneui/controls/button.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/layout/panel.h"
#include "oneui/view.h"
#include "support/recording_canvas.h"

#include <windows.h>
#include <windowsx.h>
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

class PaintCounterSurface final : public oneui::Widget {
public:
    unsigned int paints = 0;
    void paint(oneui::Canvas&) override { ++paints; }
    bool onMouseMove(const oneui::MouseEvent&) override { invalidate(); return true; }
    bool onMouseDown(const oneui::MouseEvent&) override { invalidate(); return true; }
    bool onMouseUp(const oneui::MouseEvent&) override { invalidate(); return true; }
};

void testSettingChangeHasDefinedResult() {
    auto window = hiddenWindow();
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    expectTrue("Settings change returns the default window result",
        SendMessageW(hwnd, WM_SETTINGCHANGE, SPI_SETCLIENTAREAANIMATION, 0) == 0);
    bool completed = false;
    window->post([&] { completed = true; window->close(); });
    expectTrue("Settings change leaves the message loop usable", window->run() == 0 && completed);
}

void testPointerRestoresAfterTextInputAndContentChanges() {
    class CursorSurface final : public oneui::Widget {
    public:
        oneui::CursorKind kind = oneui::CursorKind::Text;
        void paint(oneui::Canvas&) override {}
        oneui::CursorKind cursor(oneui::Point) const override { return kind; }
    };
    auto window = hiddenWindow();
    auto surface = std::make_shared<CursorSurface>();
    window->setContent(surface);
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    const HCURSOR previous = GetCursor();
    const HCURSOR text = LoadCursor(nullptr, IDC_IBEAM);
    SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(12, 12));
    expectTrue("Terminal starts with a text pointer", GetCursor() == text);
    SetCursor(nullptr); // An input method can clear the OS cursor independently.
    SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(13, 12));
    expectTrue("Moving inside the same terminal restores the pointer", GetCursor() == text);

    POINT point{};
    GetCursorPos(&point);
    ScreenToClient(hwnd, &point);
    surface->kind = oneui::CursorKind::Hidden;
    SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(point.x, point.y));
    expectTrue("A remote surface can deliberately hide its pointer", GetCursor() == nullptr);
    surface->kind = oneui::CursorKind::Text;
    SendMessageW(hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    expectTrue("Changed content restores a stationary pointer", GetCursor() == text);
    SetCursor(previous);
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

void testPostedCallbackFloodYieldsToFrames() {
    expectWorkerFinishes("Posted callback flood yields to animation", [](oneui::Window& window) {
        unsigned int completed = 0;
        unsigned int observed = 128;
        bool ordered = true;
        for (unsigned int index = 0; index < 128; ++index) {
            window.post([&, index] {
                ordered = ordered && completed == index;
                ++completed;
                Sleep(2);
            });
        }
        window.requestAnimationFrame([&](double) {
            observed = completed;
            window.close();
        });
        return window.run() == 0 && ordered && observed > 0 && observed < 128;
    });
}

void testPostedCallbacksPreserveFifoAcrossYield() {
    expectWorkerFinishes("Posted callbacks retain FIFO after yielding", [](oneui::Window& window) {
        unsigned int completed = 0;
        bool ordered = true;
        for (unsigned int index = 0; index < 96; ++index) {
            window.post([&, index] {
                ordered = ordered && completed++ == index;
                if (index == 0) window.post([&] {
                    ordered = ordered && completed == 96;
                    window.close();
                });
            });
        }
        return window.run() == 0 && ordered && completed == 96;
    });
}

void testPostedCallbackCanWakeNestedLoop() {
    expectWorkerFinishes("Posted work wakes a nested native loop", [](oneui::Window& window) {
        bool completed = false;
        window.post([&] {
            auto nested = hiddenWindow();
            window.post([&] { completed = true; nested->close(); window.close(); });
            nested->run();
            window.close();
        });
        return window.run() == 0 && completed;
    });
}

void testNestedLoopDoesNotReenterOuterPostedBatch() {
    expectWorkerFinishes("Nested loop retains the outer posted batch", [](oneui::Window& window) {
        std::vector<int> order;
        window.post([&] {
            order.push_back(1);
            auto nested = hiddenWindow();
            bool nestedDone = false;
            window.post([&] { order.push_back(2); nestedDone = true; nested->close(); });
            // Model a native modal loop without asking the shared OneUI loop
            // to quit (that would legitimately cancel a yielded outer batch).
            MSG message{};
            while (!nestedDone && GetMessageW(&message, nullptr, 0, 0) > 0) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            order.push_back(3);
        });
        window.post([&] { order.push_back(4); window.close(); });
        return window.run() == 0 && order == std::vector<int>({1,2,3,4});
    });
}

void testPostedCallbackFloodDoesNotStarveStaticPaint() {
    expectWorkerFinishes("Posted callback flood reveals static content", [](oneui::Window& window) {
        auto surface = std::make_shared<PaintCounterSurface>();
        window.setContent(surface);
        window.show();
        unsigned int before = 0;
        bool paintedBeforeEnd = false;
        // Consume the initial content tick before starting work. There is no
        // animation to rescue WM_PAINT from a perpetually nonempty post queue.
        window.requestAnimationFrame([&](double) {
            for (unsigned int index = 0; index < 80; ++index) window.post([&, index] {
                if (index == 0) { before = surface->paints; window.requestRedraw(); }
                else paintedBeforeEnd = paintedBeforeEnd || surface->paints > before;
                if (index == 79) window.close();
            });
        });
        return window.run() == 0 && paintedBeforeEnd;
    });
}

void testPointerBurstPaintsOnceAfterDispatch() {
    expectWorkerFinishes("Pointer burst coalesces paint outside input dispatch", [](oneui::Window& window) {
        auto surface = std::make_shared<PaintCounterSurface>();
        window.setContent(surface);
        window.show();
        const auto hwnd = static_cast<HWND>(window.nativeHandle());
        SendMessageW(hwnd, WM_PAINT, 0, 0);
        const unsigned int before = surface->paints;
        for (unsigned int index = 0; index < 20; ++index)
            SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(20 + index, 20));
        SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(30, 20));
        SendMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(30, 20));
        const bool deferred = surface->paints == before;
        unsigned int after = before;
        window.requestAnimationFrame([&](double) {
            window.post([&] { after = surface->paints; window.close(); });
        });
        return window.run() == 0 && deferred && after == before + 1;
    });
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

void testBorderlessCaptionRoutesControlsAndKeepsSingleClickState() {
    oneui::WindowOptions options;
    options.visible = false;
    options.borderless = true;
    options.width = 800;
    options.height = 600;
    auto window = oneui::Window::create(options);
    window->initialize();
    window->setTitleBarDragMetrics(40, 138);
    auto root = std::make_shared<oneui::OverlayHost>();
    auto bar = std::make_shared<oneui::View>();
    auto search = std::make_shared<oneui::Button>(L"Search");
    auto account = std::make_shared<oneui::Button>(L"Account");
    bar->add(search);
    bar->add(account);
    root->setContent(bar);
    window->setContent(root);
    root->setFrame({0, 0, 800, 600});
    bar->setFrame(root->frame());
    search->setFrame({300, 6, 200, 28});
    account->setFrame({610, 4, 32, 32});
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    auto point = [&](int x) {
        POINT p{static_cast<LONG>(x * window->dpiScale()),
                static_cast<LONG>(20 * window->dpiScale())};
        ClientToScreen(hwnd, &p);
        return MAKELPARAM(p.x, p.y);
    };
    expectTrue("nested search remains a client control",
        SendMessageW(hwnd, WM_NCHITTEST, 0, point(400)) == HTCLIENT);
    expectTrue("account before window buttons remains client",
        SendMessageW(hwnd, WM_NCHITTEST, 0, point(625)) == HTCLIENT);
    expectTrue("blank caption still belongs to native drag",
        SendMessageW(hwnd, WM_NCHITTEST, 0, point(120)) == HTCAPTION);
    auto welcome = std::make_shared<oneui::Panel>();
    welcome->setFrame({120, 100, 560, 420});
    root->addOverlay(welcome, oneui::OverlayOptions::modal(100));
    expectTrue("welcome modal keeps native caption drag available",
        SendMessageW(hwnd, WM_NCHITTEST, 0, point(120)) == HTCAPTION);
    expectTrue("modal does not turn the search control into a drag lane",
        SendMessageW(hwnd, WM_NCHITTEST, 0, point(400)) == HTCLIENT);
    int backgroundClicks = 0;
    search->setOnClick([&] { ++backgroundClicks; });
    root->onMouseDown(oneui::MouseEvent{oneui::Point{400, 20}});
    root->onMouseUp(oneui::MouseEvent{oneui::Point{400, 20}});
    expectTrue("welcome still blocks background control input", backgroundClicks == 0);
    root->removeOverlay(welcome.get());
    for (bool maximized : {true, false}) {
        SendMessageW(hwnd, WM_NCLBUTTONDBLCLK, HTCAPTION, point(120));
        expectTrue("double click toggles exactly once", window->state().maximized == maximized);
        // Feed the native threshold detector a completed stationary click;
        // this sends messages only to this hidden test window, no OS input.
        PostMessageW(hwnd, WM_LBUTTONUP, 0, 0);
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, point(120));
        expectTrue("following single click preserves window state", window->state().maximized == maximized);
    }
    window->close();
}

} // namespace

int main() {
    testPointerRestoresAfterTextInputAndContentChanges();
    testDestroyWithoutRunDoesNotQuitNextWindow();
    testSettingChangeHasDefinedResult();
    testOtherThreadDoesNotPreventExit();
    testAlreadyClosedWindowReturns();
    testNestedLoopPropagatesQuit();
    testExplicitQuitIsPreserved();
    testSecondaryWindowAnimation();
    testPostedCallbackFloodYieldsToFrames();
    testPostedCallbacksPreserveFifoAcrossYield();
    testPostedCallbackCanWakeNestedLoop();
    testNestedLoopDoesNotReenterOuterPostedBatch();
    testPostedCallbackFloodDoesNotStarveStaticPaint();
    testPointerBurstPaintsOnceAfterDispatch();
    testCloseRestoresOtherWindowsGpuContext();
    testPreMountStyleAnimationResumes();
    testBorderlessCaptionRoutesControlsAndKeepsSingleClickState();
    return failures == 0 ? 0 : 1;
}
