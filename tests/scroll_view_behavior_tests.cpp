#include "oneui/canvas.h"
#include "oneui/layout/scroll_view.h"
#include "oneui/layout/stack.h"
#include "oneui/controls/table.h"
#include "oneui/controls/virtual_list.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

class NullCanvas : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float = 0.0f) override {}
    void strokeRect(oneui::Rect, oneui::Color, float, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring&, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {}
};

struct FillRectCall {
    oneui::Rect rect;
    oneui::Color color;
    float radius = 0.0f;
};

class RecordingCanvas final : public NullCanvas {
public:
    void fillRect(oneui::Rect rect, oneui::Color color, float radius = 0.0f) override {
        fillRects.push_back(FillRectCall{rect, color, radius});
    }

    std::vector<FillRectCall> fillRects;
};

class LayoutProbe final : public oneui::Widget {
public:
    explicit LayoutProbe(oneui::Size preferred) {
        setPreferredSize(preferred);
    }

    void paint(oneui::Canvas&) override {}
};

void expectEqual(const char* name, int actual, int expected) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectNear(const char* name, float actual, float expected) {
    if (std::fabs(actual - expected) > 0.001f) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectBetween(const char* name, float actual, float minimum, float maximum) {
    if (actual < minimum || actual > maximum) {
        std::cerr << name << ": expected [" << minimum << ", " << maximum
                  << "], got " << actual << '\n';
        ++failures;
    }
}

oneui::MouseWheelEvent wheelEvent(float deltaY, double timestampMs) {
    return oneui::MouseWheelEvent{
        oneui::Point{20.0f, 20.0f},
        deltaY,
        false,
        false,
        false,
        timestampMs};
}

void expectRect(const char* name, oneui::Rect actual, oneui::Rect expected) {
    expectNear((std::string(name) + " x").c_str(), actual.x, expected.x);
    expectNear((std::string(name) + " y").c_str(), actual.y, expected.y);
    expectNear((std::string(name) + " width").c_str(), actual.width, expected.width);
    expectNear((std::string(name) + " height").c_str(), actual.height, expected.height);
}

void expectColor(const char* name, oneui::Color actual, oneui::Color expected) {
    if (actual.r != expected.r || actual.g != expected.g || actual.b != expected.b || actual.a != expected.a) {
        std::cerr << name << ": color mismatch\n";
        ++failures;
    }
}

void testOffsetsClampToContentBounds() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 90.0f});
    scroll.setContent(content);
    scroll.setContentWidth(260.0f);
    scroll.setContentHeight(240.0f);

    expectNear("ScrollView max horizontal offset", scroll.maxHorizontalScrollOffset(), 140.0f);
    expectNear("ScrollView max vertical offset", scroll.maxScrollOffset(), 150.0f);

    scroll.setHorizontalScrollOffset(999.0f);
    scroll.setScrollOffset(999.0f);
    expectNear("ScrollView clamps horizontal end", scroll.horizontalScrollOffset(), 140.0f);
    expectNear("ScrollView clamps vertical end", scroll.scrollOffset(), 150.0f);

    scroll.setHorizontalScrollOffset(-5.0f);
    scroll.setScrollOffset(-5.0f);
    expectNear("ScrollView clamps horizontal start", scroll.horizontalScrollOffset(), 0.0f);
    expectNear("ScrollView clamps vertical start", scroll.scrollOffset(), 0.0f);
}

void testWheelKeepsExistingVerticalBehavior() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 300.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    scroll.setContent(content);
    scroll.setWheelStep(50.0f);

    const bool handled = scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, -1.0f});
    expectEqual("ScrollView wheel down handled", handled ? 1 : 0, 1);
    expectNear("ScrollView wheel does not fabricate elapsed time", scroll.scrollOffset(), 0.0f);
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView wheel down offset", scroll.scrollOffset(), 50.0f);
    expectNear("ScrollView wheel keeps horizontal offset", scroll.horizontalScrollOffset(), 0.0f);
    expectRect("ScrollView wheel layout preserves vertical reserve", content->frame(), oneui::Rect{0.0f, -50.0f, 106.0f, 300.0f});

    scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, -10.0f});
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView wheel clamps bottom", scroll.scrollOffset(), 200.0f);

    scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, 10.0f});
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView wheel clamps top", scroll.scrollOffset(), 0.0f);
}

