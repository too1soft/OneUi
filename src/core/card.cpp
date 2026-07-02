#include "oneui/controls/card.h"

#include <utility>

namespace oneui {

void Card::setBackground(Color color) {
    background_ = color;
    invalidate();
}

void Card::setBorder(Color color) {
    border_ = color;
    invalidate();
}

void Card::setRadius(float radius) {
    radius_ = radius;
    invalidate();
}

void Card::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void Card::setShadow(BoxShadow shadow) {
    shadow_ = shadow;
    invalidate();
}

void Card::setStyleBox(StyleBox style) {
    styleBox_ = std::move(style);
    invalidate();
}

void Card::clearStyleBox() {
    styleBox_.reset();
    invalidate();
}

void Card::setContent(std::shared_ptr<Widget> child) {
    clearChildren();
    if (child) {
        add(std::move(child));
    }
}

void Card::paint(Canvas& canvas) {
    const Rect rect = frame();
    if (styleBox_) {
        paintStyleBox(canvas, rect, *styleBox_);
    } else {
        canvas.drawBoxShadow(rect, shadow_, radius_);
        canvas.fillRect(rect, background_, radius_);
        canvas.strokeRect(rect, border_, radius_, 1.0f);
    }
    View::paint(canvas);
}

void Card::layoutChildren() {
    const Rect content = frame().inset(padding_);
    for (const auto& child : children()) {
        if (!child->visible()) {
            continue;
        }
        child->setFrame(content);
    }
}

} // namespace oneui
