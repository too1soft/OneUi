#include "oneui/style_transition.h"

namespace oneui {
namespace {

Color colorOr(std::optional<Color> color, Color fallback) {
    return color.value_or(fallback);
}

float opacityOr(const StyleBox& box) {
    return box.opacity.value_or(1.0f);
}

TransitionSpec transitionSpecFrom(const StyleBox& box) {
    TransitionSpec spec;
    if (box.transitionDurationMs) {
        spec.durationMs = *box.transitionDurationMs;
    }
    if (box.transitionEasing) {
        spec.easing = *box.transitionEasing;
    }
    return spec;
}

} // namespace

void StyleBoxTransition::reset(const StyleBox& box) {
    background_.reset(colorOr(box.background.color, Color{0, 0, 0, 0}));
    foreground_.reset(colorOr(box.foreground, Color{0, 0, 0, 0}));
    border_.reset(colorOr(box.borderColor, Color{0, 0, 0, 0}));
    opacity_.reset(opacityOr(box));
    initialized_ = true;
}

void StyleBoxTransition::animateTo(const StyleBox& from, const StyleBox& target, double nowMs) {
    if (!initialized_) {
        reset(from);
    }

    const TransitionSpec spec = transitionSpecFrom(target);
    background_.animateTo(colorOr(target.background.color, Color{0, 0, 0, 0}), nowMs, spec);
    foreground_.animateTo(colorOr(target.foreground, Color{0, 0, 0, 0}), nowMs, spec);
    border_.animateTo(colorOr(target.borderColor, Color{0, 0, 0, 0}), nowMs, spec);
    opacity_.animateTo(opacityOr(target), nowMs, spec);
}

bool StyleBoxTransition::tick(double nowMs) {
    bool active = false;
    active = background_.tick(nowMs) || active;
    active = foreground_.tick(nowMs) || active;
    active = border_.tick(nowMs) || active;
    active = opacity_.tick(nowMs) || active;
    return active;
}

bool StyleBoxTransition::initialized() const {
    return initialized_;
}

bool StyleBoxTransition::running() const {
    return background_.running() || foreground_.running() || border_.running() || opacity_.running();
}

StyleBox StyleBoxTransition::applyTo(StyleBox target) const {
    if (!initialized_) {
        return target;
    }

    target.background.color = background_.value();
    target.background.gradientStart.reset();
    target.background.gradientEnd.reset();
    target.foreground = foreground_.value();
    target.borderColor = border_.value();
    target.opacity = opacity_.value();
    return target;
}

} // namespace oneui
