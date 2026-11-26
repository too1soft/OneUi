#include "oneui/oneui_c_api.h"

#include <windows.h>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include <iostream>

namespace {

void require(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}

struct Fixture {
    using Window = std::unique_ptr<OneUiWindow, decltype(&oneui_window_destroy)>;
    using Button = std::unique_ptr<OneUiWidget, decltype(&oneui_widget_destroy)>;

    static OneUiWindow* createWindow() {
        OneUiWindowOptionsUtf8 options{};
        options.width = 320;
        options.height = 240;
        return oneui_window_create_utf8(&options);
    }

    Window window{createWindow(), oneui_window_destroy};
    Button button{oneui_button_create(L"Navigate"), oneui_widget_destroy};
    Button scrollView{nullptr, oneui_widget_destroy};
    std::vector<int> steps;
    int accepted = 0;
    int cancelled = 0;
    bool closeDuringClick = false;
    bool nestClick = false;
    bool captureDuringClick = false;
    decltype(&oneui_window_post_owned) post = oneui_window_post_after_pointer_release_owned;

    explicit Fixture(bool scroll = false) {
        require(window && button, "fixture initialization failed");
        if (scroll) {
            scrollView.reset(oneui_scroll_view_create());
            require(bool(scrollView), "fixture scroll view initialization failed");
            oneui_scroll_view_set_content_height(scrollView.get(), 220);
            oneui_scroll_view_set_content(scrollView.get(), button.get());
        }
        oneui_window_set_content(window.get(), scroll ? scrollView.get() : button.get());
        oneui_window_initialize(window.get());
        oneui_button_set_on_click(button.get(), clicked, this);
    }

    ~Fixture() { window.reset(); }

    static void cleanup(void* data) { ++static_cast<Fixture*>(data)->cancelled; }

    static void clicked(void* data) {
        auto& fixture = *static_cast<Fixture*>(data);
        fixture.captureDuringClick |= GetCapture() == fixture.nativeHandle();
        fixture.steps.push_back(1);
        fixture.accepted = fixture.post(fixture.window.get(), [](void* owned) {
            static_cast<Fixture*>(owned)->steps.push_back(3);
        }, data, cleanup);
        if (std::exchange(fixture.nestClick, false)) fixture.click();
        fixture.steps.push_back(2);
        if (fixture.closeDuringClick) oneui_window_close(fixture.window.get());
    }

    HWND nativeHandle() const {
        const auto hwnd = static_cast<HWND>(oneui_window_native_handle(window.get()));
        require(hwnd != nullptr, "fixture has no native window");
        return hwnd;
    }

    void press(LPARAM point = MAKELPARAM(20, 20)) const {
        SendMessageW(nativeHandle(), WM_LBUTTONDOWN, MK_LBUTTON, point);
    }

    void release(LPARAM point = MAKELPARAM(20, 20)) const {
        SendMessageW(nativeHandle(), WM_LBUTTONUP, 0, point);
    }

    void click() const {
        press();
        release();
    }
};

void verifyReleaseOrder(bool ordinaryPost) {
    Fixture fixture;
    if (ordinaryPost) fixture.post = oneui_window_post_owned;
    require(oneui_window_post_owned(fixture.window.get(), [](void* data) {
        static_cast<Fixture*>(data)->steps.push_back(4);
    }, &fixture, Fixture::cleanup) == 1, "ordinary callback was rejected");
    fixture.click();
    require(fixture.accepted == 1 && fixture.steps == std::vector<int>{1, 2, 3},
            "navigation must follow widget release before ordinary posted work");
    // Outside a pointer release the new entry point keeps FIFO ordering.
    require(fixture.post(fixture.window.get(), [](void* data) {
        auto& owner = *static_cast<Fixture*>(data);
        owner.steps.push_back(5);
        oneui_window_close(owner.window.get());
    }, &fixture, Fixture::cleanup) == 1, "fallback callback was rejected");
    require(fixture.steps == std::vector<int>{1, 2, 3}, "ordinary fallback ran inline");
    require(oneui_window_run(fixture.window.get()) == 0 && fixture.cancelled == 0 &&
            fixture.steps == std::vector<int>{1, 2, 3, 4, 5}, "ordinary FIFO changed");
}

void verifyCloseDuringRelease() {
    Fixture fixture;
    fixture.closeDuringClick = true;
    fixture.click();
    require(fixture.accepted == 1 && fixture.steps == std::vector<int>{1, 2} &&
            fixture.cancelled == 1, "closing during release must cancel exactly once");
}

void verifyNestedRelease() {
    Fixture fixture;
    fixture.nestClick = true;
    fixture.click();
    require(fixture.steps == std::vector<int>{1, 1, 2, 2, 3, 3},
            "nested release actions must wait for the outer widget callback");
}

void verifyLeaveDuringCapture() {
    Fixture fixture(true);
    const auto hwnd = fixture.nativeHandle();
    SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(20, 20));
    fixture.press();
    require(GetCapture() == hwnd, "press did not acquire Win32 capture");
    SendMessageW(hwnd, WM_MOUSELEAVE, 0, 0);
    require(fixture.steps.empty(), "mouse leave activated the button");
    fixture.release();
    require(fixture.steps == std::vector<int>{1, 2, 3},
            "mouse leave cancelled a captured press inside the button");
    require(!fixture.captureDuringClick && GetCapture() != hwnd,
            "ordinary release did not give up capture before the click");
}

