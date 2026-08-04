#include "oneui/layout/panel.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

struct FillRectCall {
    oneui::Rect rect;
    oneui::Color color;
    float radius = 0.0f;
};

struct StrokeRectCall {
    oneui::Rect rect;
    oneui::Color color;
    float radius = 0.0f;
    float width = 0.0f;
};

struct BoxShadowCall {
    oneui::Rect rect;
    oneui::BoxShadow shadow;
    float radius = 0.0f;
};

class RecordingCanvas final : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect rect, oneui::Color color, float radius = 0.0f) override {
        fillRects.push_back(FillRectCall{rect, color, radius});
    }
    void strokeRect(oneui::Rect rect, oneui::Color color, float radius, float width = 1.0f) override {
        strokeRects.push_back(StrokeRectCall{rect, color, radius, width});
    }
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring&, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {}
    void drawBoxShadow(oneui::Rect rect, const oneui::BoxShadow& shadow, float radius = 0.0f) override {
        shadows.push_back(BoxShadowCall{rect, shadow, radius});
    }

    std::vector<FillRectCall> fillRects;
    std::vector<StrokeRectCall> strokeRects;
    std::vector<BoxShadowCall> shadows;
};

class LayoutProbe final : public oneui::Widget {
public:
    void setInvalidator(std::function<void()> invalidator) override {
        ++invalidatorInstallCount;
        Widget::setInvalidator(std::move(invalidator));
    }

    void setRectInvalidator(std::function<void(oneui::Rect)> invalidator) override {
        ++rectInvalidatorInstallCount;
        Widget::setRectInvalidator(std::move(invalidator));
    }

    void setAnimationScheduler(std::function<void()> scheduler) override {
        ++animationSchedulerInstallCount;
        Widget::setAnimationScheduler(std::move(scheduler));
    }

    void paint(oneui::Canvas&) override {
        painted = true;
    }

    bool painted = false;
    int invalidatorInstallCount = 0;
    int rectInvalidatorInstallCount = 0;
    int animationSchedulerInstallCount = 0;

    void requestInvalidation() {
        invalidate();
    }

    void requestRectInvalidation() {
        invalidateRect(oneui::Rect{1.0f, 2.0f, 3.0f, 4.0f});
    }

    void requestAnimation() {
        requestAnimationFrame();
    }

