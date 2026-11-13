#include "internal/input_layout.h"
#include "oneui/controls/form_field.h"
#include "oneui/controls/tabs.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/layout/stack.h"
#include "support/recording_canvas.h"
#include <iostream>

using namespace oneui;
namespace {
int failures = 0;
void check(bool value, const char* name) {
    std::cout << (value ? "PASS " : "FAIL ") << name << '\n';
    if (!value) ++failures;
}
bool same(Rect a, Rect b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
bool contains(Rect a, Rect b) {
    return a.x <= b.x && a.y <= b.y && a.x + a.width >= b.x + b.width &&
        a.y + a.height >= b.y + b.height;
}
struct Probe final : Widget {
    int paints = 0;
    void paint(Canvas&) override { ++paints; }
    Rect paintBounds() const override {
        const auto r = frame();
        return {r.x - 12, r.y - 12, r.width + 24, r.height + 24};
    }
};
struct Fill final : View {
    int layouts = 0;
    void layoutChildren() override {
        ++layouts;
        for (const auto& child : children()) child->setFrame(frame());
    }
};

void ownedSubtrees() {
    constexpr Rect viewport{0, 0, 640, 360};
    auto host = std::make_shared<OverlayHost>();
    auto body = std::make_shared<Fill>();
    auto leaf = std::make_shared<Probe>();
    body->add(leaf);
    host->setContent(body);
    internal::InputLayoutSnapshot snapshot;
    check(snapshot.prepare(host, viewport) && same(body->frame(), viewport) &&
        same(leaf->frame(), viewport), "OverlayHost owned content resolves without paint");
    const int layouts = body->layouts;
    snapshot.prepare(host, viewport);
    check(body->layouts == layouts, "unchanged owned subtree reuses the geometry snapshot");
    {
        internal::InputLayoutSnapshot::PaintScope painting(snapshot);
        check(!snapshot.prepare(host, viewport), "input geometry does not reenter an in-progress paint");
    }
    check(snapshot.prepare(host, viewport), "geometry preparation resumes after paint");

    auto layer = std::make_shared<Fill>();
    auto layerLeaf = std::make_shared<Probe>();
    layer->add(layerLeaf);
    host->addAnchoredOverlay(layer, OverlayOptions::modeless(), {200, 100}, {}, 2, 2);
    snapshot.prepare(host, viewport);
    check(same(layer->frame(), {440, 260, 200, 100}) && same(layerLeaf->frame(), layer->frame()),
        "anchored overlay resolves its descendants before pointer routing");
    check(leaf->paints == 0 && layerLeaf->paints == 0, "owned geometry preparation never paints");

    auto form = std::make_shared<FormField>();
    auto formBody = std::make_shared<Fill>();
    auto formLeaf = std::make_shared<Probe>();
    formBody->add(formLeaf);
    formBody->setPreferredSize({200, 60});
    form->setChild(formBody);
    snapshot.prepare(form, viewport);
    check(formBody->frame().height == 60 && same(formLeaf->frame(), formBody->frame()),
        "FormField owned child resolves without paint");

    auto tabs = std::make_shared<Tabs>();
    tabs->setFrame({0, 0, 300, 40});
    tabs->setItems({L"One", L"Two"});
    const bool editing = tabs->beginEdit(0);
    const Rect before = tabs->editorFrame();
    snapshot.prepare(tabs, {0, 0, 600, 70});
    check(editing && tabs->editorFrame().height > before.height,
        "Tabs owned editor follows current strip geometry without paint");

    auto root = std::make_shared<Fill>();
    root->add(host);
    snapshot.prepare(root, {0, 0, 800, 600});
    check(same(body->frame(), root->frame()) && same(leaf->frame(), root->frame()),
        "ordinary View recursively enters nested composite owners");
}

void queuedRouteMutation() {
    struct Button final : Widget {
        int clicks = 0;
        std::function<void()> activate;
        void paint(Canvas&) override {}
        bool onMouseDown(const MouseEvent&) override { return true; }
        bool onMouseUp(const MouseEvent&) override {
            ++clicks;
            if (activate) activate();
            return true;
        }
    };
    struct Route final : View {
        std::shared_ptr<Button> nav = std::make_shared<Button>(), action;
        Route() { replace(); }
        void replace() {
            action = std::make_shared<Button>();
            action->setFrame({0, 0, 640, 360});
            reconcileChildren({nav, action});
        }
        void layoutChildren() override {
            nav->setFrame({0, 0, 100, 360});
            action->setFrame({100, 0, 540, 360});
        }
    };
    auto host = std::make_shared<OverlayHost>();
    auto route = std::make_shared<Route>();
    host->setContent(route);
    host->setFrame({0, 0, 640, 360});
    route->setFrame(host->frame());
    route->layoutSubtree(); // Geometry of the previously presented page.
    route->nav->activate = [&] {
        route->replace();
        route->action->activate = [&] {
            auto modal = std::make_shared<View>();
            modal->setFrame(host->frame());
            host->addOverlay(modal, OverlayOptions::modal());
        };
    };
    internal::InputLayoutSnapshot snapshot;
    const MouseEvent click{{40, 60}};
    // Same MOVE/DOWN/UP ordering as PostMessage navigation. No window, OS
    // cursor, timing, or desktop input is involved in this regression.
    for (int target = 0; target < 3; ++target) {
        snapshot.prepare(host, host->frame()); host->onMouseMove(click);
        snapshot.prepare(host, host->frame()); host->onMouseDown(click);
        snapshot.prepare(host, host->frame()); host->onMouseUp(click);
    }
    check(route->nav->clicks == 3 && host->overlays().empty(),
        "queued route mutations cannot activate stale content or block recovery with a modal");
}

void anchoredLayoutSurvivesOwnerMutation() {
    for (const bool clearAll : {false, true}) {
        auto host = std::make_shared<OverlayHost>();
        auto first = std::make_shared<Fill>(), second = std::make_shared<Fill>();
        host->addAnchoredOverlay(first, OverlayOptions::modeless(), {100, 50}, {}, 0, 0);
        host->addAnchoredOverlay(second, OverlayOptions::modeless(), {100, 50}, {}, 2, 2);
        int changes = 0;
        first->setOnSizeChanged([&](Size) {
            ++changes;
            if (clearAll) host->clearOverlays();
            else host->removeOverlay(second.get());
        });
        internal::InputLayoutSnapshot snapshot;
        check(snapshot.prepare(host, {0, 0, 640, 360}) && changes == 1 &&
            second->frame().width == 0 && host->overlays().size() == (clearAll ? 0u : 1u),
            clearAll ? "anchored size observer can clear overlay ownership safely"
                     : "anchored size observer can remove a later overlay safely");
    }
}

void pendingDamageSurvivesPreInputLayout(StackEngine engine) {
    struct Root final : View {
        void layoutChildren() override {
            children().front()->setFrame({20, 30, 300, 100});
        }
    };
    auto root = std::make_shared<Root>();
    auto inner = std::make_shared<Stack>();
    inner->setEngine(engine);
    auto resized = std::make_shared<Probe>(), sibling = std::make_shared<Probe>();
    resized->setPreferredSize({200, 20});
    sibling->setPreferredSize({200, 30});
    inner->add(resized);
    inner->add(sibling);
    inner->setFlex(resized, {0, 0, std::nullopt});
    inner->setFlex(sibling, {0, 0, std::nullopt});
    root->add(inner);
    internal::InputLayoutSnapshot snapshot;
    snapshot.prepare(root, {0, 0, 640, 480});
    test_support::RecordingCanvas canvas;
    root->paint(canvas);
    const Rect before = sibling->paintBounds();
    Rect damage{};
    root->setRectInvalidator([&](Rect rect) { damage = rect; });
    resized->setPreferredSize({200, 180});
    const Rect pending = damage;
    const bool prepared = snapshot.prepare(root, root->frame());
    const Rect after = sibling->paintBounds();
    canvas.clipOverride = pending;
    root->paint(canvas);
    std::cout << "pending damage engine=" << static_cast<int>(engine) << " prepared=" << prepared
              << " beforeY=" << before.y << " afterY=" << after.y
              << " innerBottom=" << inner->frame().y + inner->frame().height
              << " pending=" << pending.x << ',' << pending.y << ',' << pending.width << ',' << pending.height
              << " siblingPaints=" << sibling->paints << '\n';
    check(prepared && after.y > inner->frame().y + inner->frame().height &&
        contains(pending, before) && contains(pending, after) && sibling->paints == 2,
        engine == StackEngine::Legacy ? "Legacy pending damage covers pre-input sibling relocation and overflow"
                                      : "Yoga pending damage covers pre-input sibling relocation and overflow");
}
} // namespace

int main() {
    ownedSubtrees();
    queuedRouteMutation();
    anchoredLayoutSurvivesOwnerMutation();
    pendingDamageSurvivesPreInputLayout(StackEngine::Legacy);
#ifdef ONEUI_HAS_YOGA
    pendingDamageSurvivesPreInputLayout(StackEngine::Yoga);
#endif
    return failures ? 1 : 0;
}