void verifyOutsideReleaseAfterLeave() {
    Fixture fixture(true);
    const auto hwnd = fixture.nativeHandle();
    fixture.press();
    require(GetCapture() == hwnd, "press did not acquire Win32 capture");
    SendMessageW(hwnd, WM_MOUSELEAVE, 0, 0);
    fixture.release(MAKELPARAM(-10, -10));
    require(fixture.steps.empty() && GetCapture() != hwnd, "outside release activated the button");
    fixture.click();
    require(fixture.steps == std::vector<int>{1, 2, 3}, "outside release left stale press state");
}

void verifyCaptureTransferCancelsPress() {
    Fixture first(true);
    Fixture next;
    first.press();
    require(GetCapture() == first.nativeHandle(), "press did not acquire Win32 capture");
    SetCapture(next.nativeHandle());
    require(GetCapture() == next.nativeHandle(), "other window did not acquire Win32 capture");
    first.release();
    require(first.steps.empty(), "capture transfer did not cancel the former press");
    require(GetCapture() == next.nativeHandle(), "old release took capture from its new owner");
    ReleaseCapture();
    first.click();
    require(first.steps == std::vector<int>{1, 2, 3}, "click after capture transfer failed");
}

void verifyScrollbarKeepsCapturedDrag() {
    Fixture fixture(true);
    const auto hwnd = fixture.nativeHandle();
    oneui_scroll_view_set_content_height(fixture.scrollView.get(), 720);
    RECT client{};
    require(GetClientRect(hwnd, &client) != 0, "fixture client area is unavailable");
    const int inset = MulDiv(6, GetDpiForWindow(hwnd), 96);
    const int y = MulDiv(20, GetDpiForWindow(hwnd), 96);
    const auto thumb = MAKELPARAM(client.right - inset, y);
    const auto moved = MAKELPARAM(client.right - inset, y * 3);
    fixture.press(thumb);
    require(GetCapture() == hwnd, "thumb press did not acquire Win32 capture");
    require(oneui_scroll_view_scroll_offset(fixture.scrollView.get()) == 0,
            "fixture pressed the scrollbar track outside the thumb");
    SendMessageW(hwnd, WM_MOUSELEAVE, 0, 0);
    SendMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, moved);
    const float offset = oneui_scroll_view_scroll_offset(fixture.scrollView.get());
    require(offset > 1, "mouse leave cancelled the captured scrollbar drag");
    fixture.release(moved);
    SendMessageW(hwnd, WM_MOUSEMOVE, 0, thumb);
    require(oneui_scroll_view_scroll_offset(fixture.scrollView.get()) == offset,
            "scrollbar kept dragging after release");
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc == 2 ? argv[1] : "";
        if (mode == "--captured-leave") {
            verifyLeaveDuringCapture();
            return 0;
        }
        if (mode == "--captured-scroll") {
            verifyScrollbarKeepsCapturedDrag();
            return 0;
        }
        verifyReleaseOrder(mode == "--ordinary-post");
        verifyCloseDuringRelease();
        verifyNestedRelease();
        verifyLeaveDuringCapture();
        verifyOutsideReleaseAfterLeave();
        verifyCaptureTransferCancelsPress();
        verifyScrollbarKeepsCapturedDrag();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
