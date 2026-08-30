#include "oneui/controls/tabs.h"

#include "oneui/icon.h"
#include "oneui/style.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {
namespace {

void applyFocusRingOverride(FocusRingStyle& style, const FocusRingStyleOverride& override) {
    if (override.color) {
        style.color = *override.color;
    }
    if (override.width) {
        style.width = *override.width;
    }
    if (override.offset) {
        style.offset = *override.offset;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.visible) {
        style.visible = *override.visible;
    }
}

void applyTabsStateOverride(TabsStyle& style, const TabsStateStyleOverride& override) {
    if (override.background) {
        style.background = *override.background;
    }
    if (override.border) {
        style.border = *override.border;
    }
    if (override.itemBackground) {
        style.itemBackground = *override.itemBackground;
    }
    if (override.itemForeground) {
        style.itemForeground = *override.itemForeground;
    }
    if (override.selectedItemBackground) {
        style.selectedItemBackground = *override.selectedItemBackground;
    }
    if (override.selectedItemForeground) {
        style.selectedItemForeground = *override.selectedItemForeground;
    }
    if (override.selectedItemBorder) {
        style.selectedItemBorder = *override.selectedItemBorder;
    }
    if (override.borderWidth) {
        style.borderWidth = *override.borderWidth;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.itemRadius) {
        style.itemRadius = *override.itemRadius;
    }
    if (override.itemBorderWidth) {
        style.itemBorderWidth = *override.itemBorderWidth;
    }
    if (override.itemInset) {
        style.itemInset = *override.itemInset;
    }
    if (override.focusRing) {
        applyFocusRingOverride(style.focusRing, *override.focusRing);
    }
}

TabsStyle baseTabsStyle(bool selected, bool disabled, bool hovered, bool pressed) {
    const auto& t = theme();
    TabsStyle style;
    style.background = disabled ? t.surfaceMuted : Color{241, 245, 249};
    style.border = disabled ? t.border : Color{226, 232, 240};
    style.itemBackground = Color{0, 0, 0, 0};
    style.itemForeground = disabled ? t.textSubtle : t.textMuted;
    style.selectedItemBackground = disabled ? t.surfaceMuted : t.surface;
    style.selectedItemForeground = disabled ? t.textSubtle : t.primary;
    style.selectedItemBorder = disabled ? t.border : Color{226, 232, 240};
    style.borderWidth = 1.0f;
    style.radius = t.radiusMd;
    style.itemRadius = t.radiusMd;
    style.itemBorderWidth = 1.0f;
    style.itemInset = Insets{2.0f};
    style.focusRing = FocusRingStyle{t.focusOutline, t.focusOutlineWidth, t.focusOutlineOffset, t.radiusLg, true};

    if (selected) {
        style.itemBackground = style.selectedItemBackground;
        style.itemForeground = style.selectedItemForeground;
    } else if (!disabled && hovered) {
        style.itemBackground = Color{248, 250, 252};
    }

    if (!disabled && pressed) {
        style.itemBackground = selected ? Color{239, 246, 255} : Color{232, 236, 242};
    }

    return style;
}

} // namespace

Tabs::Tabs() {
    setPreferredSize(Size{260.0f, 32.0f});
}

