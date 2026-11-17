#include "oneui/controls/reveal.h"
#include "support/test_ui_context.h"
#include <iostream>
#include <stdexcept>

using namespace oneui;
namespace {
struct Probe final : Widget {
    int downs = 0;
    void paint(Canvas &) override {}
    bool isFocusable() const override { return true; }
    bool onMouseDown(const MouseEvent &) override { ++downs; return true; }
    bool onMouseUp(const MouseEvent &) override { return true; }
};
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void exercise(RevealPreset preset) {
    auto probe = std::make_shared<Probe>(); probe->setPreferredSize({160, 64});
    auto reveal = std::make_shared<Reveal>(probe);
    test_support::TestUiContext ui(reveal);
    reveal->setPreset(preset); reveal->setAnimationScheduler([] {}); reveal->setTransition({180, EasingCurve::EaseOutCubic});
    reveal->setReducedMotion(true); reveal->setOpen(false); reveal->setReducedMotion(false);
    reveal->setOpen(true); reveal->setFrame({0, 0, 160, 64}); reveal->layoutSubtree(); ui.advance(40);
    require(reveal->running(), "The input test must run during the entrance animation");
    const MouseEvent touch{{24, 24}, MouseButton::Left};
    const bool fade = preset == RevealPreset::Fade;
    require(reveal->hitTest(touch.position) == fade, "Only stable fade geometry may accept an early hit");
    require(reveal->onMouseDown(touch) == fade, "Opening fade must deliver the first visible tap");
    reveal->onMouseUp(touch);
    require(probe->downs == (fade ? 1 : 0), "Pointer dispatch did not match the motion input policy");
    ui.advance(200); require(reveal->hitTest(touch.position), "The settled reveal must accept input");
    reveal->setOpen(false);
    require(!reveal->hitTest(touch.position) && !reveal->onMouseDown(touch), "Closing must reject input immediately");
    ui.advance(200); require(!reveal->visible(), "The closed subtree must be hidden");
}
}
int main() {
    try { exercise(RevealPreset::Fade); exercise(RevealPreset::Expand); }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "Reveal input: opening fade, moving expansion, settled and closing states passed.\n";
}
