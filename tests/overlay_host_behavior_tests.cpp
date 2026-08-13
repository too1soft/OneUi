#include "oneui/controls/button.h"
#include "oneui/controls/menu.h"
#include "oneui/controls/popup.h"
#include "oneui/controls/terminal_view.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/layout/panel.h"
#include "oneui/layout/stack.h"
#include "oneui/view.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

class NullCanvas final : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float = 0.0f) override {}
    void strokeRect(oneui::Rect, oneui::Color, float = 0.0f, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring&, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {}
};

class BackdropCanvas final : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color color, float = 0.0f) override {
        fills.push_back(color);
    }
    void strokeRect(oneui::Rect, oneui::Color, float = 0.0f, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring&, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {}

    std::vector<oneui::Color> fills;
};

class OverlayProbe final : public oneui::Widget {
public:
    OverlayProbe(int probeId, std::vector<int>& paintLog, std::vector<int>& eventLog)
        : id(probeId), paintLog_(paintLog), eventLog_(eventLog) {}

    void paint(oneui::Canvas&) override {
        paintLog_.push_back(id);
    }

    bool onMouseMove(const oneui::MouseEvent&) override {
        eventLog_.push_back(id * 10 + 1);
        return true;
    }

    bool onMouseDown(const oneui::MouseEvent&) override {
        eventLog_.push_back(id * 10 + 2);
        return handleMouseDown;
    }

    bool onMouseUp(const oneui::MouseEvent&) override {
        eventLog_.push_back(id * 10 + 3);
        return true;
    }

    bool onMouseWheel(const oneui::MouseWheelEvent&) override {
        eventLog_.push_back(id * 10 + 4);
        return handleMouseWheel;
    }

    bool onFocusChanged(bool focused) override {
        focusLog.push_back(focused ? 1 : 0);
        return oneui::Widget::onFocusChanged(focused);
    }

    bool tickAnimations(double) override {
        if (!animationRunning) {
            return false;
        }
        ++animationTicks;
        return true;
    }

    bool isFocusable() const override {
        return focusable;
    }

    void invalidateSelf() {
        invalidate();
    }

    void scheduleAnimationSelf() {
        requestAnimationFrame();
    }

    int id = 0;
    bool focusable = false;
    bool handleMouseDown = true;
    bool handleMouseWheel = true;
    bool animationRunning = false;
    int animationTicks = 0;
    std::vector<int> focusLog;

private:
    std::vector<int>& paintLog_;
    std::vector<int>& eventLog_;
};

void expectEqual(const char* name, int actual, int expected) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectSamePointer(const char* name, const oneui::Widget* actual, const oneui::Widget* expected) {
    if (actual != expected) {
        std::cerr << name << ": pointers differ\n";
        ++failures;
    }
}

void expectSequence(const char* name, const std::vector<int>& actual, const std::vector<int>& expected) {
    if (actual == expected) {
        return;
    }

    std::cerr << name << ": expected";
    for (int value : expected) {
        std::cerr << ' ' << value;
    }
    std::cerr << ", got";
    for (int value : actual) {
        std::cerr << ' ' << value;
    }
    std::cerr << '\n';
    ++failures;
}

std::shared_ptr<OverlayProbe> makeProbe(int id, std::vector<int>& paintLog, std::vector<int>& eventLog, oneui::Rect frame = oneui::Rect{0.0f, 0.0f, 100.0f, 100.0f}) {
    auto probe = std::make_shared<OverlayProbe>(id, paintLog, eventLog);
    probe->setFrame(frame);
    return probe;
}

void testAddOverlayPreservesEntryOrderAndLayerValues() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto first = makeProbe(1, paintLog, eventLog);
    auto second = makeProbe(2, paintLog, eventLog);
    auto third = makeProbe(3, paintLog, eventLog);

    host.addOverlay(first, 20);
    host.addOverlay(second, -10);
    host.addOverlay(third, 20);

    expectEqual("OverlayHost add count", static_cast<int>(host.overlays().size()), 3);
    expectSamePointer("OverlayHost first add order", host.overlays()[0].child.get(), first.get());
    expectSamePointer("OverlayHost second add order", host.overlays()[1].child.get(), second.get());
    expectSamePointer("OverlayHost third add order", host.overlays()[2].child.get(), third.get());
    expectEqual("OverlayHost first layer", host.overlays()[0].layer, 20);
    expectEqual("OverlayHost second layer", host.overlays()[1].layer, -10);
    expectEqual("OverlayHost third layer", host.overlays()[2].layer, 20);
}

void testPaintOrdersByLayerWithStableEqualLayers() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;
    NullCanvas canvas;

    host.addOverlay(makeProbe(1, paintLog, eventLog), 10);
    host.addOverlay(makeProbe(2, paintLog, eventLog), -5);
    host.addOverlay(makeProbe(3, paintLog, eventLog), 10);
    host.addOverlay(makeProbe(4, paintLog, eventLog), 0);

    host.paint(canvas);

    expectSequence("OverlayHost paint layer order", paintLog, {2, 4, 1, 3});
}