void Tabs::setItems(std::vector<std::wstring> items) {
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

void Tabs::setItemIcons(std::vector<std::optional<IconSymbol>> icons) {
    itemIcons_ = std::move(icons);
    compactItemOffsets_.clear();
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

void Tabs::paint(Canvas& canvas) {
    const Rect rect = frame();
    const int count = static_cast<int>(items_.size());
    const TabsStyle containerStyle = resolvedContainerStyle();

    canvas.fillRect(rect, containerStyle.background, containerStyle.radius);
    canvas.strokeRect(rect, containerStyle.border, containerStyle.radius, containerStyle.borderWidth);

    if (focusVisible() && !disabled() && containerStyle.focusRing.visible) {
        const float offset = containerStyle.focusRing.offset;
        canvas.strokeRect(
            Rect{rect.x - offset, rect.y - offset, rect.width + offset * 2.0f, rect.height + offset * 2.0f},
            containerStyle.focusRing.color,
            containerStyle.focusRing.radius,
            containerStyle.focusRing.width);
    }

    if (count == 0) {
        return;
    }

    updateCompactMetrics(canvas);
    const int selected = std::clamp(selectedIndex(), 0, count - 1);
    ensureIndexVisible(selected);

    canvas.save();
    canvas.clipRect(rect);
    for (int i = 0; i < count; ++i) {
        const TabsStyle itemStyle = resolvedItemStyle(i);
        const Rect itemCell = itemRect(i);
        const Rect itemRect = itemCell.inset(itemStyle.itemInset);
        const bool active = i == selected;
        if (itemStyle.itemBackground.a > 0) {
            canvas.fillRect(itemRect, itemStyle.itemBackground, itemStyle.itemRadius);
        }
        if (active) {
            canvas.strokeRect(itemRect, itemStyle.selectedItemBorder, itemStyle.itemRadius, itemStyle.itemBorderWidth);
        }
        const bool showClose = closable_ && (active || i == hoveredIndex_ || i == hoveredCloseIndex_);
        const auto icon = static_cast<std::size_t>(i) < itemIcons_.size()
            ? itemIcons_[static_cast<std::size_t>(i)]
            : std::nullopt;
        const float leadingInset = icon.has_value() ? 28.0f : 8.0f;
        const float textRightInset = showClose ? 27.0f : 8.0f;
        const Rect textRect{
            itemRect.x + leadingInset,
            itemRect.y,
            std::max(0.0f, itemRect.width - leadingInset - textRightInset),
            itemRect.height};
        if (icon.has_value()) {
            const float iconSize = std::min(16.0f, std::max(0.0f, itemRect.height - 8.0f));
            const Rect iconRect{
                itemRect.x + 8.0f,
                itemRect.y + (itemRect.height - iconSize) * 0.5f,
                iconSize,
                iconSize};
            paintIcon(
                canvas,
                *icon,
                iconRect,
                itemStyle.itemForeground,
                Color{0, 0, 0, 0},
                1.35f);
        }
        canvas.drawTextEllipsized(
            items_[static_cast<std::size_t>(i)],
            textRect,
            itemStyle.itemForeground,
            theme().fontMd,
            TextAlign::Left);
        if (showClose) {
            const Rect iconRect = closeRect(i);
            if (i == hoveredCloseIndex_ || i == pressedCloseIndex_) {
                const Color hoverBackground = i == pressedCloseIndex_
                    ? itemStyle.selectedItemBorder
                    : itemStyle.itemBackground;
                if (hoverBackground.a > 0) {
                    canvas.fillRect(iconRect, hoverBackground, std::min(4.0f, itemStyle.itemRadius));
                }
            }
            paintIcon(
                canvas,
                IconSymbol::Close,
                iconRect.inset(Insets{4.0f}),
                itemStyle.itemForeground,
                Color{0, 0, 0, 0},
                1.35f);
        }
    }
    if (dragging_ && dragSourceIndex_ >= 0 && dragTargetIndex_ >= 0
        && dragSourceIndex_ != dragTargetIndex_) {
        const Rect target = itemRect(dragTargetIndex_);
        const float x = dragSourceIndex_ < dragTargetIndex_ ? target.x + target.width : target.x;
        const float inset = 4.0f;
        canvas.drawLine(
            Point{x, rect.y + inset},
            Point{x, rect.y + rect.height - inset},
            theme().primary,
            2.0f);
    }
    canvas.restore();
}

bool Tabs::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const int next = hitIndex(event.position);
    const int nextClose = hitCloseIndex(event.position);
    bool interactionChanged = next != hoveredIndex_ || nextClose != hoveredCloseIndex_;
    if (reorderEnabled_ && pressedIndex_ >= 0 && pressedCloseIndex_ < 0) {
        const float deltaX = event.position.x - dragStart_.x;
        const float deltaY = event.position.y - dragStart_.y;
        if (!dragging_ && std::hypot(deltaX, deltaY) >= 5.0f) {
            dragging_ = true;
            dragSourceIndex_ = pressedIndex_;
            interactionChanged = true;
        }
        if (dragging_ && next >= 0 && dragTargetIndex_ != next) {
            dragTargetIndex_ = next;
            interactionChanged = true;
        }
    }
    if (!interactionChanged) {
        return false;
    }
    hoveredIndex_ = next;
    hoveredCloseIndex_ = nextClose;
    invalidate();
    return true;
}

bool Tabs::onMouseDown(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (event.button == MouseButton::Middle) {
        pressedCloseIndex_ = hitIndex(event.position);
        invalidate();
        return pressedCloseIndex_ >= 0;
    }
    if (event.button == MouseButton::Right) {
        contextPressedIndex_ = hitIndex(event.position);
        return contextPressedIndex_ >= 0;
    }
    if (event.button != MouseButton::Left) {
        return false;
    }
    const int closeIndex = hitCloseIndex(event.position);
    if (closeIndex >= 0) {
        pressedCloseIndex_ = closeIndex;
        invalidate();
        return true;
    }
    const int index = hitIndex(event.position);
    if (index < 0) {
        return false;
    }
    pressedIndex_ = index;
    dragStart_ = event.position;
    dragSourceIndex_ = -1;
    dragTargetIndex_ = -1;
    dragging_ = false;
    invalidate();
    return true;
}

bool Tabs::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const int wasClosePressed = pressedCloseIndex_;
    pressedCloseIndex_ = -1;
    const int closeIndex = event.button == MouseButton::Middle
        ? hitIndex(event.position)
        : hitCloseIndex(event.position);
    if (wasClosePressed >= 0 && wasClosePressed == closeIndex) {
        if (onCloseRequested_) {
            onCloseRequested_(closeIndex);
        }
        invalidate();
        return true;
    }
    if (event.button == MouseButton::Right) {
        const int wasContextPressed = contextPressedIndex_;
        contextPressedIndex_ = -1;
        const int contextIndex = hitIndex(event.position);
        if (wasContextPressed >= 0 && wasContextPressed == contextIndex) {
            if (onContextMenuRequested_) {
                onContextMenuRequested_(contextIndex, event.position);
            }
            invalidate();
            return true;
        }
        invalidate();
        return wasContextPressed >= 0;
    }
    if (event.button != MouseButton::Left) {
        invalidate();
        return false;
    }
    const int wasPressed = pressedIndex_;
    pressedIndex_ = -1;
    const bool wasDragging = dragging_;
    const int dragSource = dragSourceIndex_;
    const int dragTarget = dragTargetIndex_;
    dragging_ = false;
    dragSourceIndex_ = -1;
    dragTargetIndex_ = -1;
    if (wasDragging) {
        if (dragSource >= 0 && dragTarget >= 0 && dragSource != dragTarget
            && onReorderRequested_) {
            onReorderRequested_(dragSource, dragTarget);
        }
        invalidate();
        return true;
    }
    const int index = hitIndex(event.position);
    if (wasPressed >= 0 && wasPressed == index) {
        assignSelectedIndex(index);
    }
    invalidate();
    return wasPressed >= 0;
}

