#pragma once

#include "oneui/style.h"

namespace oneui::detail {

inline void applyFocusRingOverride(FocusRingStyle& style, const FocusRingStyleOverride& override) {
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

inline void applyListStateOverride(ListStyle& style, const ListStateStyleOverride& override) {
    if (override.background) {
        style.background = *override.background;
    }
    if (override.border) {
        style.border = *override.border;
    }
    if (override.separator) {
        style.separator = *override.separator;
    }
    if (override.rowBackground) {
        style.rowBackground = *override.rowBackground;
    }
    if (override.titleColor) {
        style.titleColor = *override.titleColor;
    }
    if (override.detailColor) {
        style.detailColor = *override.detailColor;
    }
    if (override.selectedRowBackground) {
        style.selectedRowBackground = *override.selectedRowBackground;
    }
    if (override.selectedTitleColor) {
        style.selectedTitleColor = *override.selectedTitleColor;
    }
    if (override.selectedDetailColor) {
        style.selectedDetailColor = *override.selectedDetailColor;
    }
    if (override.borderWidth) {
        style.borderWidth = *override.borderWidth;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.rowRadius) {
        style.rowRadius = *override.rowRadius;
    }
    if (override.rowInset) {
        style.rowInset = *override.rowInset;
    }
    if (override.textInset) {
        style.textInset = *override.textInset;
    }
    if (override.titleOffsetY) {
        style.titleOffsetY = *override.titleOffsetY;
    }
    if (override.detailOffsetY) {
        style.detailOffsetY = *override.detailOffsetY;
    }
    if (override.focusRing) {
        applyFocusRingOverride(style.focusRing, *override.focusRing);
    }
}

inline ListStyle baseListStyle(bool selected, bool disabled, bool hovered, bool pressed) {
    const auto& t = theme();
    ListStyle style;
    style.background = disabled ? t.surfaceMuted : t.surface;
    style.border = t.border;
    style.separator = t.border;
    style.rowBackground = Color{0, 0, 0, 0};
    style.titleColor = disabled ? t.textSubtle : t.text;
    style.detailColor = disabled ? t.textSubtle : t.textMuted;
    style.selectedRowBackground = t.primarySoft;
    style.selectedTitleColor = disabled ? t.textSubtle : t.primary;
    style.selectedDetailColor = disabled ? t.textSubtle : t.textMuted;
    style.borderWidth = 1.0f;
    style.radius = t.radiusMd;
    style.rowRadius = t.radiusSm;
    style.rowInset = Insets{3.0f, 4.0f};
    style.textInset = 12.0f;
    style.titleOffsetY = 7.0f;
    style.detailOffsetY = 27.0f;
    style.focusRing = FocusRingStyle{t.focusOutline, t.focusOutlineWidth, t.focusOutlineOffset, t.radiusLg, true};

    if (selected) {
        style.rowBackground = style.selectedRowBackground;
        style.titleColor = style.selectedTitleColor;
        style.detailColor = style.selectedDetailColor;
    } else if (!disabled && hovered) {
        style.rowBackground = t.surfaceMuted;
    }

    if (!disabled && pressed) {
        style.rowBackground = selected ? Color{191, 219, 254} : Color{232, 236, 242};
    }

    return style;
}

} // namespace oneui::detail
