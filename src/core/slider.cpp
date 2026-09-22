#include "oneui/controls/slider.h"

#include "oneui/style.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace oneui {
namespace {

void applyFocusRingOverride(FocusRingStyle& style, const FocusRingStyleOverride& override) {
    if (override.color) {
        style.color = *override.color;
    }
    if (override.width) {
        style.width = *override.width;
    }
    if (override.offset) {
        style.offset = *override.offset;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.visible) {
        style.visible = *override.visible;
    }
}

void applySliderStateOverride(SliderStyle& style, const SliderStateStyleOverride& override) {
    if (override.trackBackground) {
        style.trackBackground = *override.trackBackground;
    }
    if (override.trackFill) {
        style.trackFill = *override.trackFill;
    }
    if (override.thumbBackground) {
        style.thumbBackground = *override.thumbBackground;
    }
    if (override.thumbBorder) {
        style.thumbBorder = *override.thumbBorder;
    }
    if (override.trackHeight) {
        style.trackHeight = *override.trackHeight;
    }
    if (override.thumbSize) {
        style.thumbSize = *override.thumbSize;
    }
    if (override.thumbBorderWidth) {
        style.thumbBorderWidth = *override.thumbBorderWidth;
    }
    if (override.focusRing) {
        applyFocusRingOverride(style.focusRing, *override.focusRing);
    }
}

SliderStyle baseSliderStyle(bool disabled, bool hovered, bool pressed) {
    const auto& t = theme();
    SliderStyle style;
    style.trackBackground = disabled ? t.disabledBorder : Color{226, 232, 240};
    style.trackFill = disabled ? Color{203, 213, 225} : t.primary;
    style.thumbBackground = disabled ? t.disabledForeground : (pressed ? t.primaryPressed : (hovered ? t.primaryHover : t.primary));
    style.thumbBorder = style.thumbBackground;
    style.trackHeight = 4.0f;
    style.thumbSize = 16.0f;
    style.thumbBorderWidth = 0.0f;
    style.focusRing = FocusRingStyle{t.focusOutline, t.focusOutlineWidth, 3.0f, 13.0f, true};
    return style;
}

} // namespace

Slider::Slider() {
    setPreferredSize(Size{220.0f, 32.0f});
}

void Slider::setRange(double minimum, double maximum) {
    if (!std::isfinite(minimum) || !std::isfinite(maximum)
        || !std::isfinite(maximum - minimum)) {
        return;
    }
    const double upper = std::max(minimum + 0.0001, maximum);
    if (!std::isfinite(upper) || upper <= minimum) { return; }
    if (minimum_ == minimum && maximum_ == upper) { return; }
    cancelInteraction();
    minimum_ = minimum;
    maximum_ = upper;
    assignValue(value());
    invalidate();
}

void Slider::setStep(double step) {
    if (!std::isfinite(step)) {
        return;
    }
    const double next = std::max(0.0001, step);
    if (step_ == next) { return; }
    step_ = next;
    invalidate();
}

void Slider::setValue(double value) {
    assignValue(value);
}

double Slider::value() const {
    return valueBinding_.get(value_);
}

void Slider::bindValue(State<double>& state) {
    valueBinding_ = Binding<double>(state, [this] {
        invalidate();
    });
    invalidate();
}

void Slider::setStyleOverride(SliderStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void Slider::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void Slider::setOnChanged(std::function<void(double)> callback) {
    onChanged_ = std::move(callback);
}

void Slider::setOnInteraction(std::function<void(SliderInteraction, double)> callback) {
    onInteraction_ = std::move(callback);
}

bool Slider::dragging() const { return pressed_; }

void Slider::notifyInteraction(SliderInteraction phase) {
    // Callback replacement/removal during a callback must not destroy its
    // currently executing std::function target.
    auto callback = onInteraction_;
    if (callback) {
        callback(phase, value());
    }
}

void Slider::cancelInteraction() {
    if (!pressed_) {
        return;
    }
    pressed_ = false;
    assignValue(beginValue_);
    notifyInteraction(SliderInteraction::Cancel);
}

void Slider::paint(Canvas& canvas) {
    const SliderStyle style = resolvedStyle();
    const Rect rect = frame();
    const float thumbSize = std::max(1.0f, style.thumbSize);
    const float thumbRadius = thumbSize / 2.0f;
    const float trackHeight = std::max(1.0f, style.trackHeight);
    const float trackY = rect.y + rect.height / 2.0f - trackHeight / 2.0f;
    const Rect track{rect.x + thumbRadius, trackY, std::max(1.0f, rect.width - thumbSize), trackHeight};
    const float x = track.x + track.width * static_cast<float>(normalizedValue());

    if (focusVisible() && !disabled() && style.focusRing.visible) {
        const float ringSize = thumbSize + style.focusRing.offset * 2.0f;
        canvas.strokeRect(Rect{x - ringSize / 2.0f, rect.y + rect.height / 2.0f - ringSize / 2.0f, ringSize, ringSize}, style.focusRing.color, style.focusRing.radius, style.focusRing.width);
    }

    canvas.fillRect(track, style.trackBackground, trackHeight / 2.0f);
    canvas.fillRect(Rect{track.x, track.y, x - track.x, track.height}, style.trackFill, trackHeight / 2.0f);
    const Rect thumb{x - thumbRadius, rect.y + rect.height / 2.0f - thumbRadius, thumbSize, thumbSize};
    canvas.fillRect(thumb, style.thumbBackground, thumbRadius);
    if (style.thumbBorderWidth > 0.0f) {
        canvas.strokeRect(thumb, style.thumbBorder, thumbRadius, style.thumbBorderWidth);
    }
}

bool Slider::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const bool nextHover = contains(event.position);
    bool changed = nextHover != hovered_;
    hovered_ = nextHover;

    if (pressed_) {
        const double previous = value();
        assignFromPoint(event.position);
        if (pressed_ && previous != value()) {
            notifyInteraction(SliderInteraction::Update);
        }
        changed = true;
    }

    if (changed) {
        invalidate();
    }

    return changed;
}