bool Tabs::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive() || sizingMode_ != TabsSizingMode::Compact
        || !frame().contains(event.position) || maximumScrollOffset() <= 0.0f
        || std::abs(event.deltaY) < 0.001f) {
        return false;
    }
    const float next = std::clamp(
        scrollOffset_ - event.deltaY * 48.0f,
        0.0f,
        maximumScrollOffset());
    if (next == scrollOffset_) {
        return false;
    }
    scrollOffset_ = next;
    invalidate();
    return true;
}

bool Tabs::onKeyDown(const KeyEvent& event) {
    if (!interactive() || items_.empty()) {
        return false;
    }

    if (event.key == Key::Left || event.key == Key::Up) {
        assignSelectedIndex(selectedIndex() - 1);
        return true;
    }

    if (event.key == Key::Right || event.key == Key::Down) {
        assignSelectedIndex(selectedIndex() + 1);
        return true;
    }

    if (event.key == Key::Home) {
        assignSelectedIndex(0);
        return true;
    }

    if (event.key == Key::End) {
        assignSelectedIndex(static_cast<int>(items_.size()) - 1);
        return true;
    }

    return false;
}

bool Tabs::isFocusable() const {
    return interactive() && !items_.empty();
}

AccessibilityInfo Tabs::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::TabList;
    }
    if (!items_.empty()) {
        const int index = std::clamp(selectedIndex(), 0, static_cast<int>(items_.size()) - 1);
        info.value = items_[static_cast<std::size_t>(index)];
        info.state.selected = true;
    }
    return info;
}

