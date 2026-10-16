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
int main() { try { verify(); std::cout << "composition owners passed\n"; return 0; }
catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
