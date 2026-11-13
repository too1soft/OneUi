#include "internal/input_layout.h"
#include "oneui/controls/popup.h"
#include "oneui/view.h"
#include <iostream>

using namespace oneui;
namespace {
int failures = 0;
void check(bool ok, const char* name) {
    std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
    if (!ok) ++failures;
}
bool same(Rect a, Rect b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
struct Target final : Widget {
    int clicks = 0;
    void paint(Canvas&) override {}
    bool onMouseDown(const MouseEvent&) override { return true; }
    bool onMouseUp(const MouseEvent&) override { ++clicks; return true; }
};
struct MenuBody final : View {
    int layouts = 0;
    float rowHeight = 32;
    std::shared_ptr<Target> first = std::make_shared<Target>(), second = std::make_shared<Target>();
    MenuBody() { add(first); add(second); setPreferredSize({200, 64}); }
    void resizeRows(float height) { rowHeight = height; setPreferredSize({200, height * 2}); }
    void layoutChildren() override {
        ++layouts;
        const auto r = frame();
        first->setFrame({r.x, r.y, r.width, rowHeight});
        second->setFrame({r.x, r.y + rowHeight, r.width, rowHeight});
    }
};
struct Anchor final : View {
    int layouts = 0;
    Anchor() { setPreferredSize({120, 35}); }
    void layoutChildren() override { ++layouts; }
};
auto openPopup(const std::shared_ptr<Widget>& content) {
    auto popup = std::make_shared<Popup>();
    popup->setAnchorRect(Rect{40, 20, 100, 30});
    popup->setViewport(Rect{0, 0, 640, 480});
    popup->setContent(content);
    popup->setOpen(true);
    return popup;
}
void click(Popup& popup, Point point) {
    popup.onMouseDown(MouseEvent{point});
    popup.onMouseUp(MouseEvent{point});
}
void deepMenuGeometry() {
    auto menu = std::make_shared<MenuBody>();
    auto popup = openPopup(menu);
    internal::InputLayoutSnapshot snapshot;
    snapshot.prepare(popup, {0, 0, 640, 480});
    const auto r = menu->frame();
    click(*popup, {r.x + 10, r.y + 48});
    check(r.width > 0 && menu->second->clicks == 1 && menu->first->clicks == 0,
        "first unpainted deep popup click reaches the intended menu item");

    // Start from valid old geometry, as a previously presented popup would.
    popup->setOpen(true);
    menu->setFrame(popup->resolvedContentRect().inset(popup->resolvedStyle().padding));
    menu->layoutSubtree();
    menu->resizeRows(64);
    snapshot.prepare(popup, {0, 0, 640, 480});
    const auto changed = menu->frame();
    const int before = menu->second->clicks;
    click(*popup, {changed.x + 10, changed.y + 96});
    check(menu->second->clicks == before + 1,
        "popup content mutation updates deep hit geometry before the next paint");

    popup->setOpen(false);
    const int layouts = menu->layouts;
    const Rect closed = menu->frame();
    menu->resizeRows(80);
    snapshot.prepare(popup, {0, 0, 640, 480});
    const Point contentPoint{closed.x + 10, closed.y + 96};
    const int clicks = menu->second->clicks;
    click(*popup, contentPoint);
    check(menu->layouts == layouts && same(menu->frame(), closed) &&
        menu->second->clicks == clicks && !popup->hitTest(contentPoint),
        "closed popup content is neither laid out nor hit");
}
void ownedAnchorGeometryAndReplacement() {
    auto anchor = std::make_shared<MenuBody>();
    auto popup = std::make_shared<Popup>();
    popup->setAnchor(anchor);
    internal::InputLayoutSnapshot snapshot;
    snapshot.prepare(popup, {0, 0, 640, 480});
    click(*popup, {10, 48});
    check(anchor->second->clicks == 1,
        "closed popup still lays out and routes its own deep anchor");
    auto replacement = std::make_shared<MenuBody>();
    anchor->setOnSizeChanged([&](Size) { popup->setAnchor(replacement); });
    snapshot.prepare(popup, {0, 0, 100, 80});
    click(*popup, {10, 48});
    check(popup->anchor() == replacement && replacement->second->clicks == 1,
        "owned anchor replacement during layout recovers before pointer routing");
}
void foreignAnchorIsNotAdopted() {
    struct Page final : View {
        void layoutChildren() override {
            children()[0]->setFrame({300, 40, 120, 35});
            children()[1]->setFrame({0, 0, 0, 0});
        }
    };
    for (const bool attachPageFirst : {false, true}) {
        auto page = std::make_shared<Page>();
        auto anchor = std::make_shared<Anchor>();
        auto popup = openPopup(std::make_shared<MenuBody>());
        if (attachPageFirst) page->add(anchor);
        popup->setAnchor(anchor);
        if (!attachPageFirst) page->add(anchor);
        page->add(popup);
        popup->clearAnchorRect();
        internal::InputLayoutSnapshot snapshot;
        snapshot.prepare(page, {0, 0, 640, 480});
        const Rect expected{300, 40, 120, 35};
        check(same(anchor->frame(), expected) && anchor->layouts == 1 &&
            popup->resolvedContentRect().x >= expected.x,
            "popup references a foreign page anchor without moving or traversing it twice");
        popup->setAnchorRect(Rect{500, 250, 1, 1});
        click(*popup, {510, 290});
        check(same(anchor->frame(), expected),
            "popup pointer routing also preserves foreign anchor geometry");
    }
}
void sizeObserverCanReplaceAndCloseContent() {
    auto oldBody = std::make_shared<MenuBody>(), replacement = std::make_shared<MenuBody>();
    auto popup = openPopup(oldBody);
    int changes = 0;
    oldBody->setOnSizeChanged([&](Size) {
        ++changes;
        popup->setContent(replacement);
        popup->setOpen(false);
    });
    internal::InputLayoutSnapshot snapshot;
    check(snapshot.prepare(popup, {0, 0, 640, 480}) && changes == 1 &&
        popup->content() == replacement && replacement->layouts == 0,
        "popup size observer can replace and close owned content safely");
    popup->setOpen(true);
    snapshot.prepare(popup, {0, 0, 640, 480});
    const auto r = replacement->frame();
    click(*popup, {r.x + 10, r.y + 48});
    check(replacement->second->clicks == 1,
        "replacement popup content recovers on reopening without paint");
}
} // namespace
int main() {
    deepMenuGeometry();
    ownedAnchorGeometryAndReplacement();
    foreignAnchorIsNotAdopted();
    sizeObserverCanReplaceAndCloseContent();
    return failures ? 1 : 0;
}
