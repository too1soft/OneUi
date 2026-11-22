#include "oneui/controls/button.h"
#include "oneui/controls/label.h"
#include "oneui/controls/terminal_view.h"
#include "oneui/controls/text_field.h"
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

bool resizedChildRepaintsMovedSibling(StackEngine outerEngine, StackEngine innerEngine) {
    struct Counter final : Widget {
        int paints = 0;
        void paint(Canvas&) override { ++paints; }
        Rect paintBounds() const override {
            const Rect r = frame();
            return {r.x - 16, r.y - 16, r.width + 32, r.height + 32};
        }
        void repaint() { invalidatePaint(); }
    };
    auto root = std::make_shared<Stack>();
    auto inner = std::make_shared<Stack>();
    root->setEngine(outerEngine);
    inner->setEngine(innerEngine);
    root->add(inner);
    root->setFlex(inner, {1, 0, 400.0f});
    auto resized = std::make_shared<Counter>();
    auto sibling = std::make_shared<Counter>();
    resized->setPreferredSize({200, 20});
    sibling->setPreferredSize({200, 30});
    inner->add(resized);
    inner->add(sibling);
    root->setFrame({20, 30, 400, 400});
    test_support::RecordingCanvas canvas;
    root->paint(canvas);
    const Rect before = sibling->paintBounds();
    Rect dirty{};
    root->setRectInvalidator([&](Rect rect) { dirty = rect; });
    const auto revision = root->measureRevision();
    resized->repaint();
    const bool paintOnlyIsLocal = root->measureRevision() == revision && dirty.height < 120;
    resized->setPreferredSize({200, 180});
    // Include the Win32 renderer's generous 48px dirty margin: the sibling
    // must still be repainted when layout moves it beyond that margin.
    canvas.clipOverride = Rect{dirty.x - 48, dirty.y - 48, dirty.width + 96, dirty.height + 96};
    root->paint(canvas);
    const Rect after = sibling->paintBounds();
    const auto contains = [](Rect outer, Rect item) {
        return outer.x <= item.x && outer.y <= item.y &&
            outer.x + outer.width >= item.x + item.width &&
            outer.y + outer.height >= item.y + item.height;
    };
    std::cout << "sibling relocation: beforeY=" << before.y << ", afterY=" << after.y
              << ", dirtyHeight=" << dirty.height << ", siblingPaints=" << sibling->paints << '\n';
    if (!paintOnlyIsLocal || after.y <= before.y + 100 || sibling->paints != 2 ||
        !contains(dirty, before) || !contains(dirty, after)) return false;

    root->setFrame({20, 30, 400, 520});
    resized->setPreferredSize({200, 240});
    const auto beforeResizePaints = sibling->paints;
    canvas.clipOverride = dirty;
    root->paint(canvas);
    return sibling->paints == beforeResizePaints + 1 && contains(dirty, sibling->paintBounds());
}

bool childDamageDoesNotScanSiblings() {
    struct BoundsCounter final : Widget {
        mutable unsigned reads = 0;
        void paint(Canvas&) override {}
        Rect paintBounds() const override { ++reads; return frame(); }
        void repaint() { invalidatePaint(); }
    };
    auto root = std::make_shared<View>();
    auto inner = std::make_shared<View>();
    std::vector<std::shared_ptr<BoundsCounter>> siblings;
    root->add(inner);
    root->setFrame({0, 0, 800, 600});
    inner->setFrame(root->frame());
    for (int i = 0; i < 400; ++i) {
        auto child = std::make_shared<BoundsCounter>();
        inner->add(child);
        siblings.push_back(std::move(child));
    }
    Rect damage{};
    root->setRectInvalidator([&](Rect rect) { damage = rect; });
    for (const auto& child : siblings) child->reads = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 200; ++i) siblings.front()->setPreferredSize({200, 20.f + i});
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    unsigned reads = 0;
    for (const auto& child : siblings) reads += child->reads;
    std::cout << "measure damage: 200 changes, 400 siblings, boundsReads=" << reads
              << ", elapsedMs=" << elapsed << '\n';
    const bool bounded = reads <= 400 && siblings.back()->reads == 0;
    siblings.front()->setFrame({1000, 800, 100, 100});
    siblings.front()->setPreferredSize({100, 100});
    const bool overflowPreserved = damage.x <= 0 && damage.y <= 0 &&
        damage.x + damage.width >= 1100 && damage.y + damage.height >= 900;
    const auto revision = root->measureRevision();
    siblings.front()->repaint();
    const bool overflowPaintLocal = root->measureRevision() == revision &&
        damage.x == 1000 && damage.y == 800 && damage.width == 100 && damage.height == 100;
    return bounded && overflowPreserved && overflowPaintLocal;
}

bool clipChecksSkipVisibleDescendantBoundsAndKeepOverflow() {
    struct Leaf final : Widget {
        mutable unsigned boundsReads = 0;
        unsigned paints = 0;
        Rect paintBounds() const override { ++boundsReads; return frame(); }
        void paint(Canvas&) override { ++paints; }
    };
    View root;
    auto visible = std::make_shared<View>();
    visible->setFrame({0, 0, 100, 100});
    auto visibleLeaf = std::make_shared<Leaf>();
    visibleLeaf->setFrame({5, 5, 20, 20});
    visible->add(visibleLeaf);
    root.add(visible);

    auto outside = std::make_shared<View>();
    outside->setFrame({120, 15, 30, 30});
    auto overflow = std::make_shared<Leaf>();
    overflow->setFrame({22, 22, 20, 20});
    outside->add(overflow);
    auto clipped = std::make_shared<Leaf>();
    clipped->setFrame({125, 20, 15, 15});
    outside->add(clipped);
    root.add(outside);

    test_support::RecordingCanvas canvas;
    canvas.clipOverride = Rect{0, 0, 80, 80};
    root.paint(canvas);
    return visibleLeaf->paints == 1 && visibleLeaf->boundsReads == 0 &&
        overflow->paints == 1 && overflow->boundsReads == 1 && clipped->paints == 0;
}

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

