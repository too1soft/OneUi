#include "oneui/platform/window.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/view.h"
#include <windows.h>
#include <windowsx.h>
#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void check(bool value, const char* name) {
    std::cout << (value ? "PASS " : "FAIL ") << name << '\n';
    if (!value) ++failures;
}

class Target final : public oneui::Widget {
public:
    int downs = 0, ups = 0, keys = 0;
    std::function<void()> onActivate;
    mutable int tooltipQueries = 0;
    void paint(oneui::Canvas&) override {}
    bool isFocusable() const override { return true; }
    bool onMouseDown(const oneui::MouseEvent&) override { ++downs; return true; }
    bool onMouseUp(const oneui::MouseEvent&) override {
        ++ups;
        if (onActivate) onActivate();
        return true;
    }
    bool onMouseMove(const oneui::MouseEvent&) override { return false; }
    bool onKeyDown(const oneui::KeyEvent&) override { ++keys; return true; }
    oneui::CursorKind cursor(oneui::Point) const override { return oneui::CursorKind::Pointer; }
    const std::wstring* tooltipAt(oneui::Point) const override {
        ++tooltipQueries;
        static const std::wstring text = L"Geometry target";
        return &text;
    }
};

class GeometryRoot final : public oneui::View {
public:
    std::shared_ptr<Target> nav = std::make_shared<Target>();
    std::shared_ptr<Target> action;
    int layouts = 0, paints = 0;
    float shift = 0;
    GeometryRoot() { replaceAction(); }
    void replaceAction() {
        action = std::make_shared<Target>();
        // A newly mounted widget can still carry its previous/default box.
        // Layout must move it away from the navigation lane before hitTest.
        action->setFrame({0, 0, 100, 100});
        reconcileChildren({nav, action});
    }
    void changeGeometry(float value) { shift = value; invalidate(); }
    void paint(oneui::Canvas& canvas) override { ++paints; View::paint(canvas); }
protected:
    void layoutChildren() override {
        ++layouts;
        const auto r = frame();
        nav->setFrame({0, 0, 100, r.height});
        action->setFrame({r.width - 120 - shift, 40, 100, 50});
    }
};

struct Fixture {
    std::unique_ptr<oneui::Window> window;
    std::shared_ptr<GeometryRoot> root = std::make_shared<GeometryRoot>();
    HWND hwnd;
    Fixture() {
        oneui::WindowOptions options;
        options.visible = false;
        options.width = 640;
        options.height = 360;
        window = oneui::Window::create(options);
        window->initialize();
        window->setContent(root);
        hwnd = static_cast<HWND>(window->nativeHandle());
    }
    LPARAM point(float x, float y) const {
        return MAKELPARAM(static_cast<int>(std::lround(x * window->dpiScale())),
            static_cast<int>(std::lround(y * window->dpiScale())));
    }
    void click(float x, float y) {
        SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, point(x, y));
        SendMessageW(hwnd, WM_LBUTTONUP, 0, point(x, y));
    }
    float actionX() const { return window->clientSize().width - 70 - root->shift; }
    void establishOldGeometry() {
        const auto size = window->clientSize();
        root->setFrame({0, 0, size.width, size.height});
        root->layoutSubtree();
    }
};

void unpaintedAndReplacedTrees() {
    Fixture f;
    f.click(40, 60);
    check(f.root->nav->downs == 1 && f.root->action->downs == 0,
        "first input resolves the unpainted tree before hitTest");
    f.establishOldGeometry();
    f.root->replaceAction();
    const int navBefore = f.root->nav->downs;
    f.click(40, 60);
    check(f.root->nav->downs == navBefore + 1 && f.root->action->downs == 0,
        "retained root replacement cannot hit stale child geometry");
    check(f.root->paints == 0, "pointer geometry preparation does not paint");
    f.root = std::make_shared<GeometryRoot>();
    f.window->setContent(f.root);
    f.click(40, 60);
    check(f.root->nav->downs == 1 && f.root->action->downs == 0,
        "root identity replacement resolves geometry before hitTest");
}

void deferredMount() {
    Fixture f;
    f.establishOldGeometry();
    bool ran = false, correct = false;
    f.window->post([&] {
        f.root->replaceAction();
        f.click(40, 60);
        correct = f.root->nav->downs == 1 && f.root->action->downs == 0;
        ran = true;
        f.window->close();
    });
    const int result = f.window->run();
    check(result == 0 && ran && correct,
        "deferred mount routes the next input without an intermediate paint");
}

