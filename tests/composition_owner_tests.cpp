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

class TextProbe final : public oneui::Widget {
public:
    void paint(oneui::Canvas&) override {}
    void setTextEnvironment(std::wstring family, float scale) override {
        Widget::setTextEnvironment(std::move(family), scale);
        if (onSet) onSet();
    }
    std::function<void()> onSet;
};

void verifyTextEnvironment() {
    using namespace oneui;
    auto parent = std::make_shared<View>();
    auto child = std::make_shared<View>();
    auto leaf = std::make_shared<TextProbe>();
    parent->setTextEnvironment(L"Inherited", 1.25f);
    parent->add(child);
    child->add(leaf);
    leaf->setTextEnvironment(L"Different", 2.0f);
    parent->setTextEnvironment(L"Inherited", 1.25f);
    require(leaf->textFontFamily() == L"Inherited" && leaf->textDpiScale() == 1.25f,
            "reapplying the window environment reconciles a descendant override");

    child->setFontFamily(L"Local");
    View next;
    next.setTextEnvironment(L"Next Window", 1.5f);
    next.add(child);
    parent->clearChildren();
    require(leaf->textFontFamily() == L"Local" && leaf->textDpiScale() == 1.5f,
            "reparenting updates DPI and preserves the nested family override");
    child->setFontFamily(L"");
    require(leaf->textFontFamily() == L"Next Window", "clearing an override restores the new window font");
}

void verifyReentrantTextEnvironment(int count) {
    using namespace oneui;
    View root;
    auto first = std::make_shared<TextProbe>();
    auto replacement = std::make_shared<TextProbe>();
    root.add(first);
    for (int index = 1; index < count; ++index) root.add(std::make_shared<TextProbe>());
    const auto original = root.children();
    first->onSet = [&] {
        root.clearChildren();
        root.add(replacement);
    };
    root.setTextEnvironment(L"New Window Font", 1.75f);
    require(root.children().size() == 1 && root.children().front() == replacement,
            "setter callback can replace children during propagation");
    for (const auto& item : original)
        require(item->textFontFamily() == L"New Window Font" && item->textDpiScale() == 1.75f,
                "original snapshot remains valid across child changes");
    require(replacement->textFontFamily() == L"New Window Font" && replacement->textDpiScale() == 1.75f,
            "replacement inherits the current window environment");
}

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
int main() { try { verify(); verifyDetachedAssembly(); verifyTextEnvironment();
    verifyReentrantTextEnvironment(4); verifyReentrantTextEnvironment(5);
    std::cout << "composition owners passed\n"; return 0; }
catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; } }
