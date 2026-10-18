#include "oneui/controls/icon_view.h"

#include <algorithm>

namespace oneui {

IconView::IconView(IconSymbol symbol)
    : symbol_(symbol) {
    setPreferredSize(Size{18.0f, 18.0f});
    setAccessibleRole(AccessibilityRole::Custom);
}

void IconView::setSymbol(IconSymbol symbol) {
    if (symbol_ == symbol) {
        return;
    }
    symbol_ = symbol;
    invalidate();
}

void IconView::setColor(Color color) {
    color_ = color;
    invalidate();
}

void IconView::setAccent(Color color) {
    accent_ = color;
    invalidate();
}

void IconView::setStrokeWidth(float width) {
    strokeWidth_ = std::max(1.0f, width);
    invalidate();
}

void IconView::setGlyphSize(std::optional<float> size) {
    if (size && !(*size > 0.0f)) {
        size.reset();
    }
    if (glyphSize_ == size) {
        return;
    }
    glyphSize_ = size;
    invalidate();
}

void IconView::paint(Canvas& canvas) {
    auto rect = frame();
    if (glyphSize_) {
        const float side = std::min({*glyphSize_, rect.width, rect.height});
        rect = Rect{rect.x + (rect.width - side) * 0.5f, rect.y + (rect.height - side) * 0.5f, side, side};
    }
    const auto color = disabled() ? Color{128, 132, 146, color_.a} : color_;
    const auto accent = disabled() && accent_.a != 0 ? Color{96, 99, 112, accent_.a} : accent_;
    paintIcon(canvas, symbol_, rect, color, accent, strokeWidth_);
}

} // namespace oneui
