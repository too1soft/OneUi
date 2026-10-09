#include "oneui/layout/panel.h"

#include <utility>

namespace oneui {

void Panel::setBackground(Color color) {
    background_ = color;
    explicitBackground_ = true;
    invalidate();
}

void Panel::setBorder(Color color) {
    border_ = color;
    explicitBorder_ = true;
    invalidate();
}

void Panel::setBorderWidth(float width) {
    borderWidth_ = width;
    explicitBorderWidth_ = true;
    invalidate();
}

void Panel::setRadius(float radius) {
    radius_ = radius;
    explicitRadius_ = true;
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
    if (styleBox_ && (explicitBackground_ || explicitBorder_ || explicitBorderWidth_ || explicitRadius_)) {
        // A style rule that matches only by type or class must not silently
        // discard colors the caller set directly (for example preview swatches).
        StyleBox box = *styleBox_;
        const auto& background = box.background;
        if (explicitBackground_ && !background.color && !background.gradientStart && !background.gradient) {
            box.background.color = background_;
        }
        if (explicitBorder_ && !box.borderColor) {
            box.borderColor = border_;
        }
        if (explicitBorderWidth_ && !box.borderWidth) {
            box.borderWidth = borderWidth_;
        }
        if (explicitRadius_ && !box.radius) {
            box.radius = radius_;
        }
        paintStyleBox(canvas, rect, box);
    } else if (styleBox_) {
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
