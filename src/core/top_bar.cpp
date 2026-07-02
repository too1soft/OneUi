#include "oneui/layout/top_bar.h"

#include <algorithm>
#include <utility>

namespace oneui {

void TopBar::setLeading(std::shared_ptr<Widget> child) {
    leading_ = std::move(child);
    rebuildChildren();
}

void TopBar::addAction(std::shared_ptr<Widget> child) {
    if (!child) {
        return;
    }
    actions_.push_back(std::move(child));
    rebuildChildren();
}

void TopBar::clearActions() {
    actions_.clear();
    rebuildChildren();
}

void TopBar::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void TopBar::setGap(float gap) {
    gap_ = std::max(0.0f, gap);
    invalidate();
}

void TopBar::setLeadingWidth(float width) {
    leadingWidth_ = std::max(0.0f, width);
    invalidate();
}

void TopBar::setStyleBox(StyleBox style) {
    styleBox_ = std::move(style);
    invalidate();
}

void TopBar::clearStyleBox() {
    styleBox_.reset();
    invalidate();
}

float TopBar::gap() const {
    return gap_;
}

float TopBar::leadingWidth() const {
    return leadingWidth_;
}

void TopBar::paint(Canvas& canvas) {
    if (styleBox_) {
        paintStyleBox(canvas, frame(), *styleBox_);
    }
    View::paint(canvas);
}

void TopBar::layoutChildren() {
    Rect content = frame().inset(padding_);
    content.width = std::max(0.0f, content.width);
    content.height = std::max(0.0f, content.height);

    float right = content.x + content.width;
    for (auto it = actions_.rbegin(); it != actions_.rend(); ++it) {
        const auto& child = *it;
        if (!child || !child->visible()) {
            continue;
        }

        const Size preferred = child->preferredSize();
        const float width = preferred.width > 0.0f ? std::min(preferred.width, std::max(0.0f, right - content.x)) : 0.0f;
        const float height = preferred.height > 0.0f ? std::min(preferred.height, content.height) : content.height;
        const float x = std::max(content.x, right - width);
        const float y = content.y + std::max(0.0f, (content.height - height) * 0.5f);
        child->setFrame(Rect{x, y, width, height});
        right = std::max(content.x, x - gap_);
    }

    if (leading_ && leading_->visible()) {
        const Size preferred = leading_->preferredSize();
        const float preferredWidth = leadingWidth_ > 0.0f ? leadingWidth_ : preferred.width;
        const float width = preferredWidth > 0.0f
            ? std::min(preferredWidth, std::max(0.0f, right - content.x))
            : std::max(0.0f, right - content.x);
        const float height = preferred.height > 0.0f ? std::min(preferred.height, content.height) : content.height;
        const float y = content.y + std::max(0.0f, (content.height - height) * 0.5f);
        leading_->setFrame(Rect{content.x, y, width, height});
    }
}

void TopBar::rebuildChildren() {
    clearChildren();
    if (leading_) {
        add(leading_);
    }
    for (const auto& action : actions_) {
        if (action) {
            add(action);
        }
    }
    invalidate();
}

} // namespace oneui
