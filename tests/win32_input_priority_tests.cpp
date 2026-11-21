#include "oneui/platform/window.h"
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
    std::vector<int> order;
    void paint(oneui::Canvas&) override { if (typed) painted = true; }
    bool isFocusable() const override { return true; }
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
}

int main() {
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
    window->post([hwnd] { PostMessageW(hwnd, WM_CHAR, L'x', 0); });
    std::atomic_bool finished{false};
    std::thread watchdog([&] {
        for (int i = 0; i < 200 && !finished; ++i) Sleep(10);
        if (!finished) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == GetCurrentProcessId()) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
    });
    const int result = window->run();
    finished = true;
    watchdog.join();
    const bool fifo = surface->order == std::vector<int>{0, 1, 2};
    std::cout << "inputPaintBeforeQueuedWork=" << surface->paintedBeforeWork
              << " callbacksInOrder=" << fifo << " loopResult=" << result << '\n';
    return result == 0 && fifo && surface->paintedBeforeWork ? 0 : 1;
}
