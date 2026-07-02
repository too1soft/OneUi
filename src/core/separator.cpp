#include "oneui/controls/separator.h"

#include "oneui/style.h"

#include <utility>

namespace oneui {
namespace {

void applySeparatorStyleOverride(SeparatorStyle& style, const SeparatorStyleOverride& overrideStyle) {
    if (overrideStyle.color) {
        style.color = *overrideStyle.color;
    }
    if (overrideStyle.thickness) {
        style.thickness = *overrideStyle.thickness;
    }
}

} // namespace

Separator::Separator(SeparatorOrientation orientation) : orientation_(orientation) {
    setPreferredSize(orientation_ == SeparatorOrientation::Horizontal ? Size{0.0f, 1.0f} : Size{1.0f, 0.0f});
}

void Separator::setOrientation(SeparatorOrientation orientation) {
    orientation_ = orientation;
    setPreferredSize(orientation_ == SeparatorOrientation::Horizontal ? Size{0.0f, 1.0f} : Size{1.0f, 0.0f});
    invalidate();
}

SeparatorOrientation Separator::orientation() const {
    return orientation_;
}

void Separator::setStyleOverride(SeparatorStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void Separator::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void Separator::paint(Canvas& canvas) {
    const auto style = resolvedStyle();
    const Rect rect = frame();
    if (orientation_ == SeparatorOrientation::Horizontal) {
        const float y = rect.y + rect.height / 2.0f;
        canvas.drawLine(Point{rect.x, y}, Point{rect.x + rect.width, y}, style.color, style.thickness);
    } else {
        const float x = rect.x + rect.width / 2.0f;
        canvas.drawLine(Point{x, rect.y}, Point{x, rect.y + rect.height}, style.color, style.thickness);
    }
}

SeparatorStyle Separator::resolvedStyle() const {
    SeparatorStyle style{};
    if (styleOverride_) {
        applySeparatorStyleOverride(style, *styleOverride_);
    }
    return style;
}

} // namespace oneui
