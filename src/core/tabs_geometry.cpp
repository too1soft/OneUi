#include "oneui/controls/tabs.h"

#include "oneui/icon.h"
#include "oneui/style.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {
int Tabs::hitIndex(Point point) const {
    if (!frame().contains(point) || items_.empty()) {
        return -1;
    }
    if (documentMode_ && sizingMode_ == TabsSizingMode::Compact) {
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            auto hit = itemRect(i);
            hit.width = std::max(0.0f, hit.width - resolvedItemStyle(i).trailingInset);
            if (itemFullyVisible(i) && hit.contains(point)) return i;
        }
        return -1;
    }

    const float contentX = point.x - frame().x + scrollOffset_;
    if (sizingMode_ == TabsSizingMode::Equal) {
        const float width = itemWidth(0);
        const int index = width > 0.0f
            ? static_cast<int>(std::floor(contentX / width))
            : -1;
        return index >= 0 && index < static_cast<int>(items_.size()) ? index : -1;
    }

    if (compactItemOffsets_.size() != items_.size() + 1) {
        const int index = static_cast<int>(std::floor(contentX / minimumItemWidth_));
        return index >= 0 && index < static_cast<int>(items_.size()) ? index : -1;
    }

    const auto upper = std::upper_bound(
        compactItemOffsets_.begin(),
        compactItemOffsets_.end(),
        contentX);
    if (upper == compactItemOffsets_.begin() || upper == compactItemOffsets_.end()) {
        return -1;
    }
    const int index = static_cast<int>(std::distance(compactItemOffsets_.begin(), upper) - 1);
    if (documentMode_ && !itemFullyVisible(index)) return -1;
    return index >= 0 && index < static_cast<int>(items_.size()) ? index : -1;
}

void Tabs::updateCompactMetrics(Canvas& canvas) {
    if (sizingMode_ != TabsSizingMode::Compact || items_.empty()) {
        compactItemOffsets_.clear();
        return;
    }

    std::vector<float> offsets;
    offsets.reserve(items_.size() + 1);
    offsets.push_back(0.0f);
    for (std::size_t index = 0; index < items_.size(); ++index) {
        const TabsStyle style = resolvedItemStyle(static_cast<int>(index));
        const bool hasIcon = index < itemIcons_.size() && itemIcons_[index].has_value();
        const float leadingInset = documentMode_ ? 11.0f + (hasIcon ? 23.0f : 0.0f) : style.textInset + (hasIcon ? 20.0f : 0.0f);
        const bool status = documentMode_ && index < itemStatusColors_.size() && itemStatusColors_[index].a > 0;
        // Measure the same reserved space that textRect removes when painting.
        const float trailingInset = documentMode_ ? (closable_ ? 29.0f : 7.0f) + (status ? 21.0f : 0.0f) : (closable_ ? std::max(27.0f, style.textInset) : style.textInset);
        const float measured = canvas.measureTextWidth(items_[index], style.fontSize, style.fontWeight);
        const float desired = style.itemInset.left + style.itemInset.right
            + leadingInset + measured + trailingInset
            + (documentMode_ ? (static_cast<int>(index) == selectedIndex() ? 6.0f : 4.0f) : 0.0f);
        const float width = std::clamp(desired, minimumItemWidth_, maximumItemWidth_);
        offsets.push_back(offsets.back() + width);
    }
    compactItemOffsets_ = std::move(offsets);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maximumScrollOffset());
}

float Tabs::itemWidth(int index) const {
    if (items_.empty()) {
        return 0.0f;
    }
    if (sizingMode_ == TabsSizingMode::Equal) {
        return frame().width / static_cast<float>(items_.size());
    }
    if (compactItemOffsets_.size() == items_.size() + 1
        && index >= 0 && index < static_cast<int>(items_.size())) {
        return compactItemOffsets_[static_cast<std::size_t>(index) + 1]
            - compactItemOffsets_[static_cast<std::size_t>(index)];
    }
    return minimumItemWidth_;
}

float Tabs::itemOffset(int index) const {
    if (index <= 0) {
        return 0.0f;
    }
    if (sizingMode_ == TabsSizingMode::Equal) {
        return itemWidth(0) * static_cast<float>(index);
    }
    if (compactItemOffsets_.size() == items_.size() + 1
        && index < static_cast<int>(compactItemOffsets_.size())) {
        return compactItemOffsets_[static_cast<std::size_t>(index)];
    }
    return minimumItemWidth_ * static_cast<float>(index);
}

float Tabs::contentWidth() const {
    if (items_.empty()) {
        return 0.0f;
    }
    if (sizingMode_ == TabsSizingMode::Equal) {
        return frame().width;
    }
    if (compactItemOffsets_.size() == items_.size() + 1) {
        return compactItemOffsets_.back();
    }
    return minimumItemWidth_ * static_cast<float>(items_.size());
}

