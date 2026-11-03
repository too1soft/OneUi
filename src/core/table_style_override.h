#pragma once
#include "oneui/style.h"

namespace oneui::detail {
inline void applyTableStyleOverride(TableStyle& style, const TableStyleOverride& override) {
    if (override.checkBackground) style.checkBackground = override.checkBackground;
    if (override.checkBorder) style.checkBorder = override.checkBorder;
    if (override.checkRadius) style.checkRadius = *override.checkRadius;
    if (override.background) style.background = *override.background;
    if (override.border) style.border = *override.border;
    if (override.headerBackground) style.headerBackground = *override.headerBackground;
    if (override.headerForeground) style.headerForeground = *override.headerForeground;
    if (override.cellForeground) style.cellForeground = *override.cellForeground;
    if (override.gridLine) style.gridLine = *override.gridLine;
    if (override.rowHovered) style.rowHovered = *override.rowHovered;
    if (override.rowPressed) style.rowPressed = *override.rowPressed;
    if (override.rowSelected) style.rowSelected = *override.rowSelected;
    if (override.scrollbarColor) style.scrollbarColor = *override.scrollbarColor;
    if (override.borderWidth) style.borderWidth = *override.borderWidth;
    if (override.radius) style.radius = *override.radius;
    if (override.fontSize) style.fontSize = *override.fontSize;
    if (override.headerFontSize) style.headerFontSize = *override.headerFontSize;
    if (override.detailFontSize) style.detailFontSize = *override.detailFontSize;
    if (override.detailForeground) style.detailForeground = *override.detailForeground;
    if (override.headerHeight) style.headerHeight = *override.headerHeight;
    if (override.scrollbarWidth) style.scrollbarWidth = *override.scrollbarWidth;
    if (override.cellPadding) style.cellPadding = *override.cellPadding;
    if (override.headerTextInset) style.headerTextInset = override.headerTextInset;
    if (override.cellIconGap) style.cellIconGap = *override.cellIconGap;
}
}
