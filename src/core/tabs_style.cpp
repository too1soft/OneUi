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
    if (override.fontSize) {
        style.fontSize = *override.fontSize;
    }
    if (override.fontWeight) {
        style.fontWeight = *override.fontWeight;
    }
    if (override.indicatorColor) style.indicatorColor = override.indicatorColor;
    if (override.indicatorInset && std::isfinite(*override.indicatorInset)) style.indicatorInset = std::max(0.0f, *override.indicatorInset);
    if (override.indicatorHeight && std::isfinite(*override.indicatorHeight)) style.indicatorHeight = std::max(0.0f, *override.indicatorHeight);
    if (override.documentOutlineWidth && std::isfinite(*override.documentOutlineWidth)) style.documentOutlineWidth = std::max(0.0f, *override.documentOutlineWidth);
    if (override.trailingInset && std::isfinite(*override.trailingInset)) style.trailingInset = std::max(0.0f, *override.trailingInset);
    if (override.textInset) style.textInset = std::max(0.0f, *override.textInset);
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
    style.fontSize = t.fontMd;
    style.fontWeight = 400;
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

} // namespace oneui
