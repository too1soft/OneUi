#include "oneui/layout/panel.h"
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
class Probe final : public oneui::View {
public:
    void askFrame() { requestAnimationFrame(); }
    void askPaint() { invalidate(); }
};

class AnimationProbe final : public oneui::Widget {
public:
    void paint(oneui::Canvas&) override {}
    void setAnimationScheduler(std::function<void()> scheduler) override {
        Widget::setAnimationScheduler(std::move(scheduler));
        ++installations;
        requestAnimationFrame();
    }
    int installations = 0;
};

void verifyDetachedAssembly() {
    using namespace oneui;
    auto leaf = std::make_shared<AnimationProbe>();
    auto inner = std::make_shared<View>();
    inner->add(leaf);
    auto outer = std::make_shared<View>();
    outer->add(inner);
    View root;
    root.add(outer);
    require(leaf->installations == 1, "detached ancestor assembly does not reinstall the leaf scheduler");

    int frames = 0;
    root.setAnimationScheduler([&] { ++frames; });
    require(leaf->installations == 2 && frames == 1, "live root activates the leaf scheduler once");
    int replacementFrames = 0;
    root.setAnimationScheduler([&] { ++replacementFrames; });
    require(leaf->installations == 3 && frames == 1 && replacementFrames == 1,
            "replacing the live scheduler reactivates the leaf on the new callback");
}

void verify() {
    using namespace oneui;
    int oldFrames = 0, newFrames = 0, oldPaints = 0, newPaints = 0;
    auto oldRoot = std::make_shared<Panel>();
    auto newRoot = std::make_shared<Panel>();
    oldRoot->setAnimationScheduler([&] { ++oldFrames; });
    oldRoot->setInvalidator([&] { ++oldPaints; });
    newRoot->setAnimationScheduler([&] { ++newFrames; });
    newRoot->setInvalidator([&] { ++newPaints; });

    // A retained viewport moves into a new page while the old page still
    // lists it, as a declarative slot does until its runtime is closed.
    auto oldSlot = std::make_shared<Panel>();
    auto newSlot = std::make_shared<Panel>();
    auto viewport = std::make_shared<Probe>();
    oldRoot->setContent(oldSlot);
    newRoot->setContent(newSlot);
    oldSlot->setContent(viewport);
    newSlot->setContent(viewport);

    // Closing the old page isolates its root; propagation must not reclaim
    // the viewport from the page that now owns it.
    oldRoot->setInvalidator({});
    oldRoot->setRectInvalidator({});
    oldRoot->setAnimationScheduler({});
    oldPaints = newPaints = 0;
    viewport->askFrame();
    viewport->askPaint();
    require(newFrames == 1 && oldFrames == 0, "reparented child keeps its new animation scheduler");
    require(newPaints == 1 && oldPaints == 0, "reparented child keeps its new invalidator");

    // Owned children still follow their parent's replacement callbacks.
    int replacement = 0;
    newRoot->setAnimationScheduler([&] { ++replacement; });
    viewport->askFrame();
    require(replacement == 1, "owned child follows parent callback refresh");

    oldSlot.reset();
    oldRoot.reset();
    viewport->askFrame();
    require(replacement == 2, "old parent destruction leaves the new owner attached");
}
}
int main() { try { verify(); verifyDetachedAssembly(); std::cout << "composition owners passed\n"; return 0; }
catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