float Tabs::maximumScrollOffset() const {
    return std::max(0.0f, contentWidth() - frame().width);
}

Rect Tabs::itemRect(int index) const {
    const Rect rect = frame();
    const float width = itemWidth(index);
    if (documentMode_ && sizingMode_ == TabsSizingMode::Compact && documentItemOffsets_.size() == items_.size()) {
        if (index < 0 || index >= static_cast<int>(documentItemOffsets_.size()) || documentItemOffsets_[index] < 0.0f) return Rect{};
        return Rect{rect.x + documentItemOffsets_[index], rect.y, std::min(width, rect.width), rect.height};
    }
    return Rect{
        rect.x + itemOffset(index) - scrollOffset_,
        rect.y,
        width,
        rect.height};
}

Rect Tabs::textRect(int index) const {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return Rect{};
    }
    const TabsStyle style = resolvedItemStyle(index);
    const Rect item = documentMode_ ? itemRect(index) : itemRect(index).inset(style.itemInset);
    const bool active = index == std::clamp(selectedIndex(), 0, static_cast<int>(items_.size()) - 1);
    const bool showClose = closable_ &&
        (active || index == hoveredIndex_ || index == hoveredCloseIndex_);
    const bool hasIcon = static_cast<std::size_t>(index) < itemIcons_.size() &&
        itemIcons_[static_cast<std::size_t>(index)].has_value();
    const bool status = documentMode_ && static_cast<std::size_t>(index) < itemStatusColors_.size() && itemStatusColors_[index].a > 0;
    const float leadingInset = documentMode_ ? 12.0f + (active ? 1.0f : 0.0f) + (hasIcon ? 23.0f : 0.0f) : style.textInset + (hasIcon ? 20.0f : 0.0f);
    const float trailingInset = documentMode_ ? (closable_ ? 29.0f : 7.0f) + (status ? 21.0f : 0.0f) + 3.0f + (active ? 1.0f : 0.0f) : (showClose ? 27.0f : style.textInset);
    return Rect{
        item.x + leadingInset,
        item.y + (documentMode_ ? (active ? 0.5f : -0.5f) : 0.0f),
        std::max(0.0f, item.width - leadingInset - trailingInset),
        item.height};
}

Rect Tabs::closeRect(int index) const {
    if (documentMode_) {
        const Rect cell = itemRect(index);
        const float side = std::min(24.0f, cell.height);
        const bool active = index == selectedIndex();
        return Rect{cell.x + cell.width - 5.0f - side - (active ? 1.0f : 0.0f),
            cell.y + (cell.height-side)*.5f + 3.0f + (active ? 0.5f : 0.0f), side, side};
    }
    const Rect item = itemRect(index).inset(Insets{4.0f});
    const float side = std::min(22.0f, item.height);
    return Rect{item.x + item.width - side, item.y + (item.height - side) * 0.5f, side, side};
}

int Tabs::hitCloseIndex(Point point) const {
    if (!closable_) {
        return -1;
    }
    const int index = hitIndex(point);
    if (index < 0) {
        return -1;
    }
    const int selected = items_.empty()
        ? -1
        : std::clamp(selectedIndex(), 0, static_cast<int>(items_.size()) - 1);
    const bool visible = index == selected || index == hoveredIndex_ || index == hoveredCloseIndex_;
    return visible && closeRect(index).contains(point) ? index : -1;
}

void Tabs::ensureIndexVisible(int index) {
    if (documentMode_ && sizingMode_ == TabsSizingMode::Compact && !items_.empty()) {
        documentItemOffsets_.assign(items_.size(), -1.0f);
        const float available = std::max(0.0f, frame().width);
        float used = 0.0f;
        std::vector<int> shown;
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            if (used + itemWidth(i) <= available) {
                documentItemOffsets_[i] = used;
                used += itemWidth(i);
                shown.push_back(i);
            }
        }
        const int active = std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
        if (documentItemOffsets_[active] < 0.0f) {
            const float width = std::min(itemWidth(active), available);
            while (!shown.empty() && used + width > available) {
                const int last = shown.back(); shown.pop_back();
                used -= itemWidth(last);
                documentItemOffsets_[last] = -1.0f;
            }
            documentItemOffsets_[active] = used;
        }
        scrollOffset_ = 0.0f;
        return;
    }
    if (sizingMode_ != TabsSizingMode::Compact || items_.empty()) {
        scrollOffset_ = 0.0f;
        return;
    }
    const int bounded = std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
    const float left = itemOffset(bounded);
    const float right = left + itemWidth(bounded);
    if (left < scrollOffset_) {
        scrollOffset_ = left;
    } else if (right > scrollOffset_ + frame().width) {
        scrollOffset_ = right - frame().width;
    }
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maximumScrollOffset());
}

} // namespace oneui
