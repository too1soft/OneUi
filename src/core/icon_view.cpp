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

void IconView::paint(Canvas& canvas) {
    const auto rect = frame();
    const auto color = disabled() ? Color{128, 132, 146, color_.a} : color_;
    const auto accent = disabled() && accent_.a != 0 ? Color{96, 99, 112, accent_.a} : accent_;
    paintIcon(canvas, symbol_, rect, color, accent, strokeWidth_);
}

} // namespace oneui
