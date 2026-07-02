#include "oneui/layout/split_view.h"

#include <algorithm>
#include <utility>

namespace oneui {

SplitView::SplitView(SplitOrientation orientation) : orientation_(orientation) {}

void SplitView::setFirst(std::shared_ptr<Widget> child) {
    first_ = std::move(child);
    rebuildChildren();
}

void SplitView::setSecond(std::shared_ptr<Widget> child) {
    second_ = std::move(child);
    rebuildChildren();
}

void SplitView::setOrientation(SplitOrientation orientation) {
    orientation_ = orientation;
    invalidate();
}

void SplitView::setSplitRatio(float ratio) {
    splitRatio_ = std::clamp(ratio, 0.0f, 1.0f);
    invalidate();
}

void SplitView::setGap(float gap) {
    gap_ = gap;
    invalidate();
}

void SplitView::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void SplitView::layoutChildren() {
    Rect content = frame().inset(padding_);
    content.width = std::max(0.0f, content.width);
    content.height = std::max(0.0f, content.height);

    const bool hasFirst = first_ && first_->visible();
    const bool hasSecond = second_ && second_->visible();
    const float effectiveGap = hasFirst && hasSecond ? gap_ : 0.0f;

    if (!hasFirst && !hasSecond) {
        return;
    }

    if (hasFirst && !hasSecond) {
        first_->setFrame(content);
        return;
    }

    if (!hasFirst && hasSecond) {
        second_->setFrame(content);
        return;
    }

    if (orientation_ == SplitOrientation::Horizontal) {
        const float available = std::max(0.0f, content.width - effectiveGap);
        const float firstWidth = available * splitRatio_;
        first_->setFrame(Rect{content.x, content.y, firstWidth, content.height});
        second_->setFrame(Rect{content.x + firstWidth + effectiveGap, content.y, available - firstWidth, content.height});
    } else {
        const float available = std::max(0.0f, content.height - effectiveGap);
        const float firstHeight = available * splitRatio_;
        first_->setFrame(Rect{content.x, content.y, content.width, firstHeight});
        second_->setFrame(Rect{content.x, content.y + firstHeight + effectiveGap, content.width, available - firstHeight});
    }
}

void SplitView::rebuildChildren() {
    clearChildren();
    if (first_) {
        add(first_);
    }
    if (second_) {
        add(second_);
    }
    invalidate();
}

} // namespace oneui
