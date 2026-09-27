#include "oneui/layout/stack.h"
#include "oneui/controls/label.h"
#include "oneui/layout/scroll_view.h"
#include "support/recording_canvas.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <algorithm>

using namespace oneui;
using oneui::test_support::RecordingCanvas;
namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
void near(float value, float expected, const char* message) { check(std::abs(value - expected) < 0.02f, message); }
struct Probe : Widget {
    Size size{100, 30};
    mutable int calls = 0;
    Size naturalSize() const override { ++calls; return size; }
    void paint(Canvas&) override {}
    void update(Size next) { size = next; invalidate(); }
};
auto add(Stack& stack, float grow = 0) {
    auto item = std::make_shared<Probe>();
    stack.add(item);
    stack.setFlex(item, {grow, 0, {}, 0, INFINITY, true});
    return item;
}
void wrapAndCache() {
    Stack row(StackDirection::Row);
    check(!row.setWrap(true), "legacy rejects unsupported wrapping");
    check(row.setEngine(StackEngine::Yoga), "Yoga available");
    check(row.setWrap(true), "Yoga accepts wrapping");
    row.setGap(10);
    row.setAlign(StackAlign::Start);
    auto a = add(row), b = add(row), c = add(row);
    near(row.measure({210, INFINITY}).height, 70, "two wrapping rows measured");
    RecordingCanvas canvas;
    row.setFrame({20, 50, 210, 120});
    row.paint(canvas);
    near(a->frame().x, 20, "absolute origin");
    near(b->frame().x, 130, "second item same row");
    near(c->frame().y, 90, "third item wraps");
    const int calls = a->calls + b->calls + c->calls;
    row.paint(canvas);
    near(float(a->calls + b->calls + c->calls), float(calls), "stable paint reuses measure cache");
    a->update({150, 30});
    row.paint(canvas);
    check(a->calls + b->calls + c->calls > calls, "content invalidates measurement");
    near(b->frame().y, 90, "changed width reflows neighbors");
    b->setVisible(false);
    row.paint(canvas);
    near(c->frame().y, 90, "hidden item contributes no gap");
    row.setFrame({0, 0, 120, 160});
    row.paint(canvas);
    near(c->frame().x, 0, "resize recomputes positions");
    row.clearChildren();
    auto replacement = add(row);
    row.paint(canvas);
    near(replacement->frame().y, 0, "replaced tree has no stale nodes");
    row.setEngine(StackEngine::Legacy);
    row.paint(canvas);
    near(replacement->frame().width, 100, "switch back to legacy");
}
void constraints() {
    Stack row(StackDirection::Row); row.setEngine(StackEngine::Yoga); row.setGap(10);
    auto a = add(row, 1), b = add(row, 2);
    row.setFlex(a, {1, 1, 100, 80, 120, true});
    row.setFlex(b, {2, 1, 100, 60, INFINITY, true});
    RecordingCanvas canvas;
    row.setFrame({0, 0, 400, 60}); row.paint(canvas);
    near(a->frame().width, 120, "growth freezes at max");
    near(b->frame().width, 270, "growth remainder redistributed");
    row.setFrame({0, 0, 110, 60}); row.paint(canvas);
    near(a->frame().width, 80, "minimum preserved");
    near(b->frame().width, 60, "overflow honors minimums");
    row.setDirection(StackDirection::Column);
    row.setFrame({0, 0, 200, 400}); row.paint(canvas);
    near(a->frame().height, 120, "direction change clears old-axis constraints");
    near(a->frame().width, 200, "cross axis stretched after direction change");
}
void paragraphsAndNested() {
    Stack page; page.setEngine(StackEngine::Yoga); page.setPadding(Insets{12});
    auto body = std::make_shared<Stack>(); body->setEngine(StackEngine::Yoga);
    body->setPadding(Insets{8}); body->setGap(12);
    auto label = std::make_shared<Label>(L"中文段落应随容器宽度换行，并推动后续控件向下排列。Long text must retain its natural line height under resizing.");
    label->setTextWrapping(true); label->setFontSize(16);
    check(label->measure({INFINITY, INFINITY}).height >= 22, "unbounded paragraph preserves line height");
    body->add(label); body->setFlex(label, {0, 0, {}, 0, INFINITY, true});
    auto after = add(*body);
    page.add(body); page.setFlex(body, {0, 0, {}, 0, INFINITY, true});
    const auto wide = page.measure({600, INFINITY});
    const auto narrow = page.measure({260, INFINITY});
    check(narrow.height > wide.height, "nested paragraph height depends on width");
    RecordingCanvas canvas;
    page.setFrame({15, 20, 260, 600}); page.paint(canvas);
    near(label->frame().width, 220, "nested padding bounds paragraph width");
    check(after->frame().y >= label->frame().y + label->frame().height + 11.9f, "no overlap after wrapped paragraph");
    near(body->frame().height + 24, narrow.height, "measure agrees with arranged content height");
    const auto old = label->frame().height;
    label->setFontSize(24); page.paint(canvas);
    check(label->frame().height > old, "font changes invalidate Yoga measure cache");
    label->setMaxLines(2); page.paint(canvas);
    check(label->frame().height <= 68, "max-lines limits measured height");
    page.setTextEnvironment(L"Microsoft YaHei UI", 1.5f); page.paint(canvas);
    check(std::isfinite(after->frame().y), "DPI environment change remains finite");
    page.setFrame({0, 0, 0, 0}); page.paint(canvas);
    check(std::isfinite(label->frame().height), "zero size remains finite");
}
void lineBoxesAgreeWithPaint() {
    Label label(L"文案变长以后，标题和操作会自动换行，保持完整。");
    label.setTextEnvironment(L"Microsoft YaHei UI", 1.0f);
    label.setFontSize(28); label.setFontWeight(700); label.setTextWrapping(true);
    const auto measured = label.measure({570, INFINITY});
    label.setFrame({0, 0, 570, measured.height});
    RecordingCanvas canvas; label.paint(canvas);
    check(!canvas.textBlocks.empty() && canvas.textBlocks.back().style.maxLines >= 2,
          "fractional line boxes do not ellipsize a measured two-line heading");
}
void scrollAndExplicitBasis() {
    auto page = std::make_shared<Stack>(); page->setEngine(StackEngine::Yoga);
    auto row = std::make_shared<Stack>(StackDirection::Row); row->setEngine(StackEngine::Yoga);
    row->setWrap(true); row->setGap(12); row->setAlign(StackAlign::Center);
    auto label = std::make_shared<Label>(L"Heading with a long title which wraps");
    label->setTextWrapping(true); label->setFontSize(28);
    row->add(label); row->setFlex(label, {1, 1, 300, 0, INFINITY, false});
    auto action = add(*row);
    page->add(row); page->setFlex(row, {0, 0, {}, 0, INFINITY, true});
    auto tail = add(*page); tail->update({100, 400});
    ScrollView scroll; scroll.setContent(page); scroll.setFrame({0, 0, 260, 180});
    RecordingCanvas canvas; scroll.paint(canvas);
    check(label->frame().height >= 39, "explicit width basis measures large text height");
    check(action->frame().y >= label->frame().y + label->frame().height, "header action wraps after title");
    check(scroll.maxScrollOffset() > 200, "scroll extent includes width-aware wrapped content");
    scroll.setScrollOffset(scroll.maxScrollOffset()); scroll.paint(canvas);
    check(tail->frame().y + tail->frame().height <= 180.1f, "last content remains reachable");
    scroll.setFrame({0, 0, 1000, 1000}); scroll.paint(canvas);
    near(scroll.scrollOffset(), 0, "resize clears stale scroll offset");
}
void benchmark(StackEngine engine) {
    Stack row(StackDirection::Row); row.setEngine(engine); row.setGap(2);
    for (int i = 0; i < 200; ++i) add(row, 1);
    RecordingCanvas canvas;
    row.setFrame({0, 0, 30000, 100}); row.paint(canvas);
    std::vector<double> timings;
    for (int i = 0; i < 1200; ++i) {
        row.setFrame({0, 0, float(30000 + i % 200), 100});
        const auto start = std::chrono::steady_clock::now();
        row.paint(canvas);
        timings.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count());
    }
    std::sort(timings.begin(), timings.end());
    std::cout << (engine == StackEngine::Yoga ? "Yoga" : "Legacy") << " 200 no-op leaves, resize, microseconds: median="
              << timings[600] << " p95=" << timings[1140] << '\n';
}
}
int main(int argc, char**) {
    wrapAndCache(); constraints(); paragraphsAndNested(); scrollAndExplicitBasis(); lineBoxesAgreeWithPaint();
    if (argc > 1) { benchmark(StackEngine::Legacy); benchmark(StackEngine::Yoga); }
    return failures ? 1 : 0;
}
