#include <oneui/controls/label.h>
#include <oneui/controls/reveal.h>

std::shared_ptr<oneui::Reveal> example_reveal(bool reducedMotion) {
    auto reveal = std::make_shared<oneui::Reveal>(std::make_shared<oneui::Label>(L"有限动效"));
    reveal->setPreset(oneui::RevealPreset::Expand);
    reveal->setTransition({220, oneui::EasingCurve::EaseOutCubic});
    reveal->setReducedMotion(reducedMotion);
    reveal->setOpen(false);
    // setOpen(true) starts the transition; it stops scheduling when complete.
    return reveal;
}