void testModalOverlayPaintsTheStandardBackdrop() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;
    host.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 240.0f});
    BackdropCanvas canvas;

    host.addOverlay(makeProbe(1, paintLog, eventLog), oneui::OverlayOptions::modeless(0));
    host.addOverlay(makeProbe(2, paintLog, eventLog), oneui::OverlayOptions::modal(10));
    host.paint(canvas);

    expectEqual("Modal overlay paints one backdrop", static_cast<int>(canvas.fills.size()), 1);
    if (!canvas.fills.empty()) {
        expectEqual("Modal backdrop red", canvas.fills.front().r, 0);
        expectEqual("Modal backdrop green", canvas.fills.front().g, 0);
        expectEqual("Modal backdrop blue", canvas.fills.front().b, 0);
        expectEqual("Modal backdrop alpha", canvas.fills.front().a, 104);
    }
}

void testAnchoredOverlayLaysOutAgainstHostFrame() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;
    NullCanvas canvas;
    host.setFrame(oneui::Rect{10.0f, 20.0f, 400.0f, 300.0f});

    auto toast = makeProbe(1, paintLog, eventLog);
    host.addAnchoredOverlay(
        toast,
        oneui::OverlayOptions::modeless(5),
        oneui::Size{120.0f, 60.0f},
        oneui::Insets{12.0f, 18.0f, 0.0f, 0.0f},
        2,
        0);

    host.paint(canvas);

    expectEqual("OverlayHost anchored paint count", static_cast<int>(paintLog.size()), 1);
    expectSamePointer("OverlayHost anchored entry child", host.overlays()[0].child.get(), toast.get());
    expectEqual("OverlayHost anchored flag", host.overlays()[0].anchored ? 1 : 0, 1);
    expectEqual("OverlayHost anchored x", static_cast<int>(toast->frame().x), 272);
    expectEqual("OverlayHost anchored y", static_cast<int>(toast->frame().y), 32);
    expectEqual("OverlayHost anchored width", static_cast<int>(toast->frame().width), 120);
    expectEqual("OverlayHost anchored height", static_cast<int>(toast->frame().height), 60);
}

void testAnchoredOverlayCanFillHostFrameWithNegativeSize() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;
    NullCanvas canvas;
    host.setFrame(oneui::Rect{10.0f, 20.0f, 400.0f, 300.0f});

    auto overlay = makeProbe(1, paintLog, eventLog);
    host.addAnchoredOverlay(
        overlay,
        oneui::OverlayOptions::modeless(5),
        oneui::Size{-1.0f, -1.0f},
        oneui::Insets{4.0f, 6.0f, 8.0f, 10.0f},
        1,
        1);

    host.paint(canvas);

    expectEqual("OverlayHost fill x", static_cast<int>(overlay->frame().x), 20);
    expectEqual("OverlayHost fill y", static_cast<int>(overlay->frame().y), 24);
    expectEqual("OverlayHost fill width", static_cast<int>(overlay->frame().width), 384);
    expectEqual("OverlayHost fill height", static_cast<int>(overlay->frame().height), 288);
}

void testRemoveOverlayRemovesOnlyMatchingEntry() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto first = makeProbe(1, paintLog, eventLog);
    auto second = makeProbe(2, paintLog, eventLog);
    auto third = makeProbe(3, paintLog, eventLog);
    auto missing = makeProbe(4, paintLog, eventLog);

    host.addOverlay(first, 0);
    host.addOverlay(second, 1);
    host.addOverlay(third, 2);

    expectEqual("OverlayHost remove existing result", host.removeOverlay(second.get()) ? 1 : 0, 1);
    expectEqual("OverlayHost remove count", static_cast<int>(host.overlays().size()), 2);
    expectSamePointer("OverlayHost remove keeps first", host.overlays()[0].child.get(), first.get());
    expectSamePointer("OverlayHost remove keeps third", host.overlays()[1].child.get(), third.get());
    expectEqual("OverlayHost remove missing result", host.removeOverlay(missing.get()) ? 1 : 0, 0);
    expectEqual("OverlayHost remove missing count", static_cast<int>(host.overlays().size()), 2);
}

