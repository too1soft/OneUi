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
    minimum_ = minimum;
    maximum_ = std::max(minimum + 0.0001, maximum);
    assignValue(value());
    invalidate();
}

void Slider::setStep(double step) {
    step_ = std::max(0.0001, step);
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
        assignFromPoint(event.position);
        changed = true;
    }

    if (changed) {
        invalidate();
    }

    return changed;
}

bool Slider::onMouseDown(const MouseEvent& event) {
    if (!interactive() || !contains(event.position)) {
        return false;
    }

    pressed_ = true;
    assignFromPoint(event.position);
    invalidate();
    return true;
}

bool Slider::onMouseUp(const MouseEvent&) {
    if (!interactive()) {
        return false;
    }
    const bool wasPressed = pressed_;
    pressed_ = false;
    invalidate();
    return wasPressed;
}

bool Slider::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (event.key == Key::Left || event.key == Key::Down) {
        assignValue(value() - step_);
        return true;
    }

    if (event.key == Key::Right || event.key == Key::Up) {
        assignValue(value() + step_);
        return true;
    }

    return false;
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
    const double previous = this->value();
    const double clamped = std::clamp(value, minimum_, maximum_);
    const double stepped = minimum_ + std::round((clamped - minimum_) / step_) * step_;
    const double next = std::clamp(stepped, minimum_, maximum_);
    valueBinding_.set(next, value_);
    invalidate();
    if (previous != next && onChanged_) {
        onChanged_(next);
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
    pressed_ = false;
}

} // namespace oneui