int Tabs::hitIndex(Point point) const {
    if (!frame().contains(point) || items_.empty()) {
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
        const float leadingInset = hasIcon ? 28.0f : 8.0f;
        const float trailingInset = closable_ ? 27.0f : 8.0f;
        const float measured = canvas.measureTextWidth(items_[index], theme().fontMd);
        const float desired = style.itemInset.left + style.itemInset.right
            + leadingInset + measured + trailingInset;
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
    return Rect{
        rect.x + itemOffset(index) - scrollOffset_,
        rect.y,
        width,
        rect.height};
}

Rect Tabs::closeRect(int index) const {
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

TabsStyle Tabs::resolvedContainerStyle() const {
    TabsStyle style = baseTabsStyle(false, disabled(), false, false);
    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        applyTabsStateOverride(style, *styleOverride_->normal);
    }
    if (disabled() && styleOverride_->disabled) {
        applyTabsStateOverride(style, *styleOverride_->disabled);
    }
    if (focusVisible() && styleOverride_->focusVisible) {
        applyTabsStateOverride(style, *styleOverride_->focusVisible);
    }
    return style;
}

TabsStyle Tabs::resolvedItemStyle(int index) const {
    const bool active = index == std::clamp(selectedIndex(), 0, std::max(0, static_cast<int>(items_.size()) - 1));
    const bool hovered = index == hoveredIndex_;
    const bool pressed = index == pressedIndex_;
    TabsStyle style = baseTabsStyle(active, disabled(), hovered, pressed);
    if (!styleOverride_) {
        return style;
    }

    if (styleOverride_->normal) {
        applyTabsStateOverride(style, *styleOverride_->normal);
    }
    if (!disabled() && active) {
        style.itemBackground = style.selectedItemBackground;
        style.itemForeground = style.selectedItemForeground;
    }
    if (disabled() && styleOverride_->disabled) {
        applyTabsStateOverride(style, *styleOverride_->disabled);
    } else if (pressed && styleOverride_->pressed) {
        applyTabsStateOverride(style, *styleOverride_->pressed);
    } else if (hovered && styleOverride_->hovered) {
        applyTabsStateOverride(style, *styleOverride_->hovered);
    }
    if (active && styleOverride_->selected) {
        applyTabsStateOverride(style, *styleOverride_->selected);
        style.itemBackground = style.selectedItemBackground;
        style.itemForeground = style.selectedItemForeground;
    }
    if (active) {
        style.itemBackground = style.selectedItemBackground;
        style.itemForeground = style.selectedItemForeground;
    }
    return style;
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

bool Tabs::hasInteractionState() const {
    return hoveredIndex_ >= 0 || pressedIndex_ >= 0 || hoveredCloseIndex_ >= 0
        || pressedCloseIndex_ >= 0 || contextPressedIndex_ >= 0 || dragging_;
}

void Tabs::resetInteractionState() {
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    hoveredCloseIndex_ = -1;
    pressedCloseIndex_ = -1;
    contextPressedIndex_ = -1;
    dragSourceIndex_ = -1;
    dragTargetIndex_ = -1;
    dragging_ = false;
}

} // namespace oneui