void testRemoveFocusedOverlayRestoresNextFocusableOverlay() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto low = makeProbe(1, paintLog, eventLog);
    auto middle = makeProbe(2, paintLog, eventLog);
    auto high = makeProbe(3, paintLog, eventLog);
    low->focusable = true;
    middle->focusable = true;
    high->focusable = true;

    host.addOverlay(low, 0);
    host.addOverlay(middle, 5);
    host.addOverlay(high, 10);

    host.onFocusChanged(true);
    host.setFocusVisible(true);

    expectEqual("OverlayHost initially focuses highest overlay", high->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost initial focus-visible follows host", high->focusVisible() ? 1 : 0, 1);

    expectEqual("OverlayHost remove focused overlay result", host.removeOverlay(high.get()) ? 1 : 0, 1);

    expectEqual("OverlayHost removed overlay loses focus", high->focused() ? 1 : 0, 0);
    expectEqual("OverlayHost remove focused overlay restores next", middle->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost remove focused overlay preserves focus-visible", middle->focusVisible() ? 1 : 0, 1);
    expectEqual("OverlayHost remove focused overlay leaves lower unfocused", low->focused() ? 1 : 0, 0);
    expectSequence("OverlayHost remove focused overlay focus log", high->focusLog, {1, 0});
    expectSequence("OverlayHost restore next overlay focus log", middle->focusLog, {1});
}

void testRemoveFocusedOverlayDoesNotRestoreViewBeforeNextOverlay() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto base = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 80.0f, 40.0f});
    auto middle = makeProbe(2, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 120.0f, 80.0f});
    auto high = makeProbe(3, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 120.0f, 80.0f});
    base->focusable = true;
    middle->focusable = true;
    high->focusable = true;

    host.add(base);
    host.onFocusChanged(true);
    host.setFocusVisible(true);

    host.addOverlay(middle, 5);
    host.addOverlay(high, 10);
    expectEqual("OverlayHost Tab focuses high overlay", host.onKeyDown(oneui::KeyEvent{oneui::Key::Tab}) ? 1 : 0, 1);
    expectEqual("OverlayHost high overlay starts focused", high->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost base loses focus while high overlay focused", base->focused() ? 1 : 0, 0);

    expectEqual("OverlayHost remove high overlay result", host.removeOverlay(high.get()) ? 1 : 0, 1);

    expectEqual("OverlayHost middle overlay takes focus", middle->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost base stays unfocused while next overlay takes focus", base->focused() ? 1 : 0, 0);
    expectSequence("OverlayHost base is not transiently restored", base->focusLog, {1, 0});
    expectSequence("OverlayHost high overlay focus log", high->focusLog, {1, 0});
    expectSequence("OverlayHost middle overlay focus log", middle->focusLog, {1});
}

void testRemoveFocusedOverlayRestoresPreviousOverlayBeforeTopRemaining() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto base = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 80.0f, 40.0f});
    auto previous = makeProbe(2, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 120.0f, 80.0f});
    auto unrelated = makeProbe(3, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 120.0f, 80.0f});
    auto current = makeProbe(4, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 120.0f, 80.0f});
    base->focusable = true;
    previous->focusable = true;
    unrelated->focusable = false;
    current->focusable = false;

    host.add(base);
    host.onFocusChanged(true);
    host.setFocusVisible(true);

    host.addOverlay(previous, 5);
    host.addOverlay(unrelated, 8);
    host.addOverlay(current, 10);
    expectEqual("OverlayHost Tab focuses previous overlay", host.onKeyDown(oneui::KeyEvent{oneui::Key::Tab}) ? 1 : 0, 1);
    expectEqual("OverlayHost previous overlay receives focus", previous->focused() ? 1 : 0, 1);

    unrelated->focusable = true;
    current->focusable = true;
    expectEqual("OverlayHost Tab focuses current overlay", host.onKeyDown(oneui::KeyEvent{oneui::Key::Tab}) ? 1 : 0, 1);
    expectEqual("OverlayHost current overlay receives focus", current->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost previous overlay loses focus", previous->focused() ? 1 : 0, 0);

    expectEqual("OverlayHost remove current overlay result", host.removeOverlay(current.get()) ? 1 : 0, 1);

    expectEqual("OverlayHost restores historical previous overlay", previous->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost does not prefer unrelated higher remaining overlay", unrelated->focused() ? 1 : 0, 0);
    expectEqual("OverlayHost base stays unfocused during overlay history restore", base->focused() ? 1 : 0, 0);
    expectSequence("OverlayHost previous overlay focus history log", previous->focusLog, {1, 0, 1});
    expectSequence("OverlayHost unrelated overlay not focused", unrelated->focusLog, {});
    expectSequence("OverlayHost current overlay focus log", current->focusLog, {1, 0});
}

void testFocusTrapKeepsTabInsideTrappingOverlay() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto base = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 80.0f, 40.0f});
    auto lower = makeProbe(2, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 120.0f, 80.0f});
    auto modal = makeProbe(3, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 120.0f, 80.0f});
    base->focusable = true;
    lower->focusable = true;
    modal->focusable = true;

    host.add(base);
    host.onFocusChanged(true);
    host.setFocusVisible(true);

    host.addOverlay(lower, 5);
    host.addOverlay(modal, oneui::OverlayOptions{10, true});

    expectEqual("OverlayHost focus trap Tab handled", host.onKeyDown(oneui::KeyEvent{oneui::Key::Tab}) ? 1 : 0, 1);
    expectEqual("OverlayHost focus trap focuses modal", modal->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost focus trap skips lower overlay", lower->focused() ? 1 : 0, 0);
    expectEqual("OverlayHost focus trap removes base focus", base->focused() ? 1 : 0, 0);

    expectEqual("OverlayHost focus trap wraps forward", host.onKeyDown(oneui::KeyEvent{oneui::Key::Tab}) ? 1 : 0, 1);
    expectEqual("OverlayHost focus trap keeps modal focused", modal->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost focus trap still skips lower", lower->focused() ? 1 : 0, 0);

    expectEqual("OverlayHost focus trap wraps reverse", host.onKeyDown(oneui::KeyEvent{oneui::Key::Tab, true}) ? 1 : 0, 1);
    expectEqual("OverlayHost focus trap keeps modal focused after reverse", modal->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost lower overlay never receives focus", lower->focusLog.empty() ? 1 : 0, 1);
    expectSequence("OverlayHost base focus only leaves once", base->focusLog, {1, 0});
    expectSequence("OverlayHost modal focus log", modal->focusLog, {1});
}

void testClearingLastFocusedOverlayRestoresPreviousViewFocus() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto base = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 80.0f, 40.0f});
    auto overlay = makeProbe(2, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 120.0f, 80.0f});
    base->focusable = true;
    overlay->focusable = true;

    host.add(base);
    host.onFocusChanged(true);
    host.setFocusVisible(true);

    expectEqual("OverlayHost base initially focused", base->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost base initially focus-visible", base->focusVisible() ? 1 : 0, 1);

    host.addOverlay(overlay, 10);
    expectEqual("OverlayHost Tab focuses overlay", host.onKeyDown(oneui::KeyEvent{oneui::Key::Tab}) ? 1 : 0, 1);
    expectEqual("OverlayHost overlay receives focus", overlay->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost overlay receives focus-visible", overlay->focusVisible() ? 1 : 0, 1);
    expectEqual("OverlayHost base loses focus while overlay focused", base->focused() ? 1 : 0, 0);

    expectEqual("OverlayHost remove last focused overlay result", host.removeOverlay(overlay.get()) ? 1 : 0, 1);
    expectEqual("OverlayHost removed last overlay loses focus", overlay->focused() ? 1 : 0, 0);
    expectEqual("OverlayHost restores previous view focus", base->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost restores previous view focus-visible", base->focusVisible() ? 1 : 0, 1);
    expectSequence("OverlayHost base focus restore log", base->focusLog, {1, 0, 1});
    expectSequence("OverlayHost last overlay focus log", overlay->focusLog, {1, 0});
}

void testModelessOverlayDoesNotStealInitialContentFocus() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto content = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    auto toast = makeProbe(2, paintLog, eventLog, oneui::Rect{100.0f, 0.0f, 120.0f, 60.0f});
    content->focusable = true;
    toast->focusable = true;

    host.setContent(content);
    host.addOverlay(toast, oneui::OverlayOptions::modeless(10));

    host.onFocusChanged(true);

    expectEqual("OverlayHost modeless overlay does not steal content focus", content->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost modeless overlay starts unfocused", toast->focused() ? 1 : 0, 0);
    expectSequence("OverlayHost content focus log", content->focusLog, {1});
    expectSequence("OverlayHost modeless toast focus log", toast->focusLog, {});
}

void testClickingContentClearsFocusedModelessOverlay() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto content = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    auto toast = makeProbe(2, paintLog, eventLog, oneui::Rect{100.0f, 0.0f, 120.0f, 60.0f});
    content->focusable = true;
    toast->focusable = true;

    host.setContent(content);
    host.addOverlay(toast, oneui::OverlayOptions::modeless(10));
    host.onMouseDown(oneui::MouseEvent{oneui::Point{120.0f, 20.0f}});
    expectEqual("OverlayHost modeless overlay can be focused by click", toast->focused() ? 1 : 0, 1);

    eventLog.clear();
    expectEqual("OverlayHost content click handled after modeless overlay focus",
                host.onMouseDown(oneui::MouseEvent{oneui::Point{20.0f, 120.0f}}) ? 1 : 0,
                1);

    expectEqual("OverlayHost focused modeless overlay loses focus on content click", toast->focused() ? 1 : 0, 0);
    expectEqual("OverlayHost content receives focus after content click", content->focused() ? 1 : 0, 1);
    expectSequence("OverlayHost content receives click event", eventLog, {12});
}

void testContentReceivesOverlayHostInvalidationAndAnimationCallbacks() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto content = makeProbe(1, paintLog, eventLog, oneui::Rect{12.0f, 18.0f, 160.0f, 44.0f});
    int invalidations = 0;
    int rectInvalidations = 0;
    int animationSchedules = 0;

    host.setContent(content);
    host.setInvalidator([&] {
        ++invalidations;
    });
    host.setRectInvalidator([&](oneui::Rect rect) {
        ++rectInvalidations;
        expectEqual("OverlayHost content rect invalidation x", static_cast<int>(rect.x), 12);
        expectEqual("OverlayHost content rect invalidation width", static_cast<int>(rect.width), 160);
    });
    host.setAnimationScheduler([&] {
        ++animationSchedules;
    });

    content->invalidateSelf();
    content->scheduleAnimationSelf();

    expectEqual("OverlayHost content forwards rect invalidation", rectInvalidations, 1);
    expectEqual("OverlayHost content does not fall back to full invalidation when rect callback exists", invalidations, 0);
    expectEqual("OverlayHost content forwards animation scheduling", animationSchedules, 1);
}

