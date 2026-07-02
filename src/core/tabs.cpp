#include "oneui/controls/tabs.h"

#include "oneui/style.h"

#include <algorithm>
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
    assignSelectedIndex(selectedIndex());
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

void Tabs::setOnChanged(std::function<void(int)> callback) {
    onChanged_ = std::move(callback);
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

    const float itemWidth = rect.width / static_cast<float>(count);
    const int selected = std::clamp(selectedIndex(), 0, count - 1);

    for (int i = 0; i < count; ++i) {
        const TabsStyle itemStyle = resolvedItemStyle(i);
        const Rect itemCell{rect.x + itemWidth * i, rect.y, itemWidth, rect.height};
        const Rect itemRect = itemCell.inset(itemStyle.itemInset);
        const bool active = i == selected;
        if (itemStyle.itemBackground.a > 0) {
            canvas.fillRect(itemRect, itemStyle.itemBackground, itemStyle.itemRadius);
        }
        if (active) {
            canvas.strokeRect(itemRect, itemStyle.selectedItemBorder, itemStyle.itemRadius, itemStyle.itemBorderWidth);
        }
        canvas.drawText(items_[static_cast<std::size_t>(i)], itemRect, itemStyle.itemForeground, theme().fontMd, TextAlign::Center);
    }
}

bool Tabs::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const int next = hitIndex(event.position);
    if (next == hoveredIndex_) {
        return false;
    }
    hoveredIndex_ = next;
    invalidate();
    return true;
}

bool Tabs::onMouseDown(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const int index = hitIndex(event.position);
    if (index < 0) {
        return false;
    }
    pressedIndex_ = index;
    invalidate();
    return true;
}

bool Tabs::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    const int wasPressed = pressedIndex_;
    pressedIndex_ = -1;
    const int index = hitIndex(event.position);
    if (wasPressed >= 0 && wasPressed == index) {
        assignSelectedIndex(index);
    }
    invalidate();
    return wasPressed >= 0;
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

    const float itemWidth = frame().width / static_cast<float>(items_.size());
    return std::clamp(static_cast<int>((point.x - frame().x) / itemWidth), 0, static_cast<int>(items_.size()) - 1);
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
    invalidate();
    if (previous != next && onChanged_) {
        onChanged_(next);
    }
}

bool Tabs::hasInteractionState() const {
    return hoveredIndex_ >= 0 || pressedIndex_ >= 0;
}

void Tabs::resetInteractionState() {
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
}

} // namespace oneui