bool Slider::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position) || event.button != MouseButton::Left || pressed_) {
        return false;
    }

    beginValue_ = value();
    pressed_ = true;
    notifyInteraction(SliderInteraction::Begin);
    if (!pressed_ || !interactive()) {
        return true;
    }
    assignFromPoint(event.position);
    if (pressed_) {
        notifyInteraction(SliderInteraction::Update);
    }
    invalidate();
    return true;
}

bool Slider::onMouseUp(const MouseEvent& event) {
    if (!interactive() || !pressed_ || event.button != MouseButton::Left) {
        return false;
    }
    // The release can occur beyond the widget or without a final move event.
    assignFromPoint(event.position);
    if (!pressed_) {
        return true;
    }
    pressed_ = false;
    notifyInteraction(SliderInteraction::Commit);
    invalidate();
    return true;
}

bool Slider::onKeyDown(const KeyEvent& event) {
    if (!interactive() || !event.pressed) {
        return false;
    }
    if (event.key == Key::Escape && pressed_) {
        cancelInteraction();
        return true;
    }
    if (pressed_) {
        return false;
    }
    double target;
    switch (event.key) {
    case Key::Left: case Key::Down: target = value() - step_; break;
    case Key::Right: case Key::Up: target = value() + step_; break;
    case Key::Home: target = minimum_; break;
    case Key::End: target = maximum_; break;
    default: return false;
    }
    beginValue_ = value();
    pressed_ = true;
    notifyInteraction(SliderInteraction::Begin);
    if (!pressed_ || !interactive()) { return true; }
    assignValue(target);
    if (!pressed_) { return true; }
    notifyInteraction(SliderInteraction::Update);
    if (!pressed_) { return true; }
    pressed_ = false;
    notifyInteraction(SliderInteraction::Commit);
    invalidate();
    return true;
}

bool Slider::isFocusable() const {
    return interactive();
}

AccessibilityInfo Slider::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::Slider;
    }
    info.value = std::to_wstring(value());
    return info;
}

void Slider::assignValue(double value) {
    if (!std::isfinite(value)) { return; }
    const double previous = this->value();
    const double clamped = std::clamp(value, minimum_, maximum_);
    const double stepped = minimum_ + std::round((clamped - minimum_) / step_) * step_;
    const double next = std::clamp(stepped, minimum_, maximum_);
    if (previous == next) { return; }
    valueBinding_.set(next, value_);
    invalidate();
    if (previous != next && onChanged_) {
        auto callback = onChanged_;
        callback(next);
    }
}

void Slider::assignFromPoint(Point point) {
    const Rect rect = frame();
    const SliderStyle style = resolvedStyle();
    const float thumbSize = std::max(1.0f, style.thumbSize);
    const float trackX = rect.x + thumbSize / 2.0f;
    const float trackWidth = std::max(1.0f, rect.width - thumbSize);
    const double normalized = std::clamp((point.x - trackX) / trackWidth, 0.0f, 1.0f);
    assignValue(minimum_ + (maximum_ - minimum_) * normalized);
}

double Slider::normalizedValue() const {
    return std::clamp((value() - minimum_) / (maximum_ - minimum_), 0.0, 1.0);
}

SliderStyle Slider::resolvedStyle() const {
    SliderStyle style = baseSliderStyle(disabled(), hovered_, pressed_);
    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        applySliderStateOverride(style, *styleOverride_->normal);
    }
    if (disabled() && styleOverride_->disabled) {
        applySliderStateOverride(style, *styleOverride_->disabled);
    } else if (pressed_ && styleOverride_->pressed) {
        applySliderStateOverride(style, *styleOverride_->pressed);
    } else if (hovered_ && styleOverride_->hovered) {
        applySliderStateOverride(style, *styleOverride_->hovered);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        applySliderStateOverride(style, *styleOverride_->focusVisible);
    }
    return style;
}

bool Slider::hasInteractionState() const {
    return hovered_ || pressed_;
}

void Slider::resetInteractionState() {
    hovered_ = false;
    cancelInteraction();
}

} // namespace oneui