void resizeAndCapture() {
    Fixture f;
    f.establishOldGeometry();
    RECT outer{};
    GetWindowRect(f.hwnd, &outer);
    SetWindowPos(f.hwnd, nullptr, 0, 0, outer.right - outer.left + 160,
        outer.bottom - outer.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    f.click(f.actionX(), 60);
    check(f.root->action->downs == 1 && f.root->action->ups == 1,
        "resize geometry is current before the next input");
    // Isolate capture checks from a preceding resize regression.
    f.establishOldGeometry();
    const int downsBefore = f.root->action->downs, upsBefore = f.root->action->ups;
    SendMessageW(f.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, f.point(f.actionX(), 60));
    f.root->changeGeometry(150);
    SendMessageW(f.hwnd, WM_LBUTTONUP, 0, f.point(40, 60));
    check(f.root->action->downs == downsBefore + 1 && f.root->action->ups == upsBefore + 1 && f.root->nav->ups == 0,
        "layout changes preserve the captured down/up recipient");
    const auto retired = f.root->action;
    SendMessageW(f.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, f.point(f.actionX(), 60));
    f.root->replaceAction();
    SendMessageW(f.hwnd, WM_LBUTTONUP, 0, f.point(40, 60));
    check(retired->ups == upsBefore + 1 && f.root->action->ups == 0,
        "removed captured widgets do not receive the later mouse up");
    check(f.root->paints == 0, "resize and capture preparation do not paint");
}

void hoverAndFocus() {
    Fixture f;
    const HCURSOR previous = GetCursor();
    SendMessageW(f.hwnd, WM_MOUSEMOVE, 0, f.point(f.actionX(), 60));
    check(f.root->action->tooltipQueries > 0 && GetCursor() == LoadCursor(nullptr, IDC_HAND),
        "tooltip and cursor lookup use current unpainted geometry");
    const int layouts = f.root->layouts;
    for (int i = 0; i < 12; ++i)
        SendMessageW(f.hwnd, WM_MOUSEMOVE, 0, f.point(f.actionX() + i, 60));
    check(f.root->layouts == layouts,
        "unchanged hover does not repeat subtree layout");
    f.root->replaceAction();
    check(f.window->requestFocus(f.root->action.get()), "unpainted replacement can receive focus");
    const float expected = f.window->clientSize().width - 120;
    check(std::abs(f.root->action->frame().x - expected) < .1f,
        "programmatic focus resolves the replacement geometry");
    SendMessageW(f.hwnd, WM_KEYDOWN, VK_F24, 0);
    check(f.root->action->keys == 1, "keyboard focus survives geometry preparation");
    check(f.root->paints == 0, "hover and focus preparation do not paint");
    SetCursor(previous);
}

void layoutObserversCanReplaceOrDestroyTheWindow() {
    Fixture replaced;
    const auto replacement = std::make_shared<GeometryRoot>();
    replaced.root->setOnSizeChanged([&](oneui::Size) {
        replaced.window->setContent(replacement);
    });
    replaced.click(40, 60);
    check(replacement->nav->downs == 1 && replacement->action->downs == 0,
        "root replacement during layout routes input to the current tree");

    Fixture destroyed;
    destroyed.root->setOnSizeChanged([&](oneui::Size) { destroyed.window.reset(); });
    SendMessageW(destroyed.hwnd, WM_MOUSEMOVE, 0, destroyed.point(40, 60));
    check(!destroyed.window && destroyed.root->nav->downs == 0,
        "a layout size observer can destroy its window safely");
}

void overlayOwnedTreesAndQueuedNavigation() {
    Fixture f;
    f.establishOldGeometry();
    auto host = std::make_shared<oneui::OverlayHost>();
    host->setContent(f.root);
    f.window->setContent(host);
    f.root->replaceAction();
    f.click(40, 60);
    check(f.root->nav->downs == 1 && f.root->action->downs == 0,
        "OverlayHost content owned outside children has current hit geometry");

    Fixture overlay;
    auto layers = std::make_shared<oneui::OverlayHost>();
    layers->addAnchoredOverlay(overlay.root, oneui::OverlayOptions::modeless(), {-1, -1}, {}, 0, 0);
    overlay.window->setContent(layers);
    overlay.click(40, 60);
    check(overlay.root->nav->downs == 1 && overlay.root->action->downs == 0,
        "anchored overlay descendants are laid out before hitTest");
    f.window->close();
    overlay.window->close();

    Fixture queued;
    auto outer = std::make_shared<oneui::OverlayHost>();
    outer->setContent(queued.root);
    queued.window->setContent(outer);
    queued.establishOldGeometry();
    queued.root->nav->onActivate = [&] {
        queued.root->replaceAction();
        if (queued.root->nav->ups != 1) return;
        const LPARAM location = queued.point(40, 60);
        PostMessageW(queued.hwnd, WM_MOUSEMOVE, 0, location);
        PostMessageW(queued.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, location);
        PostMessageW(queued.hwnd, WM_LBUTTONUP, 0, location);
        PostMessageW(queued.hwnd, WM_CLOSE, 0, 0);
    };
    queued.click(40, 60);
    const int result = queued.window->run();
    check(result == 0 && queued.root->nav->downs == 2 && queued.root->action->downs == 0,
        "queued MOVE/DOWN/UP after route mutation cannot activate stale content");
}
} // namespace

int main() {
    unpaintedAndReplacedTrees();
    deferredMount();
    resizeAndCapture();
    hoverAndFocus();
    layoutObserversCanReplaceOrDestroyTheWindow();
    overlayOwnedTreesAndQueuedNavigation();
    return failures ? 1 : 0;
}