template <typename Control>
void checkNestedWheelRouting(const char* kind, const std::shared_ptr<Control>& child) {
    const auto check = [kind](const char* behavior, bool pass) {
        expectEqual((std::string(kind) + " " + behavior).c_str(), pass ? 1 : 0, 1);
    };
    child->setPreferredSize({0.0f, 120.0f});
    auto content = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
    content->add(child);
    content->add(std::make_shared<LayoutProbe>(oneui::Size{0.0f, 300.0f}));
    oneui::ScrollView outer;
    outer.setFrame({0.0f, 0.0f, 240.0f, 180.0f});
    outer.setContent(content);
    outer.setContentHeight(420.0f);
    NullCanvas canvas;
    outer.paint(canvas);

    check("receives fractional wheel before overflowing parent", outer.onMouseWheel(wheelEvent(-0.25f, 1000.0)));
    outer.tickAnimations(1.0e15);
    check("moves child only", child->scrollOffset() > 0.0f && outer.scrollOffset() == 0.0f);

    child->setScrollOffset(child->maxScrollOffset());
    check("bubbles at lower boundary", outer.onMouseWheel(wheelEvent(-1.0f, 2000.0)));
    outer.tickAnimations(1.0e15);
    check("scrolls parent after child reaches end", outer.scrollOffset() > 0.0f);

    outer.setScrollOffset(20.0f);
    child->setScrollOffset(0.0f);
    outer.paint(canvas);
    check("bubbles at upper boundary", outer.onMouseWheel(wheelEvent(1.0f, 3000.0)));
    outer.tickAnimations(1.0e15);
    check("parent reaches top while child stays at top", outer.scrollOffset() == 0.0f && child->scrollOffset() == 0.0f);

    outer.setContentHeight(180.0f);
    outer.paint(canvas);
    check("receives wheel through a non-overflowing parent", outer.onMouseWheel(wheelEvent(-1.0f, 4000.0)));
    outer.tickAnimations(1.0e15);
    check("scrolls inside fitted parent", child->scrollOffset() > 0.0f && outer.scrollOffset() == 0.0f);

    const float beforeOutside = child->scrollOffset();
    auto outside = wheelEvent(-1.0f, 5000.0);
    outside.position = {20.0f, 200.0f};
    check("rejects input outside clipping viewport", !outer.onMouseWheel(outside));
    outer.tickAnimations(1.0e15);
    check("off-viewport input does not move child", child->scrollOffset() == beforeOutside);

    outer.setContentHeight(420.0f);
    outer.paint(canvas);
    auto blank = wheelEvent(-1.0f, 6000.0);
    blank.position = {20.0f, 150.0f};
    check("scrolls outer blank area", outer.onMouseWheel(blank));
    outer.tickAnimations(1.0e15);
    check("blank area does not scroll child", outer.scrollOffset() > 0.0f && child->scrollOffset() == beforeOutside);

    outer.setScrollOffset(0.0f);
    outer.paint(canvas);
    child->setDisabled(true);
    check("disabled child allows parent scroll", outer.onMouseWheel(wheelEvent(-1.0f, 7000.0)));
    outer.tickAnimations(1.0e15);
    check("disabled child stays still", outer.scrollOffset() > 0.0f && child->scrollOffset() == beforeOutside);
}

void testNestedScrollablesReceiveWheelBeforeTheirParent() {
    auto list = std::make_shared<oneui::VirtualList>();
    list->setItems(std::vector<oneui::ListItem>(40, {L"item", L"detail"}));
    checkNestedWheelRouting("VirtualList", list);

    auto table = std::make_shared<oneui::Table>();
    table->setColumns({{L"Name", 0.0f}});
    table->setRowHeight(30.0f);
    table->setRows(std::vector<std::vector<std::wstring>>(40, {L"row"}));
    checkNestedWheelRouting("Table", table);

    auto scroll = std::make_shared<oneui::ScrollView>();
    scroll->setContent(std::make_shared<LayoutProbe>(oneui::Size{0.0f, 800.0f}));
    checkNestedWheelRouting("ScrollView", scroll);
}

