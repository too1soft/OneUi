#include "oneui/controls/button.h"
#include "oneui/controls/label.h"
#include "oneui/layout/stack.h"
#include "support/recording_canvas.h"
#include <chrono>
#include <cmath>
#include <iostream>

using namespace oneui;

struct PaintProbe : Widget {
    void paint(Canvas&) override {}
    void repaint() { invalidatePaint(); }
};

bool reentrantContentChange() {
    auto root = std::make_shared<Stack>();
    root->setEngine(StackEngine::Yoga);
    auto trigger = std::make_shared<PaintProbe>();
    auto text = std::make_shared<Label>(L"Short");
    root->add(trigger);
    root->add(text);
    root->setFrame({0, 0, 500, 300});
    test_support::RecordingCanvas canvas;
    root->paint(canvas);
    bool changed = false;
    root->setRectInvalidator([&](Rect) {
        if (!changed) {
            changed = true;
            text->setText(L"A real content change inside the repaint callback");
        }
    });
    const auto revision = root->measureRevision();
    trigger->repaint();
    if (!changed || root->measureRevision() == revision) return false;
    root->paint(canvas);
    const auto settled = root->measureRevision();
    trigger->repaint();
    if (root->measureRevision() != settled) return false;
    root->setRectInvalidator([](Rect) { throw 1; });
    try { trigger->repaint(); } catch (int) {}
    root->setRectInvalidator([](Rect) {});
    text->setText(L"Normal content invalidation after an exception");
    if (root->measureRevision() == settled) return false;
    int redraws = 0;
    trigger->setRectInvalidator({});
    trigger->setInvalidator([&] { ++redraws; });
    trigger->repaint();
    return redraws == 1;
}

int main() {
    if (!reentrantContentChange()) {
        std::cerr << "FAIL: nested content change, exception or fallback invalidation\n";
        return 1;
    }
    auto outer = std::make_shared<Stack>();
    auto row = std::make_shared<Stack>(StackDirection::Row);
    outer->setEngine(StackEngine::Yoga);
    row->setEngine(StackEngine::Yoga);
    outer->add(row);
    outer->setFlex(row, {1, 1, 0.0f});
    auto button = std::make_shared<Button>(L"Navigate");
    ButtonStyleOverride style;
    ButtonStateStyleOverride normal, hover;
    normal.background = Color{20, 30, 40};
    normal.transition = TransitionSpec{100, EasingCurve::EaseOutCubic};
    hover.background = Color{80, 90, 100};
    style.normal = normal;
    style.hovered = hover;
    button->setStyleOverride(style);
    row->add(button);
    for (int i = 0; i < 400; ++i) {
        auto label = std::make_shared<Label>(L"Unchanged content");
        row->add(label);
        row->setFlex(label, {1, 1, 100.0f});
    }
    test_support::RecordingCanvas canvas;
    int redraws = 0;
    outer->setRectInvalidator([&](Rect) { ++redraws; });
    outer->setAnimationScheduler([] {});
    outer->setFrame({0, 0, 1200, 800});
    outer->paint(canvas);
    const auto frame = button->frame();
    button->onMouseMove({{frame.x + 1, frame.y + 1}});
    outer->paint(canvas); // The hover state change itself may affect layout.
    const auto before = outer->layoutStats().calculations;
    const auto started = std::chrono::steady_clock::now();
    const double now = std::chrono::duration<double, std::milli>(started.time_since_epoch()).count();
    for (int i = 0; i < 60; ++i) {
        outer->tickAnimations(now + i * 16);
        outer->paint(canvas);
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    const auto calculations = outer->layoutStats().calculations - before;
    std::cout << "color animation: 60 paints, layout calculations=" << calculations
              << ", elapsedMs=" << elapsed << ", redraws=" << redraws << '\n';
    if (calculations != 0) {
        std::cerr << "FAIL: color-only animation invalidated ancestor layout\n";
        return 1;
    }
    const auto textBefore = outer->layoutStats().calculations;
    button->setText(L"A longer navigation label");
    outer->paint(canvas);
    if (outer->layoutStats().calculations <= textBefore) {
        std::cerr << "FAIL: text change did not invalidate ancestor layout\n";
        return 1;
    }
    return 0;
}
