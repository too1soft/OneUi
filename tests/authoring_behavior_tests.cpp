#include "oneui/controls/button.h"
#include "oneui/layout/stack.h"
#include "oneui/style_adapter.h"
#include "support/recording_canvas.h"

#include <cmath>
#include <iostream>

using namespace oneui;
using oneui::test_support::RecordingCanvas;
int failures = 0;
void near(float actual, float expected, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > .01f) {
        std::cerr << message << ": " << actual << " != " << expected << '\n';
        ++failures;
    }
}
std::shared_ptr<Button> child(Stack& row, float width) {
    auto result = std::make_shared<Button>(L"Item");
    result->setPreferredSize({width, 30});
    row.add(result);
    return result;
}
void flexTests() {
    Stack row(StackDirection::Row);
    row.setFrame({0, 0, 500, 60});
    row.setGap(10);
    auto a = child(row, 100), b = child(row, 100);
    row.setFlex(a, {1, 1, 100, 60, 160});
    row.setFlex(b, {2, 1, 100, 60});
    RecordingCanvas canvas;
    row.paint(canvas);
    near(a->frame().width, 160, "grow capped");
    near(b->frame().width, 330, "remaining redistributed");
    row.setFrame({0, 0, 160, 60});
    row.setFlex(a, {1, 1, 100, 90});
    row.paint(canvas);
    near(a->frame().width, 90, "shrink freezes at min");
    near(b->frame().width, 60, "shrink redistribution");
    row.setFrame({0, 0, 100, 60});
    row.paint(canvas);
    near(b->frame().width, 60, "minimums overflow instead of negative sizes");
    b->setVisible(false);
    row.setFrame({0, 0, 300, 60});
    row.paint(canvas);
    near(a->frame().width, 300, "hidden item excludes gap and allocation");
    row.clearFlex(a);
    row.setJustify(StackJustify::End);
    row.paint(canvas);
    near(a->frame().x, 200, "legacy fixed size and end alignment");
    b->setVisible(true);
    row.clearFlex(b);
    row.setJustify(StackJustify::SpaceBetween);
    row.paint(canvas);
    near(b->frame().x, 200, "space between");
    row.setDirection(StackDirection::Column);
    row.setFrame({0, 0, 50, 100});
    row.paint(canvas);
    near(b->frame().y, 70, "column justify");
    row.setPadding(Insets{100});
    row.paint(canvas);
    near(a->frame().width, 0, "oversized padding clamps cross axis");
    row.clearChildren();
    row.paint(canvas);
    row.setPadding(Insets{0});
    row.setDirection(StackDirection::Row);
    row.setFrame({0, 0, 300, 50});
    auto c = child(row, 200), d = child(row, 100);
    row.setFlex(c, {0, 1});
    row.setFlex(d, {0, 1});
    row.setGap(0);
    row.setFrame({0, 0, 150, 50});
    row.paint(canvas);
    near(c->frame().width, 100, "shrink weighted by basis");
    near(d->frame().width, 50, "shrink weighted by basis second");
}
void paddingTests() {
    StyleSheet sheet;
    std::string error;
    if (!sheet.addRulesFromCss("button { padding: 4px 16px 8px 24px; } button:hover { padding: 2px; }", &error)) {
        std::cerr << error; ++failures; return;
    }
    Button button(L"Run");
    button.setFrame({10, 20, 160, 40});
    button.setContentAlign(TextAlign::Left);
    button.setStyleOverride(buttonStyleOverrideFromStyleSheet(sheet, StyleNode{"button", {}, StyleStateNone}));
    RecordingCanvas canvas;
    button.paint(canvas);
    near(canvas.texts.at(0).rect.x, 34, "CSS padding reaches button paint");
    near(canvas.texts.at(0).rect.y, 24, "asymmetric top padding");
    near(canvas.texts.at(0).rect.width, 120, "asymmetric horizontal padding");
    near(canvas.texts.at(0).rect.height, 28, "asymmetric vertical padding");
    near(canvas.fillRects.at(0).rect.width, 160, "background keeps full frame");
    MouseEvent event; event.position = {30, 30};
    button.onMouseMove(event);
    RecordingCanvas hovered;
    button.paint(hovered);
    near(hovered.texts.at(0).rect.x, 12, "hover padding");
    near(hovered.saves, hovered.restores, "balanced content clip");
    button.setTrailingText(L"12");
    RecordingCanvas metadata;
    button.paint(metadata);
    near(metadata.texts.at(0).rect.x, 12, "metadata does not double inset");
    ButtonStyleOverride style;
    style.normal = ButtonStateStyleOverride{};
    style.normal->padding = Insets{0};
    button.setStyleOverride(style);
    const auto unpadded = button.naturalContentSize();
    style.normal->padding = Insets{4, 12};
    button.setStyleOverride(style);
    const auto padded = button.naturalContentSize();
    near(padded.width - unpadded.width, 24, "measurement includes horizontal padding");
    near(padded.height - unpadded.height, 8, "measurement includes vertical padding");
    style.normal->padding = Insets{0};
    button.setStyleOverride(style);
    RecordingCanvas zero;
    button.paint(zero);
    near(zero.texts.at(0).rect.x, 10, "explicit zero padding");
    style.normal->padding = Insets{500};
    button.setStyleOverride(style);
    RecordingCanvas tiny;
    button.paint(tiny);
    near(static_cast<float>(tiny.texts.size()), 0, "no content in exhausted box");
}
void contentSizingTests() {
    struct Probe : Widget {
        mutable int measurements = 0;
        Size naturalSize() const override { ++measurements; return {80, 24}; }
        void paint(Canvas&) override {}
    };
    auto leaf = std::make_shared<Probe>();
    std::shared_ptr<Widget> node = leaf;
    for (int i = 0; i < 12; ++i) {
        auto stack = std::make_shared<Stack>();
        stack->setPadding(Insets{2});
        stack->add(node);
        stack->setFlex(node, StackFlex{0, 0, {}, 0, 10000, true});
        node = stack;
    }
    const auto size = node->naturalSize();
    near(size.width, 128, "nested natural width with padding");
    near(size.height, 72, "nested natural height with padding");
    near(static_cast<float>(leaf->measurements), 1, "one measurement per descendant, not exponential");
    Stack row(StackDirection::Row);
    row.setFrame({0, 0, 200, 60});
    row.setAlign(StackAlign::Center);
    row.add(leaf);
    row.setFlex(leaf, StackFlex{0, 0, {}, 0, 10000, true});
    RecordingCanvas canvas;
    row.paint(canvas);
    near(leaf->frame().width, 80, "content basis lays out natural width");
    near(leaf->frame().height, 24, "content basis uses natural cross axis");
    near(leaf->frame().y, 18, "intrinsic content is centered");
}
int main() {
#ifndef ONEUI_HAS_YOGA
    Stack optional;
    near(optional.setEngine(StackEngine::Yoga) ? 1.0f : 0.0f, 0, "unavailable engine rejected");
    near(optional.setWrap(true) ? 1.0f : 0.0f, 0, "legacy wrap rejected");
#endif
    flexTests();
    paddingTests();
    contentSizingTests();
    return failures ? 1 : 0;
}