void testWheelRetargetsContinuousMotionAndAcceptsPrecisionDelta() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 400.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    scroll.setContent(content);
    scroll.setWheelStep(40.0f);

    scroll.onMouseWheel(wheelEvent(-1.0f, 1000.0));
    expectNear("ScrollView wheel starts at the sampled position", scroll.scrollOffset(), 0.0f);
    scroll.tickAnimations(1008.0);
    const float firstFrameOffset = scroll.scrollOffset();
    expectBetween("ScrollView first frame advances without jumping to target",
                  firstFrameOffset, 0.01f, 39.99f);

    scroll.onMouseWheel(wheelEvent(-1.0f, 1100.0));
    const float retargetOffset = scroll.scrollOffset();
    expectEqual("ScrollView retarget samples a later position on the running curve",
                retargetOffset > firstFrameOffset && retargetOffset < 40.0f ? 1 : 0,
                1);
    scroll.tickAnimations(1116.0);
    expectEqual("ScrollView retarget continues forward", scroll.scrollOffset() > retargetOffset ? 1 : 0, 1);
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView accumulated wheel target", scroll.scrollOffset(), 80.0f);

    scroll.onMouseWheel(wheelEvent(0.25f, 2000.0));
    expectNear("ScrollView precision delta keeps the current sample", scroll.scrollOffset(), 80.0f);
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView precision delta target", scroll.scrollOffset(), 70.0f);
}

void testBatchedWheelDeltaIsDistributedAcrossFrames() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 800.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    scroll.setContent(content);
    scroll.setWheelStep(40.0f);

    scroll.onMouseWheel(wheelEvent(-6.0f, 1000.0));
    scroll.tickAnimations(1008.0);
    expectBetween("ScrollView batched wheel avoids a large first-frame jump",
                  scroll.scrollOffset(), 0.01f, 60.0f);
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView batched wheel reaches its full target", scroll.scrollOffset(), 240.0f);
}

void testWheelDirectionChangeDropsOpposingQueuedMotion() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 800.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    scroll.setContent(content);
    scroll.setWheelStep(40.0f);

    scroll.onMouseWheel(wheelEvent(-6.0f, 1000.0));
    scroll.onMouseWheel(wheelEvent(1.0f, 1020.0));
    const float reversalSample = scroll.scrollOffset();
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView reversal discards queued distance and applies one reverse step",
               scroll.scrollOffset(), std::max(0.0f, reversalSample - 40.0f));
}

void testAcceleratedWheelPacketsStayInOneContinuousMotion() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 900.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    scroll.setContent(content);
    scroll.setWheelStep(54.0f);

    scroll.onMouseWheel(wheelEvent(-1.0f, 1000.0));
    scroll.tickAnimations(1250.0);
    const float leadPacketOffset = scroll.scrollOffset();
    expectBetween("ScrollView lead packet stays within its target",
                  leadPacketOffset, 0.01f, 54.0f);

    scroll.onMouseWheel(wheelEvent(-6.0f, 1258.0));
    const float retargetOffset = scroll.scrollOffset();
    expectEqual("ScrollView accelerated packet catches up without restarting",
                retargetOffset > leadPacketOffset ? 1 : 0, 1);
    scroll.tickAnimations(1274.0);
    expectEqual("ScrollView accelerated packet continues on the next frame",
                scroll.scrollOffset() > retargetOffset ? 1 : 0, 1);

    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView accelerated packets preserve their full distance",
               scroll.scrollOffset(), 378.0f);
}

