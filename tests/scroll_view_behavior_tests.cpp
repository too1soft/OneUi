#include "oneui/canvas.h"
#include "oneui/layout/scroll_view.h"

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
    expectEqual("ScrollView wheel begins a transition", scroll.scrollOffset() > 0.0f && scroll.scrollOffset() < 50.0f ? 1 : 0, 1);
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

void testWheelRetargetsContinuousMotionAndAcceptsPrecisionDelta() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 400.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    scroll.setContent(content);
    scroll.setWheelStep(40.0f);

    scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, -1.0f});
    const float firstOffset = scroll.scrollOffset();
    expectEqual("ScrollView first frame consumes responsive distance", firstOffset >= 12.0f ? 1 : 0, 1);
    scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, -1.0f});
    expectEqual("ScrollView retarget keeps moving immediately", scroll.scrollOffset() > firstOffset ? 1 : 0, 1);
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView accumulated wheel target", scroll.scrollOffset(), 80.0f);

    scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, 0.25f});
    expectEqual("ScrollView precision delta responds immediately", scroll.scrollOffset() < 80.0f ? 1 : 0, 1);
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView precision delta target", scroll.scrollOffset(), 70.0f);
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

} // namespace

int main() {
    testOffsetsClampToContentBounds();
    testWheelKeepsExistingVerticalBehavior();
    testWheelRetargetsContinuousMotionAndAcceptsPrecisionDelta();
    testKeyboardScrollsVerticalAndHorizontalOffsets();
    testHorizontalOffsetAppliesToContentLayout();
    testHorizontalThumbPaintsWhenContentOverflows();
    testHorizontalThumbDragUpdatesAndClampsOffset();
    testChromeAndScrollbarStyleCanBeCustomized();

    if (failures != 0) {
        std::cerr << failures << " scroll view behavior test(s) failed.\n";
        return 1;
    }

    return 0;
}
