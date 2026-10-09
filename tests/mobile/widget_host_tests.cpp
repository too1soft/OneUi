#include "oneui/controls/button.h"
#include "oneui/layout/panel.h"
#include "oneui/layout/scroll_view.h"
#include "oneui/layout/stack.h"
#include "oneui/mobile/widget_host.h"
#include "support/recording_canvas.h"
#include <iostream>
#include <stdexcept>

using namespace oneui;
namespace {
void require(bool value, const char *text) {
    if (!value)
        throw std::runtime_error(text);
}
void ready(mobile::WidgetHost &host) {
    host.runtime().setViewport({600, 900, 3, 0, 30, 0, 30, 0});
    host.runtime().setSurface(true);
    host.runtime().setLifecycle(ONEUI_MOBILE_ACTIVE);
}
void send(mobile::WidgetHost &host, int phase, int id = 5) {
    require(host.runtime().pushPointer({id, phase, ONEUI_MOBILE_TOUCH, 150, 150, 1, 1, 0}) ==
                ONEUI_MOBILE_OK,
            "pointer push");
    require(host.pumpInput() == ONEUI_MOBILE_OK, "input dispatch");
}
class Probe final : public Widget {
  public:
    int paints = 0;
    void paint(Canvas &) override { ++paints; }
    void dirty() { invalidate(); }
};
void retainedWidgetAndIdle() {
    auto child = std::make_shared<Probe>();
    test_support::RecordingCanvas canvas;
    {
        mobile::WidgetHost host;
        ready(host);
        require(host.setContent(child) == ONEUI_MOBILE_OK, "attach");
        require(child->hasCompositionOwner(), "composition ownership");
        host.paint(canvas, 1);
        require(child->frame().width == 200 && child->frame().height == 280 &&
                    child->frame().y == 10,
                "safe-area layout");
        // A resize may invalidate during the first paint; drain that one frame.
        host.paint(canvas, 2);
        const auto paints = child->paints;
        require(host.paint(canvas, 3) == ONEUI_MOBILE_EMPTY && child->paints == paints,
                "idle skips paint");
        child->dirty();
        require(host.paint(canvas, 4) == ONEUI_MOBILE_OK, "widget invalidation reaches host");
        mobile::WidgetHost second;
        require(second.setContent(child) == ONEUI_MOBILE_INVALID_STATE, "exclusive attachment");
        host.runtime().setSurface(false);
        host.pumpInput();
        host.runtime().setSurface(true);
        host.paint(canvas, 5);
        require(child->hasCompositionOwner(), "surface replacement retains content");
        host.runtime().setLifecycle(ONEUI_MOBILE_BACKGROUND);
        host.pumpInput();
        child->dirty();
        require(host.paint(canvas, 6) == ONEUI_MOBILE_EMPTY, "background skips paint");
    }
    require(!child->hasCompositionOwner(), "host releases callbacks");
    child->dirty(); // No dangling host callback after destruction.
    mobile::WidgetHost replacement;
    require(replacement.setContent(child) == ONEUI_MOBILE_OK, "same instance can be reattached");
}
void cancelledTapCannotClick() {
    mobile::WidgetHost host;
    ready(host);
    auto panel = std::make_shared<Panel>();
    auto button = std::make_shared<Button>(L"mobile");
    int clicks = 0;
    button->setOnClick([&] { ++clicks; });
    panel->setContent(button);
    host.setContent(panel);
    test_support::RecordingCanvas canvas;
    host.paint(canvas, 1);
    send(host, ONEUI_MOBILE_DOWN);
    host.runtime().setLifecycle(ONEUI_MOBILE_BACKGROUND);
    host.pumpInput();
    host.runtime().setLifecycle(ONEUI_MOBILE_ACTIVE);
    // Old touch-up cannot create a click after the OS cancelled the gesture.
    require(host.runtime().pushPointer({5, ONEUI_MOBILE_UP, ONEUI_MOBILE_TOUCH, 150, 150, 0, 2,
                                        0}) == ONEUI_MOBILE_INVALID_STATE,
            "stale up rejected");
    require(clicks == 0, "background cancellation never clicks");
    send(host, ONEUI_MOBILE_DOWN);
    send(host, ONEUI_MOBILE_UP);
    require(clicks == 1, "new gesture clicks once");
    send(host, ONEUI_MOBILE_DOWN);
    send(host, ONEUI_MOBILE_DOWN, 7);
    send(host, ONEUI_MOBILE_UP, 7);
    require(clicks == 1, "secondary finger cannot release primary press");
    host.runtime().cancelPointers();
    host.pumpInput();
    require(clicks == 1, "cancel doesn't synthesize mouse up");
}
void verticalPanCancelsTapAndKeepsHorizontalControl() {
    mobile::WidgetHost host;
    ready(host);
    auto content = std::make_shared<Stack>();
    content->setPreferredSize({200, 720});
    auto button = std::make_shared<Button>(L"tap or pan");
    auto lower = std::make_shared<Panel>();
    int clicks = 0;
    button->setOnClick([&] { ++clicks; });
    content->add(button); content->setFlex(button, {0, 0, 120, 120});
    content->add(lower); content->setFlex(lower, {0, 0, 600, 600});
    auto scroll = std::make_shared<ScrollView>();
    scroll->setContent(content);
    host.setContent(scroll); host.setTouchScrolling(true);
    test_support::RecordingCanvas canvas;
    host.paint(canvas, 1);
    uint64_t time = 1;
    auto pointer = [&](int phase, float x, float y) {
        require(host.runtime().pushPointer({3, phase, ONEUI_MOBILE_TOUCH,
            x * 3, y * 3, 1, time++, 0}) == ONEUI_MOBILE_OK, "pan pointer");
        require(host.pumpInput() == ONEUI_MOBILE_OK, "pan dispatch");
    };
    pointer(ONEUI_MOBILE_DOWN, 50, 80); pointer(ONEUI_MOBILE_UP, 50, 80);
    require(clicks == 1, "tap still clicks");
    pointer(ONEUI_MOBILE_DOWN, 50, 80); pointer(ONEUI_MOBILE_MOVE, 51, 35);
    pointer(ONEUI_MOBILE_UP, 51, 35);
    require(clicks == 1 && scroll->scrollOffset() >= 44, "vertical pan scrolls without clicking");
    const float offset = scroll->scrollOffset();
    pointer(ONEUI_MOBILE_DOWN, 50, 60); pointer(ONEUI_MOBILE_MOVE, 90, 61);
    pointer(ONEUI_MOBILE_UP, 90, 61);
    require(scroll->scrollOffset() == offset, "horizontal gesture stays with child");
    const int beforeCancel = clicks;
    pointer(ONEUI_MOBILE_DOWN, 50, 60); pointer(ONEUI_MOBILE_MOVE, 50, 30);
    host.runtime().setSurface(false); host.pumpInput();
    host.runtime().setSurface(true); host.paint(canvas, 2);
    require(clicks == beforeCancel, "surface cancellation cannot click during pan");
    host.setTouchScrolling(false);
    const float disabledOffset = scroll->scrollOffset();
    pointer(ONEUI_MOBILE_DOWN, 50, 60); pointer(ONEUI_MOBILE_MOVE, 50, 20);
    pointer(ONEUI_MOBILE_UP, 50, 20);
    require(scroll->scrollOffset() == disabledOffset, "pan remains opt-in");
}
} // namespace
int main() {
    try {
        retainedWidgetAndIdle();
        cancelledTapCannotClick();
        verticalPanCancelsTapAndKeepsHorizontalControl();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    std::cout << "mobile widget host: ownership, idle, lifecycle and tap cancellation passed\n";
}
