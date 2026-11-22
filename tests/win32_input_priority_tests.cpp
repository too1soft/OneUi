#include "oneui/platform/window.h"
#include "oneui/controls/text_field.h"
#include "oneui/widget.h"
#include <windows.h>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

namespace {
class InputSurface final : public oneui::Widget {
public:
    oneui::Window* window = nullptr;
    bool typed = false, painted = false, paintedBeforeWork = false;
    bool clickReceived = false, clickCommitted = false, paintedOldClickState = false;
    std::vector<int> order;
    void paint(oneui::Canvas&) override {
        if (typed) painted = true;
        if (clickReceived && !clickCommitted) paintedOldClickState = true;
    }
    bool isFocusable() const override { return true; }
    bool onMouseUp(const oneui::MouseEvent&) override {
        clickReceived = true;
        invalidate();
        window->post([this] {
            clickCommitted = true;
            const auto hwnd = static_cast<HWND>(window->nativeHandle());
            PostMessageW(hwnd, WM_CHAR, L'x', 0);
        });
        return true;
    }
    bool onTextInput(wchar_t) override {
        if (typed) return true;
        typed = true;
        invalidate();
        // Model updates queued by an edit must not hide the edit's own paint.
        for (int i = 0; i < 3; ++i) window->post([this, i] {
            if (i == 0) paintedBeforeWork = painted;
            order.push_back(i);
            if (i == 0) Sleep(20); // Existing single callbacks are not preemptible.
            if (i == 2) window->close();
        });
        return true;
    }
};

class CoalescedField final : public oneui::TextField {
public:
    oneui::Window* window = nullptr;
    bool modelApplied = false;
    bool paintedEarly = false;
    bool paintedTogether = false;

    void applyModel() {
        modelApplied = true;
        invalidate();
    }

    void paint(oneui::Canvas&) override {
        if (text().empty()) return;
        if (!modelApplied) paintedEarly = true;
        else if (!paintedTogether) {
            paintedTogether = true;
            window->post([this] { window->close(); });
        }
    }
};

int runWithWatchdog(oneui::Window& window, HWND hwnd) {
    std::atomic_bool finished{false};
    std::thread watchdog([&] {
        for (int i = 0; i < 200 && !finished; ++i) Sleep(10);
        if (!finished) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == GetCurrentProcessId()) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
    });
    const int result = window.run();
    finished = true;
    watchdog.join();
    return result;
}

int testDefault() {
    oneui::WindowOptions options;
    options.title = L"OneUI input priority contract";
    options.width = 400;
    options.height = 240;
    options.visible = true;
    auto window = oneui::Window::create(options);
    window->initialize();
    auto surface = std::make_shared<InputSurface>();
    surface->window = window.get();
    window->setContent(surface);
    window->show();
    if (!window->requestFocus(surface.get())) return 2;
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    window->post([hwnd] { PostMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(20, 20)); });
    const int result = runWithWatchdog(*window, hwnd);
    const bool fifo = surface->order == std::vector<int>{0, 1, 2};
    const bool clickBeforePaint = surface->clickCommitted && !surface->paintedOldClickState;
    std::cout << "inputPaintBeforeQueuedWork=" << surface->paintedBeforeWork
              << " clickCommittedBeforePaint=" << clickBeforePaint
              << " callbacksInOrder=" << fifo << " loopResult=" << result << '\n';
    return result == 0 && fifo && clickBeforePaint && surface->paintedBeforeWork ? 0 : 1;
}

bool testCoalesced() {
    oneui::WindowOptions options;
    options.title = L"OneUI coalesced edit contract";
    options.width = 400;
    options.height = 240;
    options.visible = true;
    auto window = oneui::Window::create(options);
    window->initialize();
    auto field = std::make_shared<CoalescedField>();
    field->window = window.get();
    field->setCoalesceEditPaint(true);
    field->setOnChanged([owner = window.get(), target = field.get()](const std::wstring&) {
        owner->post([target] { target->applyModel(); });
    });
    window->setContent(field);
    window->show();
    if (!window->requestFocus(field.get())) return false;
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    // Drive the initial presentation before sending the edit, so a pending
    // first-show WM_PAINT cannot observe the edit outside its normal path.
    window->requestAnimationFrame([hwnd](double) { PostMessageW(hwnd, WM_CHAR, L'x', 0); });
    const int result = runWithWatchdog(*window, hwnd);
    std::cout << "coalescedEditAndModelPaint=" << field->paintedTogether
              << " earlyEditPaint=" << field->paintedEarly << '\n';
    return result == 0 && field->paintedTogether && !field->paintedEarly;
}
} // namespace

int main() {
    const int normal = testDefault();
    if (normal != 0) return normal;
    return testCoalesced() ? 0 : 1;
}