void testOverlayHostTicksContentAndOverlayAnimations() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto content = makeProbe(1, paintLog, eventLog);
    auto overlay = makeProbe(2, paintLog, eventLog);
    content->animationRunning = true;
    overlay->animationRunning = true;

    host.setContent(content);
    host.addOverlay(overlay, oneui::OverlayOptions::modeless(10));

    expectEqual("OverlayHost ticks running children", host.tickAnimations(120.0) ? 1 : 0, 1);
    expectEqual("OverlayHost content animation ticked", content->animationTicks, 1);
    expectEqual("OverlayHost overlay animation ticked", overlay->animationTicks, 1);
}

void testClearOverlaysRemovesEntriesAndStopsDispatch() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto first = makeProbe(1, paintLog, eventLog);
    auto second = makeProbe(2, paintLog, eventLog);
    first->focusable = true;
    second->focusable = true;

    host.addOverlay(first, 0);
    host.addOverlay(second, 1);

    host.onFocusChanged(true);
    expectEqual("OverlayHost focus before clear", second->focused() ? 1 : 0, 1);

    host.clearOverlays();

    expectEqual("OverlayHost clear count", static_cast<int>(host.overlays().size()), 0);
    expectEqual("OverlayHost clear drops focused overlay", second->focused() ? 1 : 0, 0);
    expectEqual("OverlayHost clear mouse dispatch result", host.onMouseDown(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}}) ? 1 : 0, 0);
    expectSequence("OverlayHost clear stops overlay dispatch", eventLog, {});
}

