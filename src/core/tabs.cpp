#include "oneui/controls/tabs.h"

#include "oneui/icon.h"
#include "oneui/style.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {
Tabs::Tabs() {
    setPreferredSize(Size{260.0f, 32.0f});
}

void Tabs::setItems(std::vector<std::wstring> items) {
    if (items != items_) {
        cancelEdit();
    }
    items_ = std::move(items);
    compactItemOffsets_.clear();
    assignSelectedIndex(selectedIndex());
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maximumScrollOffset());
    ensureIndexVisible(selectedIndex());
    invalidate();
}

const std::vector<std::wstring>& Tabs::items() const {
    return items_;
}

Rect Tabs::itemFrame(int index) const {
    return itemRect(index);
}

Rect Tabs::itemTextFrame(int index) const {
    return textRect(index);
}

float Tabs::itemPaintFontSize(int index) const {
    return resolvedItemStyle(index).fontSize;
}

int Tabs::itemPaintFontWeight(int index) const {
    return resolvedItemStyle(index).fontWeight;
}

TextAlign Tabs::itemTextAlign(int index) const {
    (void)index;
    return sizingMode_ == TabsSizingMode::Equal ? TextAlign::Center : TextAlign::Left;
}

void Tabs::setItemIcons(std::vector<std::optional<IconSymbol>> icons) {
    if(itemIcons_==icons)return;
    itemIcons_ = std::move(icons);
    compactItemOffsets_.clear();
    invalidate();
}

void Tabs::setDocumentMode(bool enabled) {
    if (documentMode_ == enabled) return;
    documentMode_ = enabled;
    compactItemOffsets_.clear();
    invalidate();
}

void Tabs::setItemStatusColors(std::vector<Color> colors) {
    const bool metricsChanged=colors.size()!=itemStatusColors_.size() ||
        !std::equal(colors.begin(),colors.end(),itemStatusColors_.begin(),
            [](Color a,Color b){return (a.a>0)==(b.a>0);});
    itemStatusColors_ = std::move(colors);
    if(metricsChanged)compactItemOffsets_.clear();
    invalidate();
}

bool Tabs::itemFullyVisible(int index) const {
    const auto cell = itemRect(index);
    const auto bounds = frame();
    // A single item wider than the viewport must remain usable at narrow sizes.
    return cell.width > 0.0f && cell.x >= bounds.x - 0.5f &&
        (cell.x + cell.width <= bounds.x + bounds.width + 0.5f || cell.width > bounds.width);
}

int Tabs::overflowItemCount() const {
    int hidden = 0;
    for (int i = 0; i < static_cast<int>(items_.size()); ++i)
        if (!itemFullyVisible(i)) ++hidden;
    return hidden;
}

void Tabs::setOnOverflowChanged(std::function<void(int)> callback) {
    onOverflowChanged_ = std::move(callback);
    reportedOverflow_ = -1;
    invalidate();
}

void Tabs::setSelectedIndex(int index) {
    assignSelectedIndex(index);
}

int Tabs::selectedIndex() const {
    return selectedBinding_.get(selectedIndex_);
}

void Tabs::bindSelectedIndex(State<int>& state) {
    selectedBinding_ = Binding<int>(state, [this] {
        invalidate();
    });
    invalidate();
}

void Tabs::setStyleOverride(TabsStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void Tabs::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void Tabs::setSizingMode(TabsSizingMode mode) {
    if (sizingMode_ == mode) {
        return;
    }
    sizingMode_ = mode;
    compactItemOffsets_.clear();
    scrollOffset_ = 0.0f;
    ensureIndexVisible(selectedIndex());
    invalidate();
}

TabsSizingMode Tabs::sizingMode() const {
    return sizingMode_;
}

void Tabs::setItemWidthRange(float minimum, float maximum) {
    const float nextMinimum = std::max(32.0f, minimum);
    const float nextMaximum = std::max(nextMinimum, maximum);
    if (minimumItemWidth_ == nextMinimum && maximumItemWidth_ == nextMaximum) {
        return;
    }
    minimumItemWidth_ = nextMinimum;
    maximumItemWidth_ = nextMaximum;
    compactItemOffsets_.clear();
    scrollOffset_ = std::clamp(scrollOffset_, 0.0f, maximumScrollOffset());
    ensureIndexVisible(selectedIndex());
    invalidate();
}

void Tabs::setClosable(bool closable) {
    if (closable_ == closable) {
        return;
    }
    closable_ = closable;
    compactItemOffsets_.clear();
    hoveredCloseIndex_ = -1;
    pressedCloseIndex_ = -1;
    invalidate();
}

bool Tabs::closable() const {
    return closable_;
}

void Tabs::setReorderEnabled(bool enabled) {
    if (reorderEnabled_ == enabled) {
        return;
    }
    reorderEnabled_ = enabled;
    dragSourceIndex_ = -1;
    dragTargetIndex_ = -1;
    dragging_ = false;
    invalidate();
}

bool Tabs::reorderEnabled() const {
    return reorderEnabled_;
}

void Tabs::setOnChanged(std::function<void(int)> callback) {
    onChanged_ = std::move(callback);
}

void Tabs::setOnCloseRequested(std::function<void(int)> callback) {
    onCloseRequested_ = std::move(callback);
}

void Tabs::setOnContextMenuRequested(std::function<void(int, Point)> callback) {
    onContextMenuRequested_ = std::move(callback);
}

void Tabs::setOnReorderRequested(std::function<void(int, int)> callback) {
    onReorderRequested_ = std::move(callback);
}

void Tabs::assignSelectedIndex(int index) {
    if (items_.empty()) {
        const int previous = 0;
        selectedBinding_.set(0, selectedIndex_);
        invalidate();
        if (previous != 0 && onChanged_) {
            onChanged_(0);
        }
        return;
    }

    const int maxIndex = static_cast<int>(items_.size()) - 1;
    const int previous = std::clamp(selectedIndex(), 0, maxIndex);
    const int next = std::clamp(index, 0, maxIndex);
    selectedBinding_.set(next, selectedIndex_);
    ensureIndexVisible(next);
    invalidate();
    if (previous != next && onChanged_) {
        onChanged_(next);
    }
}

} // namespace oneui
