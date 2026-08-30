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

} // namespace

int main() {
    testRowDistributesRemainingWidthToFlexibleChildren();
    testColumnDistributesRemainingHeightToFlexibleChildren();
    testStackStyleBoxPaintsContainerBackground();
    testContentExtentUsesPreferredChildrenGapAndPadding();

    if (failures != 0) {
        std::cerr << failures << " stack behavior test(s) failed.\n";
        return 1;
    }

    return 0;
}
