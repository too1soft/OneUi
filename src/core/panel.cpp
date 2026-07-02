#include "oneui/layout/panel.h"

#include <utility>

namespace oneui {

void Panel::setBackground(Color color) {
    background_ = color;
    invalidate();
}

void Panel::setBorder(Color color) {
    border_ = color;
    invalidate();
}

void Panel::setBorderWidth(float width) {
    borderWidth_ = width;
    invalidate();
}

void Panel::setRadius(float radius) {
    radius_ = radius;
    invalidate();
}

void Panel::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void Panel::setShadow(BoxShadow shadow) {
    shadow_ = shadow;
    invalidate();
}

void Panel::setStyleBox(StyleBox style) {
    styleBox_ = std::move(style);
    invalidate();
}

void Panel::clearStyleBox() {
    styleBox_.reset();
    invalidate();
}

void Panel::setContent(std::shared_ptr<Widget> child) {
    clearChildren();
    if (child) {
        add(std::move(child));
    }
}

void Panel::paint(Canvas& canvas) {
    const Rect rect = frame();
    if (styleBox_) {
        paintStyleBox(canvas, rect, *styleBox_);
    } else {
        canvas.drawBoxShadow(rect, shadow_, radius_);
        canvas.fillRect(rect, background_, radius_);
        if (borderWidth_ > 0.0f && border_.a > 0) {
            canvas.strokeRect(rect, border_, radius_, borderWidth_);
        }
    }
    View::paint(canvas);
}

void Panel::layoutChildren() {
    const Rect content = frame().inset(padding_);
    for (const auto& child : children()) {
        if (!child->visible()) {
            continue;
        }
        child->setFrame(content);
    }
}

} // namespace oneui