bool terminalOutputPreservesLayout() {
    auto root = std::make_shared<Stack>();
    auto terminal = std::make_shared<TerminalView>();
    root->setEngine(StackEngine::Yoga);
    root->add(terminal);
    root->setFrame({0, 0, 800, 600});
    terminal->setGrid(4, 12, {});
    test_support::RecordingCanvas canvas;
    root->paint(canvas);
    const auto before = root->layoutStats().calculations;
    Rect damage{};
    root->setRectInvalidator([&](Rect rect) { damage = rect; });
    terminal->updateCells(1, {TerminalCell{L"X"}});
    if (damage.width <= 0 || damage.width >= root->frame().width) return false;
    terminal->setCursor({0, 1, true});
    terminal->setScrollback(25, 0);
    root->paint(canvas);
    if (root->layoutStats().calculations != before) return false;
    terminal->setFontSize(terminal->fontSize() + 1);
    root->paint(canvas);
    return root->layoutStats().calculations > before;
}

bool textEditingPreservesAssignedLayout() {
    auto root = std::make_shared<Stack>();
    auto field = std::make_shared<TextField>(L"Filter");
    root->setEngine(StackEngine::Yoga);
    root->add(field);
    root->setFrame({0, 0, 800, 600});
    field->onFocusChanged(true);
    test_support::RecordingCanvas canvas;
    root->paint(canvas);
    Rect damage{};
    root->setRectInvalidator([&](Rect rect) { damage = rect; });
    const auto revision = root->measureRevision();
    const auto calculations = root->layoutStats().calculations;
    if (!field->onTextCommitted(L"search")) return false;
    const Rect box = field->frame();
    const bool editDamageIsLocal = damage.x == box.x && damage.y == box.y &&
        damage.width == box.width && damage.height == box.height;
    field->setSelectionRange(1, 5);
    field->setTextComposition(L"ほ", 1);
    field->setTextComposition({}, 0);
    if (!field->undo() || !field->text().empty()) return false;
    root->paint(canvas);
    if (!editDamageIsLocal || root->measureRevision() != revision ||
        root->layoutStats().calculations != calculations) return false;

    struct ContentSizedField final : TextField {
        Size naturalSize() const override { return {static_cast<float>(text().size()), 40}; }
    } custom;
    const auto customRevision = custom.measureRevision();
    custom.setText(L"Content size changes");
    return custom.measureRevision() != customRevision && custom.naturalSize().width > 0;
}

int main() {
    if (!clipChecksSkipVisibleDescendantBoundsAndKeepOverflow()) {
        std::cerr << "FAIL: clip bounds traversal or overflow paint changed\n";
        return 1;
    }
    {
        Label label(L"Settings");
        const auto revision = label.measureRevision();
        int redraws = 0;
        label.setInvalidator([&] { ++redraws; });
        label.setFontSize(label.fontSize());
        label.setFontWeight(label.fontWeight());
        label.setColor({12, 34, 56});
        label.setColor({12, 34, 56});
        if (label.measureRevision() != revision || redraws != 1) {
            std::cerr << "FAIL: label style replay or recoloring invalidated measure\n";
            return 1;
        }
        label.setFontSize(label.fontSize() + 1);
        if (label.measureRevision() == revision) {
            std::cerr << "FAIL: label font size change did not invalidate measure\n";
            return 1;
        }
    }
    if (!childDamageDoesNotScanSiblings()) {
        std::cerr << "FAIL: child damage propagation scanned unrelated siblings\n";
        return 1;
    }
    if (!terminalOutputPreservesLayout()) {
        std::cerr << "FAIL: terminal paint damage expanded or invalidated layout\n";
        return 1;
    }
    if (!textEditingPreservesAssignedLayout()) {
        std::cerr << "FAIL: text edit damage expanded or custom measurement was skipped\n";
        return 1;
    }
    bool layoutDamagePassed = true;
    for (const auto outer : {StackEngine::Legacy, StackEngine::Yoga})
        for (const auto inner : {StackEngine::Legacy, StackEngine::Yoga})
            layoutDamagePassed = resizedChildRepaintsMovedSibling(outer, inner) && layoutDamagePassed;
    if (!layoutDamagePassed) {
        std::cerr << "FAIL: layout invalidation omitted the moved sibling from the dirty clip\n";
        return 1;
    }
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
    const auto hoverStart = std::chrono::steady_clock::now();
    button->onMouseMove({{frame.x + 1, frame.y + 1}});
    const int hoverRedraws = redraws;
    const double hoverStartMs = std::chrono::duration<double, std::milli>(hoverStart.time_since_epoch()).count();
    if (!button->tickAnimations(hoverStartMs + 4) || redraws != hoverRedraws) {
        std::cerr << "FAIL: color samples that repeat the first frame scheduled a redraw\n";
        return 1;
    }
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
