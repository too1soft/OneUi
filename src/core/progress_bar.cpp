#include "oneui/controls/progress_bar.h"

#include "oneui/style.h"

#include <algorithm>
#include <utility>

namespace oneui {
namespace {

void applyProgressBarStyleOverride(ProgressBarStyle& style, const ProgressBarStyleOverride& overrideStyle) {
    if (overrideStyle.trackBackground) {
        style.trackBackground = *overrideStyle.trackBackground;
    }
    if (overrideStyle.fill) {
        style.fill = *overrideStyle.fill;
    }
    if (overrideStyle.disabledFill) {
        style.disabledFill = *overrideStyle.disabledFill;
    }
    if (overrideStyle.radius) {
        style.radius = *overrideStyle.radius;
    }
}

} // namespace

ProgressBar::ProgressBar() {
    setPreferredSize(Size{220.0f, 10.0f});
}

void ProgressBar::setValue(double value) {
    valueBinding_.set(std::clamp(value, 0.0, 1.0), value_);
    invalidate();
}

double ProgressBar::value() const {
    return valueBinding_.get(value_);
}

void ProgressBar::bindValue(State<double>& state) {
    valueBinding_ = Binding<double>(state, [this] {
        invalidate();
    });
    invalidate();
}

void ProgressBar::setStyleOverride(ProgressBarStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void ProgressBar::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void ProgressBar::paint(Canvas& canvas) {
    const auto style = resolvedStyle();
    const Rect rect = frame();
    const float fillWidth = rect.width * static_cast<float>(clampedValue());

    canvas.fillRect(rect, style.trackBackground, style.radius);
    if (fillWidth > 0.0f) {
        canvas.fillRect(Rect{rect.x, rect.y, fillWidth, rect.height}, disabled() ? style.disabledFill : style.fill, style.radius);
    }
}

double ProgressBar::clampedValue() const {
    return std::clamp(value(), 0.0, 1.0);
}

ProgressBarStyle ProgressBar::resolvedStyle() const {
    ProgressBarStyle style{};
    if (styleOverride_) {
        applyProgressBarStyleOverride(style, *styleOverride_);
    }
    return style;
}

} // namespace oneui