void testMouseDownDispatchesToHighestHitLayerFirst() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto low = makeProbe(1, paintLog, eventLog);
    auto high = makeProbe(2, paintLog, eventLog);
    auto laterSameLayer = makeProbe(3, paintLog, eventLog);

    host.addOverlay(low, 0);
    host.addOverlay(high, 10);
    host.addOverlay(laterSameLayer, 10);

    expectEqual("OverlayHost high-layer mouse-down handled", host.onMouseDown(oneui::MouseEvent{oneui::Point{20.0f, 20.0f}}) ? 1 : 0, 1);
    expectSequence("OverlayHost high-layer mouse-down order", eventLog, {32});

    eventLog.clear();
    laterSameLayer->setDisabled(true);

    expectEqual("OverlayHost disabled top mouse-down handled", host.onMouseDown(oneui::MouseEvent{oneui::Point{20.0f, 20.0f}}) ? 1 : 0, 1);
    expectSequence("OverlayHost disabled top is skipped", eventLog, {22});
}

void testMouseEventsRespectHitTestAndPressedOverlay() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto wideLow = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 200.0f, 200.0f});
    auto smallHigh = makeProbe(2, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 40.0f, 40.0f});

    host.addOverlay(wideLow, 0);
    host.addOverlay(smallHigh, 10);

    expectEqual("OverlayHost point outside top hits lower overlay", host.onMouseDown(oneui::MouseEvent{oneui::Point{80.0f, 80.0f}}) ? 1 : 0, 1);
    host.onMouseUp(oneui::MouseEvent{oneui::Point{20.0f, 20.0f}});

    expectSequence("OverlayHost pressed overlay receives mouse-up", eventLog, {12, 13});

    eventLog.clear();
    wideLow->setVisible(false);

    expectEqual("OverlayHost hidden lower outside top not handled", host.onMouseDown(oneui::MouseEvent{oneui::Point{80.0f, 80.0f}}) ? 1 : 0, 0);
    expectSequence("OverlayHost hidden lower receives no event", eventLog, {});
}

void testPressedContentReceivesMouseMoveOutsideItsHitArea() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto content = makeProbe(
        1,
        paintLog,
        eventLog,
        oneui::Rect{0.0f, 0.0f, 80.0f, 80.0f});
    host.setContent(content);

    expectEqual(
        "OverlayHost content mouse-down handled",
        host.onMouseDown(oneui::MouseEvent{oneui::Point{20.0f, 20.0f}}) ? 1 : 0,
        1);
    expectEqual(
        "OverlayHost pressed content move handled outside hit area",
        host.onMouseMove(oneui::MouseEvent{oneui::Point{140.0f, 120.0f}}) ? 1 : 0,
        1);
    host.onMouseUp(oneui::MouseEvent{oneui::Point{140.0f, 120.0f}});

    expectSequence(
        "OverlayHost pressed content keeps pointer capture",
        eventLog,
        {12, 11, 13});
}

