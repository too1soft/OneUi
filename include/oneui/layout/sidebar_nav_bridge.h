#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/icon.h"
#include "oneui/style_sheet.h"

namespace oneui {

struct SidebarNavItemBridgeConfig {
    Rect frame{};
    Rect icon{};
    IconSymbol symbol = IconSymbol::RemoteAssist;
    bool selected = false;
    bool hovered = false;
    bool pressed = false;
    bool disabled = false;
    float labelLeftOffset = 39.0f;
    float labelRightInset = 9.0f;
    StyleNode itemNode{"button", {"nav-item"}, StyleStateNone};
    StyleNode selectedItemNode{"button", {"nav-item", "selected"}, StyleStateSelected};
    Color normalForeground{202, 205, 214};
    Color selectedForeground{255, 47, 105};
    Color disabledForeground{139, 145, 158};
};

struct SidebarNavItemBridgeLayout {
    Rect frame{};
    Rect icon{};
    Rect label{};
    StyleBox style;
    IconSymbol symbol = IconSymbol::RemoteAssist;
    Color foreground{202, 205, 214};
};

ONEUI_API StylePseudoMask sidebarNavItemBridgeState(const SidebarNavItemBridgeConfig& config);

ONEUI_API SidebarNavItemBridgeLayout computeSidebarNavItemBridgeLayout(
    const StyleSheet& sheet,
    SidebarNavItemBridgeConfig config);

ONEUI_API bool hitTestSidebarNavItem(
    const SidebarNavItemBridgeLayout& layout,
    Point point);

} // namespace oneui