    void resetInstallCounts() {
        invalidatorInstallCount = 0;
        rectInvalidatorInstallCount = 0;
        animationSchedulerInstallCount = 0;
    }
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

void testPanelPaintsChromeAndLaysOutContent() {
    oneui::Panel panel;
    panel.setFrame(oneui::Rect{10.0f, 20.0f, 300.0f, 200.0f});
    panel.setBackground(oneui::Color{17, 24, 39, 255});
    panel.setBorder(oneui::Color{55, 65, 81, 255});
    panel.setBorderWidth(2.0f);
    panel.setRadius(12.0f);
    panel.setPadding(oneui::Insets{8.0f, 12.0f, 16.0f, 20.0f});
    panel.setShadow(oneui::BoxShadow{oneui::Color{0, 0, 0, 80}, oneui::Point{0.0f, 4.0f}, 18.0f, 0.0f});

    auto child = std::make_shared<LayoutProbe>();
    panel.setContent(child);

    RecordingCanvas canvas;
    panel.paint(canvas);

    expectEqual("Panel shadow count", static_cast<int>(canvas.shadows.size()), 1);
    expectRect("Panel shadow rect", canvas.shadows.front().rect, oneui::Rect{10.0f, 20.0f, 300.0f, 200.0f});
    expectNear("Panel shadow radius", canvas.shadows.front().radius, 12.0f);
    expectEqual("Panel fill count", static_cast<int>(canvas.fillRects.size()), 1);
    expectRect("Panel fill rect", canvas.fillRects.front().rect, oneui::Rect{10.0f, 20.0f, 300.0f, 200.0f});
    expectNear("Panel fill radius", canvas.fillRects.front().radius, 12.0f);
    expectEqual("Panel stroke count", static_cast<int>(canvas.strokeRects.size()), 1);
    expectNear("Panel stroke width", canvas.strokeRects.front().width, 2.0f);
    expectRect("Panel content frame", child->frame(), oneui::Rect{30.0f, 28.0f, 268.0f, 176.0f});
    expectEqual("Panel child painted", child->painted ? 1 : 0, 1);
}

void testPanelCanReplaceContent() {
    oneui::Panel panel;
    panel.setFrame(oneui::Rect{0.0f, 0.0f, 100.0f, 80.0f});

    auto first = std::make_shared<LayoutProbe>();
    auto second = std::make_shared<LayoutProbe>();
    panel.setContent(first);
    panel.setContent(second);

    RecordingCanvas canvas;
    panel.paint(canvas);

    expectEqual("Panel first child not painted after replace", first->painted ? 1 : 0, 0);
    expectEqual("Panel second child painted after replace", second->painted ? 1 : 0, 1);
    expectRect("Panel second child fills frame", second->frame(), oneui::Rect{0.0f, 0.0f, 100.0f, 80.0f});
}

void testReparentedContentKeepsNewOwnerCallbacks() {
    auto child = std::make_shared<LayoutProbe>();
    int oldInvalidations = 0;
    int newInvalidations = 0;
    int oldRectInvalidations = 0;
    int newRectInvalidations = 0;
    int oldAnimationRequests = 0;
    int newAnimationRequests = 0;
    auto oldParent = std::make_unique<oneui::Panel>();
    auto newParent = std::make_unique<oneui::Panel>();
    oldParent->setInvalidator([&] { ++oldInvalidations; });
    newParent->setInvalidator([&] { ++newInvalidations; });
    oldParent->setRectInvalidator([&](oneui::Rect) { ++oldRectInvalidations; });
    newParent->setRectInvalidator([&](oneui::Rect) { ++newRectInvalidations; });
    oldParent->setAnimationScheduler([&] { ++oldAnimationRequests; });
    newParent->setAnimationScheduler([&] { ++newAnimationRequests; });

    oldParent->setContent(child);
    newParent->setContent(child);
    oldParent.reset();
    child->requestRectInvalidation();
    child->requestAnimation();
    newParent->setRectInvalidator({});
    child->requestInvalidation();

    expectEqual("Reparented child ignores destroyed owner", oldInvalidations, 0);
    expectEqual("Reparented child invalidates current owner", newInvalidations, 1);
    expectEqual("Reparented child ignores destroyed rect owner", oldRectInvalidations, 0);
    expectEqual("Reparented child invalidates current rect owner", newRectInvalidations, 1);
    expectEqual("Reparented child ignores destroyed animation owner", oldAnimationRequests, 0);
    expectEqual("Reparented child schedules with current owner", newAnimationRequests, 1);
}

void testNestedCallbackPropagationStaysLinear() {
    auto child = std::make_shared<LayoutProbe>();
    auto middle = std::make_shared<oneui::Panel>();
    oneui::Panel root;
    middle->setContent(child);
    child->resetInstallCounts();

    root.setContent(middle);

    expectEqual("Nested invalidator installed once", child->invalidatorInstallCount, 1);
    expectEqual("Nested rect invalidator installed once", child->rectInvalidatorInstallCount, 1);
    expectEqual("Nested animation scheduler installed once", child->animationSchedulerInstallCount, 1);
}

void testDestroyedParentDetachesRetainedContent() {
    auto child = std::make_shared<LayoutProbe>();
    int invalidations = 0;
    {
        oneui::Panel parent;
        parent.setInvalidator([&] { ++invalidations; });
        parent.setContent(child);
    }

    child->requestInvalidation();
    expectEqual("Retained child ignores destroyed parent", invalidations, 0);
}

} // namespace

int main() {
    testPanelPaintsChromeAndLaysOutContent();
    testPanelCanReplaceContent();
    testReparentedContentKeepsNewOwnerCallbacks();
    testNestedCallbackPropagationStaysLinear();
    testDestroyedParentDetachesRetainedContent();

    if (failures != 0) {
        std::cerr << failures << " panel behavior test(s) failed.\n";
        return 1;
    }

    return 0;
}