void testNestedTerminalKeepsPointerCaptureThroughContainerTree() {
    oneui::OverlayHost host;
    host.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 120.0f});

    auto root = std::make_shared<oneui::Panel>();
    auto column = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
    auto surface = std::make_shared<oneui::Panel>();
    auto stage = std::make_shared<oneui::Panel>();
    auto terminal = std::make_shared<oneui::TerminalView>();
    terminal->setFontSize(20.0f);
    terminal->setGrid(
        1,
        8,
        {
            oneui::TerminalCell{L"A"},
            oneui::TerminalCell{L"B"},
            oneui::TerminalCell{L"C"},
            oneui::TerminalCell{L"D"},
            oneui::TerminalCell{L"E"},
            oneui::TerminalCell{L"F"},
            oneui::TerminalCell{L"G"},
            oneui::TerminalCell{L"H"},
        });

    stage->setContent(terminal);
    surface->setContent(stage);
    column->add(surface);
    root->setContent(column);
    host.setContent(root);

    NullCanvas canvas;
    host.paint(canvas);

    expectEqual(
        "nested terminal mouse-down handled",
        host.onMouseDown(oneui::MouseEvent{{1.0f, 10.0f}, oneui::MouseButton::Left}) ? 1 : 0,
        1);
    expectEqual(
        "nested terminal mouse-move handled",
        host.onMouseMove(oneui::MouseEvent{{49.0f, 10.0f}, oneui::MouseButton::Left}) ? 1 : 0,
        1);
    host.onMouseUp(oneui::MouseEvent{{49.0f, 10.0f}, oneui::MouseButton::Left});

    expectEqual(
        "nested terminal drag extends selection through every container",
        terminal->selectedText().size() > 1 ? 1 : 0,
        1);
}

void testMouseWheelUsesHighestHandledHitOverlay() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto low = makeProbe(1, paintLog, eventLog);
    auto high = makeProbe(2, paintLog, eventLog);
    high->handleMouseWheel = false;

    host.addOverlay(low, 0);
    host.addOverlay(high, 10);

    expectEqual("OverlayHost wheel bubbles past unhandled top", host.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, 1.0f}) ? 1 : 0, 1);
    expectSequence("OverlayHost wheel hit-test order", eventLog, {24, 14});
}

void testPointerBlockerConsumesOutsideEventsBeforeLowerTargets() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto lower = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    auto modal = makeProbe(2, paintLog, eventLog, oneui::Rect{20.0f, 20.0f, 80.0f, 60.0f});
    lower->focusable = true;
    modal->focusable = true;

    host.addOverlay(lower, 0);
    host.addOverlay(modal, oneui::OverlayOptions{10, true, true});

    expectEqual("OverlayHost pointer blocker outside mouse-down consumed",
                host.onMouseDown(oneui::MouseEvent{oneui::Point{160.0f, 120.0f}}) ? 1 : 0,
                1);
    expectEqual("OverlayHost pointer blocker outside mouse-up consumed",
                host.onMouseUp(oneui::MouseEvent{oneui::Point{160.0f, 120.0f}}) ? 1 : 0,
                1);
    expectEqual("OverlayHost pointer blocker outside wheel consumed",
                host.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{160.0f, 120.0f}, 1.0f}) ? 1 : 0,
                1);
    expectSequence("OverlayHost pointer blocker keeps lower untouched", eventLog, {});

    expectEqual("OverlayHost pointer blocker inside mouse-down reaches modal",
                host.onMouseDown(oneui::MouseEvent{oneui::Point{30.0f, 30.0f}}) ? 1 : 0,
                1);
    expectSequence("OverlayHost pointer blocker inside modal dispatch", eventLog, {22});
    expectEqual("OverlayHost pointer blocker focuses modal", modal->focused() ? 1 : 0, 1);
    expectEqual("OverlayHost pointer blocker does not focus lower", lower->focused() ? 1 : 0, 0);
}

void testHigherOverlayCanReceivePointerAboveBlocker() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto lower = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    auto modal = makeProbe(2, paintLog, eventLog, oneui::Rect{20.0f, 20.0f, 80.0f, 60.0f});
    auto toast = makeProbe(3, paintLog, eventLog, oneui::Rect{150.0f, 100.0f, 60.0f, 50.0f});

    host.addOverlay(lower, 0);
    host.addOverlay(modal, oneui::OverlayOptions{10, true, true});
    host.addOverlay(toast, 20);

    expectEqual("OverlayHost pointer above blocker reaches higher overlay",
                host.onMouseDown(oneui::MouseEvent{oneui::Point{170.0f, 120.0f}}) ? 1 : 0,
                1);
    expectSequence("OverlayHost pointer above blocker dispatch order", eventLog, {32});
}

