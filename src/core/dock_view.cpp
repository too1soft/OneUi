#include "oneui/layout/dock_view.h"

#include <algorithm>
#include <utility>

namespace oneui {

void DockView::setTop(std::shared_ptr<Widget> child) {
    top_ = std::move(child);
    rebuildChildren();
}

void DockView::setRight(std::shared_ptr<Widget> child) {
    right_ = std::move(child);
    rebuildChildren();
}

void DockView::setBottom(std::shared_ptr<Widget> child) {
    bottom_ = std::move(child);
    rebuildChildren();
}

void DockView::setLeft(std::shared_ptr<Widget> child) {
    left_ = std::move(child);
    rebuildChildren();
}

void DockView::setCenter(std::shared_ptr<Widget> child) {
    center_ = std::move(child);
    rebuildChildren();
}

void DockView::setGap(float gap) {
    gap_ = gap;
    invalidate();
}

void DockView::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void DockView::layoutChildren() {
    Rect remaining = frame().inset(padding_);
    remaining.width = std::max(0.0f, remaining.width);
    remaining.height = std::max(0.0f, remaining.height);

    if (top_ && top_->visible()) {
        const float height = std::min(std::max(0.0f, top_->preferredSize().height), remaining.height);
        top_->setFrame(Rect{remaining.x, remaining.y, remaining.width, height});
        const float used = std::min(remaining.height, height + gap_);
        remaining.y += used;
        remaining.height -= used;
    }

    if (bottom_ && bottom_->visible()) {
        const float height = std::min(std::max(0.0f, bottom_->preferredSize().height), remaining.height);
        bottom_->setFrame(Rect{remaining.x, remaining.y + remaining.height - height, remaining.width, height});
        const float used = std::min(remaining.height, height + gap_);
        remaining.height -= used;
    }

    if (left_ && left_->visible()) {
        const float width = std::min(std::max(0.0f, left_->preferredSize().width), remaining.width);
        left_->setFrame(Rect{remaining.x, remaining.y, width, remaining.height});
        const float used = std::min(remaining.width, width + gap_);
        remaining.x += used;
        remaining.width -= used;
    }

    if (right_ && right_->visible()) {
        const float width = std::min(std::max(0.0f, right_->preferredSize().width), remaining.width);
        right_->setFrame(Rect{remaining.x + remaining.width - width, remaining.y, width, remaining.height});
        const float used = std::min(remaining.width, width + gap_);
        remaining.width -= used;
    }

    if (center_ && center_->visible()) {
        center_->setFrame(remaining);
    }
}

void DockView::rebuildChildren() {
    clearChildren();
    if (top_) {
        add(top_);
    }
    if (right_) {
        add(right_);
    }
    if (bottom_) {
        add(bottom_);
    }
    if (left_) {
        add(left_);
    }
    if (center_) {
        add(center_);
    }
    invalidate();
}

} // namespace oneui