void testSmoothScrollMotionPreservesVelocityWhenTargetExtends() {
    oneui::SmoothScrollMotion motion;
    const oneui::ScrollMotionSpec spec{150.0, 110.0, 0.55, 0.90};

    expectEqual("SmoothScrollMotion accepts the lead packet",
                motion.addDelta(54.0f, 0.0f, 800.0f, 1000.0, spec) ? 1 : 0,
                1);
    motion.tick(1258.0);
    const float velocityBeforeRetarget = motion.velocity();
    expectEqual("SmoothScrollMotion remains active across the device packet gap",
                motion.running() ? 1 : 0,
                1);

    expectEqual("SmoothScrollMotion accepts the accelerated packet",
                motion.addDelta(324.0f, 0.0f, 800.0f, 1258.0, spec) ? 1 : 0,
                1);
    expectNear("SmoothScrollMotion retarget preserves velocity",
               motion.velocity(), velocityBeforeRetarget);
    motion.tick(1274.0);
    expectEqual("SmoothScrollMotion accelerates continuously toward the new target",
                motion.velocity() > velocityBeforeRetarget ? 1 : 0,
                1);
}

void testKeyboardScrollsVerticalAndHorizontalOffsets() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 100.0f, 80.0f});
    scroll.setContent(content);
    scroll.setContentWidth(220.0f);
    scroll.setContentHeight(240.0f);
    scroll.setWheelStep(25.0f);

    expectEqual("ScrollView Down handled", scroll.onKeyDown(oneui::KeyEvent{oneui::Key::Down}) ? 1 : 0, 1);
    expectNear("ScrollView Down offset", scroll.scrollOffset(), 25.0f);

    expectEqual("ScrollView Up handled", scroll.onKeyDown(oneui::KeyEvent{oneui::Key::Up}) ? 1 : 0, 1);
    expectNear("ScrollView Up offset", scroll.scrollOffset(), 0.0f);

    expectEqual("ScrollView Right handled", scroll.onKeyDown(oneui::KeyEvent{oneui::Key::Right}) ? 1 : 0, 1);
    expectNear("ScrollView Right offset", scroll.horizontalScrollOffset(), 25.0f);

    expectEqual("ScrollView Left handled", scroll.onKeyDown(oneui::KeyEvent{oneui::Key::Left}) ? 1 : 0, 1);
    expectNear("ScrollView Left offset", scroll.horizontalScrollOffset(), 0.0f);

    scroll.onKeyDown(oneui::KeyEvent{oneui::Key::End});
    expectNear("ScrollView End vertical offset", scroll.scrollOffset(), 160.0f);
    expectNear("ScrollView End horizontal offset", scroll.horizontalScrollOffset(), 120.0f);

    scroll.onKeyDown(oneui::KeyEvent{oneui::Key::Home});
    expectNear("ScrollView Home vertical offset", scroll.scrollOffset(), 0.0f);
    expectNear("ScrollView Home horizontal offset", scroll.horizontalScrollOffset(), 0.0f);
}

void testHorizontalOffsetAppliesToContentLayout() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{10.0f, 20.0f, 120.0f, 90.0f});
    scroll.setContent(content);
    scroll.setContentWidth(260.0f);
    scroll.setContentHeight(240.0f);
    scroll.setHorizontalScrollOffset(40.0f);
    scroll.setScrollOffset(30.0f);

    NullCanvas canvas;
    scroll.paint(canvas);

    expectRect("ScrollView applies x and y offsets", content->frame(), oneui::Rect{-30.0f, -10.0f, 260.0f, 240.0f});
}

void testHorizontalThumbPaintsWhenContentOverflows() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{10.0f, 20.0f, 100.0f, 80.0f});
    scroll.setContent(content);
    scroll.setContentWidth(200.0f);
    scroll.setHorizontalScrollOffset(50.0f);

    RecordingCanvas canvas;
    scroll.paint(canvas);

    expectEqual("ScrollView horizontal thumb paint count", static_cast<int>(canvas.fillRects.size()), 2);
    expectRect("ScrollView horizontal thumb rect", canvas.fillRects.back().rect, oneui::Rect{35.0f, 91.0f, 50.0f, 4.0f});
}