void testPopupWithOutsideCloseDisabledDoesNotBlockLowerOverlay() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto low = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 220.0f, 180.0f});
    auto popup = std::make_shared<oneui::Popup>();
    popup->setFrame(oneui::Rect{20.0f, 20.0f, 120.0f, 120.0f});
    popup->setAnchor(std::make_shared<oneui::Button>(L"Menu"));
    popup->anchor()->setPreferredSize(oneui::Size{70.0f, 30.0f});
    popup->setContent(makeProbe(2, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 90.0f, 40.0f}));
    popup->content()->setPreferredSize(oneui::Size{90.0f, 40.0f});
    popup->setOpen(true);
    popup->setCloseOnOutsideClick(false);

    host.addOverlay(low, 0);
    host.addOverlay(popup, 10);

    expectEqual("Popup non-closing outside click falls through", host.onMouseDown(oneui::MouseEvent{oneui::Point{190.0f, 150.0f}}) ? 1 : 0, 1);
    expectSequence("Popup non-closing outside click reaches lower overlay", eventLog, {12});
}

void testFillOverlayGivesPopupAViewportForMenuLayout() {
    oneui::OverlayHost host;
    host.setFrame(oneui::Rect{0.0f, 0.0f, 1600.0f, 1000.0f});

    auto anchor = std::make_shared<oneui::View>();
    anchor->setVisible(false);
    auto menu = std::make_shared<oneui::Menu>();
    menu->addHeader(L"Production", L"root@10.0.0.1:22");
    menu->addItem(L"Connect", oneui::IconSymbol::Terminal);
    menu->addItem(L"Delete", oneui::IconSymbol::Trash, true);
    menu->setPreferredSize(oneui::Size{224.0f, menu->preferredHeight()});

    auto popup = std::make_shared<oneui::Popup>();
    popup->setAnchor(anchor);
    popup->setAnchorRect(oneui::Rect{700.0f, 270.0f, 1.0f, 1.0f});
    popup->setContent(menu);
    popup->setOpen(true);

    host.addAnchoredOverlay(
        popup,
        oneui::OverlayOptions::modeless(100),
        oneui::Size{-1.0f, -1.0f},
        oneui::Insets{},
        0,
        0);

    NullCanvas canvas;
    host.paint(canvas);

    expectEqual("Fill popup overlay width", static_cast<int>(popup->frame().width), 1600);
    expectEqual("Fill popup overlay height", static_cast<int>(popup->frame().height), 1000);
    expectEqual("Popup menu x includes surface padding", static_cast<int>(menu->frame().x), 708);
    expectEqual("Popup menu y includes offset and padding", static_cast<int>(menu->frame().y), 285);
    expectEqual("Popup menu width excludes surface padding", static_cast<int>(menu->frame().width), 208);
    expectEqual(
        "Popup menu keeps non-zero natural height",
        menu->frame().height > 0.0f ? 1 : 0,
        1);
}

void testPopupBlockOutsidePolicyStopsLowerOverlayWithoutClosing() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto low = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 220.0f, 180.0f});
    auto popup = std::make_shared<oneui::Popup>();
    popup->setFrame(oneui::Rect{20.0f, 20.0f, 120.0f, 120.0f});
    popup->setAnchor(std::make_shared<oneui::Button>(L"Menu"));
    popup->anchor()->setPreferredSize(oneui::Size{70.0f, 30.0f});
    popup->setContent(makeProbe(2, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 90.0f, 40.0f}));
    popup->content()->setPreferredSize(oneui::Size{90.0f, 40.0f});
    popup->setOpen(true);
    popup->setOutsidePointerPolicy(oneui::PopupOutsidePointerPolicy::Block);

    host.addOverlay(low, 0);
    host.addOverlay(popup, 10);

    expectEqual("Popup block outside policy consumes outside click",
                host.onMouseDown(oneui::MouseEvent{oneui::Point{190.0f, 150.0f}}) ? 1 : 0,
                1);
    expectEqual("Popup block outside policy keeps popup open", popup->isOpen() ? 1 : 0, 1);
    expectSequence("Popup block outside policy keeps lower untouched", eventLog, {});
}

void testPopupModalModeCombinesFocusTrapAndPointerBlocker() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto low = makeProbe(1, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 220.0f, 180.0f});
    low->focusable = true;

    auto popup = std::make_shared<oneui::Popup>();
    popup->setFrame(oneui::Rect{20.0f, 20.0f, 120.0f, 120.0f});
    popup->setAnchor(std::make_shared<oneui::Button>(L"Dialog"));
    popup->anchor()->setPreferredSize(oneui::Size{70.0f, 30.0f});
    popup->setContent(std::make_shared<oneui::Button>(L"OK"));
    popup->content()->setPreferredSize(oneui::Size{90.0f, 40.0f});
    popup->setOpen(true);
    popup->setInteractionMode(oneui::PopupInteractionMode::Modal);

    host.addOverlay(low, 0);
    host.addOverlay(popup, popup->overlayOptions(10));

    expectEqual("Popup modal overlay traps focus", host.overlays()[1].trapsFocus ? 1 : 0, 1);
    expectEqual("Popup modal overlay blocks outside pointer", host.overlays()[1].blocksOutsidePointer ? 1 : 0, 1);

    expectEqual("Popup modal outside click is consumed",
                host.onMouseDown(oneui::MouseEvent{oneui::Point{190.0f, 150.0f}}) ? 1 : 0,
                1);
    expectEqual("Popup modal keeps popup open after outside click", popup->isOpen() ? 1 : 0, 1);
    expectSequence("Popup modal keeps lower overlay untouched", eventLog, {});

    expectEqual("Popup modal Tab handled by focus trap", host.onKeyDown(oneui::KeyEvent{oneui::Key::Tab}) ? 1 : 0, 1);
    expectEqual("Popup modal focuses popup", popup->focused() ? 1 : 0, 1);
    expectEqual("Popup modal skips lower focus", low->focused() ? 1 : 0, 0);
}

