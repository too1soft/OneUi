#include "oneui/layout/stack.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

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

class RecordingCanvas final : public NullCanvas {
public:
    void fillRect(oneui::Rect rect, oneui::Color color, float radius = 0.0f) override {
        fillCount += 1;
        lastRect = rect;
        lastColor = color;
        lastRadius = radius;
    }

    int fillCount = 0;
    oneui::Rect lastRect{};
    oneui::Color lastColor{0, 0, 0, 0};
    float lastRadius = 0.0f;
};

class LayoutProbe final : public oneui::Widget {
public:
    explicit LayoutProbe(oneui::Size preferred) {
        setPreferredSize(preferred);
    }
    void paint(oneui::Canvas&) override {}
};

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

void testRowDistributesRemainingWidthToFlexibleChildren() {
    oneui::Stack stack(oneui::StackDirection::Row);
    stack.setFrame(oneui::Rect{0.0f, 0.0f, 500.0f, 120.0f});
    stack.setGap(10.0f);

    auto fixed = std::make_shared<LayoutProbe>(oneui::Size{180.0f, 0.0f});
    auto flex = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    stack.add(fixed);
    stack.add(flex);

    NullCanvas canvas;
    stack.paint(canvas);

    expectRect("Stack row fixed child", fixed->frame(), oneui::Rect{0.0f, 0.0f, 180.0f, 120.0f});
    expectRect("Stack row flex child", flex->frame(), oneui::Rect{190.0f, 0.0f, 310.0f, 120.0f});
}

void testColumnDistributesRemainingHeightToFlexibleChildren() {
    oneui::Stack stack(oneui::StackDirection::Column);
    stack.setFrame(oneui::Rect{10.0f, 20.0f, 200.0f, 300.0f});
    stack.setGap(8.0f);
    stack.setPadding(oneui::Insets{4.0f});

    auto header = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 40.0f});
    auto body = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    stack.add(header);
    stack.add(body);

    NullCanvas canvas;
    stack.paint(canvas);

    expectRect("Stack column fixed child", header->frame(), oneui::Rect{14.0f, 24.0f, 192.0f, 40.0f});
    expectRect("Stack column flex child", body->frame(), oneui::Rect{14.0f, 72.0f, 192.0f, 244.0f});
}

void testStackStyleBoxPaintsContainerBackground() {
    oneui::Stack stack(oneui::StackDirection::Column);
    stack.setFrame(oneui::Rect{2.0f, 4.0f, 120.0f, 48.0f});

    oneui::StyleBox box;
    box.background.color = oneui::Color{17, 17, 20, 255};
    box.radius = 6.0f;
    stack.setStyleBox(box);

    RecordingCanvas canvas;
    stack.paint(canvas);

    expectNear("Stack style box fill count", static_cast<float>(canvas.fillCount), 1.0f);
    expectRect("Stack style box fill rect", canvas.lastRect, oneui::Rect{2.0f, 4.0f, 120.0f, 48.0f});
    expectNear("Stack style box radius", canvas.lastRadius, 6.0f);
    expectNear("Stack style box fill red", static_cast<float>(canvas.lastColor.r), 17.0f);
}

void testContentExtentUsesPreferredChildrenGapAndPadding() {
    oneui::Stack stack(oneui::StackDirection::Column);
    stack.setGap(7.0f);
    stack.setPadding(oneui::Insets{2.0f, 3.0f, 4.0f, 5.0f});
    stack.add(std::make_shared<LayoutProbe>(oneui::Size{140.0f, 19.0f}));
    stack.add(std::make_shared<LayoutProbe>(oneui::Size{80.0f, 32.0f}));
    stack.add(std::make_shared<LayoutProbe>(oneui::Size{120.0f, 28.0f}));

    expectNear("Stack content width", stack.contentWidth(), 148.0f);
    expectNear("Stack content height", stack.contentHeight(), 99.0f);
}

void testRepeatedLayoutValuesDoNotInvalidate() {
    oneui::Stack stack(oneui::StackDirection::Column);
    auto child = std::make_shared<LayoutProbe>(oneui::Size{80.0f, 28.0f});
    stack.add(child);
    stack.setGap(7.0f);
    stack.setPadding(oneui::Insets{2.0f, 3.0f, 4.0f, 5.0f});
    int invalidations = 0;
    stack.setInvalidator([&] { ++invalidations; });
    stack.setGap(7.0f);
    stack.setPadding(oneui::Insets{2.0f, 3.0f, 4.0f, 5.0f});
    stack.setAlign(stack.align());
    stack.setJustify(oneui::StackJustify::Start);
    stack.clearFlex(child);
    expectNear("Unchanged layout values cause no invalidations", float(invalidations), 0.0f);
    stack.setGap(8.0f);
    stack.setFlexPresentation(child, {50.0f, 0});
    stack.clearFlex(child);
    expectNear("Actual changes still invalidate the stack", float(invalidations), 3.0f);
}

void testStackOrderHonorsOverridesAndRestoresInsertionOrder() {
    for (auto engine : {oneui::StackEngine::Legacy, oneui::StackEngine::Yoga}) {
        oneui::Stack stack(oneui::StackDirection::Row);
        if (!stack.setEngine(engine)) continue;
        stack.setFrame({0, 0, 200, 40});
        const auto a = std::make_shared<LayoutProbe>(oneui::Size{30, 20});
        const auto b = std::make_shared<LayoutProbe>(oneui::Size{30, 20});
        const auto c = std::make_shared<LayoutProbe>(oneui::Size{30, 20});
        stack.add(a);
        stack.add(b);
        stack.add(c);
        NullCanvas canvas;
        stack.paint(canvas);
        expectNear("Default stack order starts with the first child", a->frame().x, 0);
        expectNear("Default stack order keeps the second child", b->frame().x, 30);

        stack.setFlexPresentation(b, {{}, -1});
        stack.setFlexPresentation(c, {{}, -1});
        stack.paint(canvas);
        expectNear("Negative order moves a child first", b->frame().x, 0);
        expectNear("Equal overrides preserve insertion order", c->frame().x, 30);
        expectNear("Unordered child follows negative order", a->frame().x, 60);

        stack.setFlexPresentation(b, {10.0f, 0});
        stack.setFlexPresentation(c, {});
        stack.paint(canvas);
        expectNear("Resetting order restores the first child", a->frame().x, 0);
        expectNear("A percentage basis preserves insertion order", b->frame().x, 30);
        expectNear("A percentage basis still supplies geometry", b->frame().width, 20);
        expectNear("Resetting order restores the final child", c->frame().x, 50);
    }
}

} // namespace

int main() {
    testRowDistributesRemainingWidthToFlexibleChildren();
    testColumnDistributesRemainingHeightToFlexibleChildren();
    testStackStyleBoxPaintsContainerBackground();
    testContentExtentUsesPreferredChildrenGapAndPadding();
    testRepeatedLayoutValuesDoNotInvalidate();
    testStackOrderHonorsOverridesAndRestoresInsertionOrder();

    if (failures != 0) {
        std::cerr << failures << " stack behavior test(s) failed.\n";
        return 1;
    }

    return 0;
}
