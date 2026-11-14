#include "oneui/platform/window.h"
#include <windows.h>
#include <iostream>

namespace {
int failures = 0;

void check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL " << message << '\n';
        ++failures;
    }
}

class InputSurface final : public oneui::Widget {
public:
    bool clientPreedit = false;
    int keys = 0;
    int compositionClears = 0;
    void paint(oneui::Canvas&) override {}
    bool onKeyDown(const oneui::KeyEvent&) override { ++keys; return true; }
    oneui::TextInputState textInputState() const override {
        return {{}, 0, 0, true, false, this, clientPreedit, 1};
    }
    void setTextComposition(std::wstring text, std::size_t) override {
        if (text.empty()) ++compositionClears;
    }
};

void sendKeys(HWND hwnd) {
    for (const WPARAM key : {VK_TAB, VK_RETURN}) {
        SendMessageW(hwnd, WM_KEYDOWN, key, 0);
        SendMessageW(hwnd, WM_KEYUP, key, 0);
    }
}

void cancellationRestoresKeys(bool clientPreedit, UINT message, WPARAM value) {
    oneui::WindowOptions options;
    options.visible = false;
    auto window = oneui::Window::create(options);
    window->initialize();
    auto surface = std::make_shared<InputSurface>();
    surface->clientPreedit = clientPreedit;
    window->setContent(surface);
    const auto hwnd = static_cast<HWND>(window->nativeHandle());

    sendKeys(hwnd);
    check(surface->keys == 2, "ordinary Tab and Enter reach the input surface");
    SendMessageW(hwnd, WM_IME_STARTCOMPOSITION, 0, 0);
    sendKeys(hwnd);
    check(surface->keys == 2, "active composition owns Tab and Enter");

    SendMessageW(hwnd, message, value, 0);
    sendKeys(hwnd);
    check(surface->keys == 4, "cancelled composition releases Tab and Enter without a later END");
    check(surface->compositionClears > 0, "cancelled composition clears client preedit");

    const int keysBeforeEnd = surface->keys;
    SendMessageW(hwnd, WM_IME_ENDCOMPOSITION, 0, 0);
    sendKeys(hwnd);
    check(surface->keys == keysBeforeEnd + 2, "a delayed duplicate END preserves ordinary input");
    window->close();
}
} // namespace

int main() {
    for (bool clientPreedit : {false, true}) {
        cancellationRestoresKeys(clientPreedit, WM_IME_COMPOSITION, 0);
        cancellationRestoresKeys(clientPreedit, WM_IME_SETCONTEXT, FALSE);
    }
    return failures == 0 ? 0 : 1;
}
