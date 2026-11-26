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
    std::vector<int> steps;
    int accepted = 0;
    int cancelled = 0;
    bool closeDuringClick = false;
    bool nestClick = false;
    decltype(&oneui_window_post_owned) post = oneui_window_post_after_pointer_release_owned;

    Fixture() {
        require(window && button, "fixture initialization failed");
        oneui_window_set_content(window.get(), button.get());
        oneui_window_initialize(window.get());
        oneui_button_set_on_click(button.get(), clicked, this);
    }

    ~Fixture() { window.reset(); }

    static void cleanup(void* data) { ++static_cast<Fixture*>(data)->cancelled; }

    static void clicked(void* data) {
        auto& fixture = *static_cast<Fixture*>(data);
        fixture.steps.push_back(1);
        fixture.accepted = fixture.post(fixture.window.get(), [](void* owned) {
            static_cast<Fixture*>(owned)->steps.push_back(3);
        }, data, cleanup);
        if (std::exchange(fixture.nestClick, false)) fixture.click();
        fixture.steps.push_back(2);
        if (fixture.closeDuringClick) oneui_window_close(fixture.window.get());
    }

    void click() const {
        const auto hwnd = static_cast<HWND>(oneui_window_native_handle(window.get()));
        require(hwnd != nullptr, "fixture has no native window");
        const auto point = MAKELPARAM(20, 20);
        SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, point);
        SendMessageW(hwnd, WM_LBUTTONUP, 0, point);
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

} // namespace

int main(int argc, char** argv) {
    try {
        verifyReleaseOrder(argc == 2 && std::string_view(argv[1]) == "--ordinary-post");
        verifyCloseDuringRelease();
        verifyNestedRelease();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