void testHorizontalThumbDragUpdatesAndClampsOffset() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{10.0f, 20.0f, 100.0f, 80.0f});
    scroll.setContent(content);
    scroll.setContentWidth(200.0f);

    expectEqual("ScrollView horizontal thumb mouse-down handled",
                scroll.onMouseDown(oneui::MouseEvent{oneui::Point{16.0f, 92.0f}}) ? 1 : 0,
                1);
    expectEqual("ScrollView horizontal thumb mouse-move handled",
                scroll.onMouseMove(oneui::MouseEvent{oneui::Point{60.0f, 92.0f}}) ? 1 : 0,
                1);
    expectNear("ScrollView horizontal thumb drag updates offset", scroll.horizontalScrollOffset(), 100.0f);
    expectEqual("ScrollView horizontal thumb mouse-up handled",
                scroll.onMouseUp(oneui::MouseEvent{oneui::Point{60.0f, 92.0f}}) ? 1 : 0,
                1);

    scroll.setHorizontalScrollOffset(0.0f);
    scroll.onMouseDown(oneui::MouseEvent{oneui::Point{16.0f, 92.0f}});
    scroll.onMouseMove(oneui::MouseEvent{oneui::Point{-40.0f, 92.0f}});
    expectNear("ScrollView horizontal thumb drag clamps start", scroll.horizontalScrollOffset(), 0.0f);
}

void testVerticalThumbDragUpdatesAndClampsOffset() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    oneui::ScrollView scroll;
    // Viewport 100 tall, content 400: thumb 25 tall, inset 5 -> travel 65, max 300.
    scroll.setFrame(oneui::Rect{10.0f, 20.0f, 100.0f, 100.0f});
    scroll.setContent(content);
    scroll.setContentHeight(400.0f);
    scroll.setScrollbarStyle(oneui::Color{1, 2, 3, 255}, 3.0f);

    // The painted thumb is 3px wide at x 102..105; press 4px to its left,
    // still inside the 13px gutter, near the thumb's top.
    expectEqual("ScrollView vertical gutter press grabs the thumb",
                scroll.onMouseDown(oneui::MouseEvent{oneui::Point{98.0f, 30.0f}, oneui::MouseButton::Left}) ? 1 : 0,
                1);
    scroll.onMouseMove(oneui::MouseEvent{oneui::Point{98.0f, 30.0f + 32.5f}});
    expectNear("ScrollView vertical thumb drag maps track travel to offset", scroll.scrollOffset(), 150.0f);
    scroll.onMouseMove(oneui::MouseEvent{oneui::Point{98.0f, 500.0f}});
    expectNear("ScrollView vertical thumb drag clamps end", scroll.scrollOffset(), 300.0f);
    expectEqual("ScrollView vertical thumb release handled",
                scroll.onMouseUp(oneui::MouseEvent{oneui::Point{98.0f, 500.0f}, oneui::MouseButton::Left}) ? 1 : 0,
                1);
    scroll.onMouseMove(oneui::MouseEvent{oneui::Point{98.0f, 30.0f}});
    expectNear("ScrollView vertical drag ends on release", scroll.scrollOffset(), 300.0f);
}

void testVerticalTrackClickPagesByOneViewport() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{10.0f, 20.0f, 100.0f, 100.0f});
    scroll.setContent(content);
    scroll.setContentHeight(400.0f);

    // Below the thumb (thumb occupies y 25..50 at offset 0).
    scroll.onMouseDown(oneui::MouseEvent{oneui::Point{105.0f, 110.0f}, oneui::MouseButton::Left});
    scroll.onMouseUp(oneui::MouseEvent{oneui::Point{105.0f, 110.0f}, oneui::MouseButton::Left});
    expectNear("ScrollView track click below the thumb pages down", scroll.scrollOffset(), 100.0f);
    scroll.onMouseDown(oneui::MouseEvent{oneui::Point{105.0f, 22.0f}, oneui::MouseButton::Left});
    scroll.onMouseUp(oneui::MouseEvent{oneui::Point{105.0f, 22.0f}, oneui::MouseButton::Left});
    expectNear("ScrollView track click above the thumb pages up", scroll.scrollOffset(), 0.0f);
}

