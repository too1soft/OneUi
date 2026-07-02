#include "oneui/layout/stack.h"

#include <algorithm>
#include <utility>

namespace oneui {

Stack::Stack(StackDirection direction) : direction_(direction) {}

void Stack::setDirection(StackDirection direction) {
    direction_ = direction;
    invalidate();
}

void Stack::setGap(float gap) {
    gap_ = gap;
    invalidate();
}

void Stack::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void Stack::setAlign(StackAlign align) {
    align_ = align;
    invalidate();
}

void Stack::setStyleBox(StyleBox style) {
    styleBox_ = std::move(style);
    invalidate();
}

void Stack::clearStyleBox() {
    styleBox_.reset();
    invalidate();
}

void Stack::paint(Canvas& canvas) {
    if (styleBox_) {
        paintStyleBox(canvas, frame(), *styleBox_);
    }
    View::paint(canvas);
}

void Stack::layoutChildren() {
    const Rect content = frame().inset(padding_);
    float cursor = direction_ == StackDirection::Column ? content.y : content.x;
    int visibleCount = 0;
    int flexCount = 0;
    float fixedMainSize = 0.0f;

    for (const auto& child : children()) {
        if (!child->visible()) {
            continue;
        }

        ++visibleCount;
        const Size preferred = child->preferredSize();
        const float preferredMainSize = direction_ == StackDirection::Column ? preferred.height : preferred.width;
        if (preferredMainSize <= 0.0f) {
            ++flexCount;
        } else {
            fixedMainSize += preferredMainSize;
        }
    }

    const float totalGap = std::max(0, visibleCount - 1) * gap_;
    const float availableMainSize = direction_ == StackDirection::Column ? content.height : content.width;
    const float flexMainSize = flexCount > 0
        ? std::max(0.0f, (availableMainSize - fixedMainSize - totalGap) / static_cast<float>(flexCount))
        : 0.0f;

    for (const auto& child : children()) {
        if (!child->visible()) {
            continue;
        }

        const Size preferred = child->preferredSize();

        if (direction_ == StackDirection::Column) {
            const float height = preferred.height <= 0.0f ? flexMainSize : std::max(0.0f, preferred.height);
            const float width = align_ == StackAlign::Stretch || preferred.width <= 0.0f
                ? content.width
                : std::min(preferred.width, content.width);

            float x = content.x;
            if (align_ == StackAlign::Center) {
                x = content.x + (content.width - width) / 2.0f;
            } else if (align_ == StackAlign::End) {
                x = content.x + content.width - width;
            }

            child->setFrame(Rect{x, cursor, width, height});
            cursor += height + gap_;
        } else {
            const float width = preferred.width <= 0.0f ? flexMainSize : std::max(0.0f, preferred.width);
            const float height = align_ == StackAlign::Stretch || preferred.height <= 0.0f
                ? content.height
                : std::min(preferred.height, content.height);

            float y = content.y;
            if (align_ == StackAlign::Center) {
                y = content.y + (content.height - height) / 2.0f;
            } else if (align_ == StackAlign::End) {
                y = content.y + content.height - height;
            }

            child->setFrame(Rect{cursor, y, width, height});
            cursor += width + gap_;
        }
    }
}

} // namespace oneui