void testFocusedPopupReceivesEscapeThroughOverlayHost() {
    std::vector<int> paintLog;
    std::vector<int> eventLog;
    oneui::OverlayHost host;

    auto popup = std::make_shared<oneui::Popup>();
    popup->setFrame(oneui::Rect{20.0f, 20.0f, 120.0f, 120.0f});
    popup->setAnchor(std::make_shared<oneui::Button>(L"Menu"));
    popup->anchor()->setPreferredSize(oneui::Size{70.0f, 30.0f});
    popup->setContent(makeProbe(2, paintLog, eventLog, oneui::Rect{0.0f, 0.0f, 90.0f, 40.0f}));
    popup->content()->setPreferredSize(oneui::Size{90.0f, 40.0f});
    popup->setOpen(true);
    popup->setCloseOnEscape(true);

    host.addOverlay(popup, 10);
    host.onFocusChanged(true);

    expectEqual("OverlayHost focused popup handles Escape", host.onKeyDown(oneui::KeyEvent{oneui::Key::Escape}) ? 1 : 0, 1);
    expectEqual("OverlayHost Escape closes focused popup", popup->isOpen() ? 1 : 0, 0);
}

void testCommittedTextAndProgrammaticFocusReachNestedTerminal() {
    oneui::OverlayHost host;
    auto content = std::make_shared<oneui::View>();
    auto terminal = std::make_shared<oneui::TerminalView>();
    std::wstring received;
    terminal->setOnTextInput([&](const std::wstring& text) { received += text; });
    content->add(terminal);
    host.setContent(content);

    expectEqual(
        "OverlayHost requestFocus finds nested terminal",
        host.requestFocus(terminal.get(), false) ? 1 : 0,
        1);

    const std::wstring committed{
        L'A',
        static_cast<wchar_t>(0xD83D),
        static_cast<wchar_t>(0xDE80),
    };
    expectEqual(
        "OverlayHost routes committed text batch",
        host.onTextInputText(committed) ? 1 : 0,
        1);
    expectEqual(
        "OverlayHost preserves committed text batch",
        received == committed ? 1 : 0,
        1);
}

} // namespace

int main() {
    testAddOverlayPreservesEntryOrderAndLayerValues();
    testPaintOrdersByLayerWithStableEqualLayers();
    testModalOverlayPaintsTheStandardBackdrop();
    testAnchoredOverlayLaysOutAgainstHostFrame();
    testAnchoredOverlayCanFillHostFrameWithNegativeSize();
    testRemoveOverlayRemovesOnlyMatchingEntry();
    testRemoveFocusedOverlayRestoresNextFocusableOverlay();
    testRemoveFocusedOverlayDoesNotRestoreViewBeforeNextOverlay();
    testRemoveFocusedOverlayRestoresPreviousOverlayBeforeTopRemaining();
    testFocusTrapKeepsTabInsideTrappingOverlay();
    testClearingLastFocusedOverlayRestoresPreviousViewFocus();
    testModelessOverlayDoesNotStealInitialContentFocus();
    testClickingContentClearsFocusedModelessOverlay();
    testContentReceivesOverlayHostInvalidationAndAnimationCallbacks();
    testOverlayHostTicksContentAndOverlayAnimations();
    testClearOverlaysRemovesEntriesAndStopsDispatch();
    testMouseDownDispatchesToHighestHitLayerFirst();
    testMouseEventsRespectHitTestAndPressedOverlay();
    testPressedContentReceivesMouseMoveOutsideItsHitArea();
    testNestedTerminalKeepsPointerCaptureThroughContainerTree();
    testMouseWheelUsesHighestHandledHitOverlay();
    testPointerBlockerConsumesOutsideEventsBeforeLowerTargets();
    testHigherOverlayCanReceivePointerAboveBlocker();
    testPopupWithOutsideCloseDisabledDoesNotBlockLowerOverlay();
    testFillOverlayGivesPopupAViewportForMenuLayout();
    testPopupBlockOutsidePolicyStopsLowerOverlayWithoutClosing();
    testPopupModalModeCombinesFocusTrapAndPointerBlocker();
    testFocusedPopupReceivesEscapeThroughOverlayHost();
    testCommittedTextAndProgrammaticFocusReachNestedTerminal();

    if (failures != 0) {
        std::cerr << failures << " overlay host behavior test(s) failed\n";
        return 1;
    }

    return 0;
}