void testUnchangedContentHeightKeepsWheelMotion() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    scroll.setContent(content);
    scroll.setContentHeight(600.0f);
    scroll.setWheelStep(40.0f);

    scroll.onMouseWheel(wheelEvent(-3.0f, 1000.0));
    scroll.tickAnimations(1008.0);
    // A binding refresh re-applies the same height while the wheel animates.
    scroll.setContentHeight(600.0f);
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView wheel target survives an unchanged content height", scroll.scrollOffset(), 120.0f);

    // A real change that keeps the offset in range also keeps the motion.
    scroll.onMouseWheel(wheelEvent(-2.0f, 2000.0));
    scroll.tickAnimations(2008.0);
    scroll.setContentHeight(700.0f);
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView wheel target survives a growing content height", scroll.scrollOffset(), 200.0f);

    // Shrinking below the current offset still clamps.
    scroll.setContentHeight(150.0f);
    expectNear("ScrollView shrinking content clamps the offset", scroll.scrollOffset(), 50.0f);
}

void testChromeAndScrollbarStyleCanBeCustomized() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{10.0f, 20.0f, 100.0f, 80.0f});
    scroll.setContent(content);
    scroll.setContentHeight(240.0f);
    scroll.setChromeVisible(false);
    scroll.setScrollbarStyle(oneui::Color{70, 80, 90, 100}, 3.0f);

    RecordingCanvas canvas;
    scroll.paint(canvas);

    expectEqual("ScrollView custom chrome omits viewport fill", static_cast<int>(canvas.fillRects.size()), 1);
    expectNear("ScrollView custom scrollbar thickness", canvas.fillRects.front().rect.width, 3.0f);
    expectColor("ScrollView custom scrollbar color", canvas.fillRects.front().color, oneui::Color{70, 80, 90, 100});
}

void testCssStyleBoxPaintsWhenDefaultChromeIsHidden() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{10.0f, 20.0f, 100.0f, 80.0f});
    scroll.setContent(content);
    scroll.setChromeVisible(false);

    oneui::StyleBox style;
    style.background.color = oneui::Color{18, 24, 36, 255};
    style.borderColor = oneui::Color{44, 52, 66, 255};
    style.borderWidth = 0.0f;
    scroll.setStyleBox(style);

    RecordingCanvas canvas;
    scroll.paint(canvas);

    expectEqual("ScrollView CSS background fill count", static_cast<int>(canvas.fillRects.size()), 1);
    expectRect("ScrollView CSS background rect", canvas.fillRects.front().rect, oneui::Rect{10.0f, 20.0f, 100.0f, 80.0f});
    expectColor("ScrollView CSS background color", canvas.fillRects.front().color, oneui::Color{18, 24, 36, 255});
}

} // namespace

int main() {
    testOffsetsClampToContentBounds();
    testWheelKeepsExistingVerticalBehavior();
    testNestedScrollablesReceiveWheelBeforeTheirParent();
    testWheelRetargetsContinuousMotionAndAcceptsPrecisionDelta();
    testBatchedWheelDeltaIsDistributedAcrossFrames();
    testWheelDirectionChangeDropsOpposingQueuedMotion();
    testAcceleratedWheelPacketsStayInOneContinuousMotion();
    testSmoothScrollMotionPreservesVelocityWhenTargetExtends();
    testKeyboardScrollsVerticalAndHorizontalOffsets();
    testHorizontalOffsetAppliesToContentLayout();
    testHorizontalThumbPaintsWhenContentOverflows();
    testHorizontalThumbDragUpdatesAndClampsOffset();
    testVerticalThumbDragUpdatesAndClampsOffset();
    testVerticalTrackClickPagesByOneViewport();
    testUnchangedContentHeightKeepsWheelMotion();
    testChromeAndScrollbarStyleCanBeCustomized();
    testCssStyleBoxPaintsWhenDefaultChromeIsHidden();

    if (failures != 0) {
        std::cerr << failures << " scroll view behavior test(s) failed.\n";
        return 1;
    }

    return 0;
}
