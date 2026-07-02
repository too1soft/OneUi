#include "oneui/layout/sidebar_nav_bridge.h"

#include "oneui/style_adapter.h"

#include <algorithm>

namespace oneui {

StylePseudoMask sidebarNavItemBridgeState(const SidebarNavItemBridgeConfig& config) {
    StylePseudoMask state = StyleStateNone;
    if (config.selected) {
        state |= StyleStateSelected;
    }
    if (config.hovered && !config.disabled) {
        state |= StyleStateHover;
    }
    if (config.pressed && !config.disabled) {
        state |= StyleStateActive;
    }
    if (config.disabled) {
        state |= StyleStateDisabled;
    }
    return state;
}

SidebarNavItemBridgeLayout computeSidebarNavItemBridgeLayout(
    const StyleSheet& sheet,
    SidebarNavItemBridgeConfig config) {
    const StylePseudoMask state = sidebarNavItemBridgeState(config);
    StyleNode node = config.selected ? config.selectedItemNode : config.itemNode;
    SidebarNavItemBridgeLayout layout;
    layout.frame = config.frame;
    layout.icon = config.icon;
    layout.symbol = config.symbol;
    layout.style = buttonStyleBoxFromStyleSheet(sheet, node, state);
    layout.label = Rect{
        config.frame.x + config.labelLeftOffset,
        config.frame.y,
        std::max(0.0f, config.frame.width - config.labelLeftOffset - config.labelRightInset),
        config.frame.height};

    const Color fallback = config.disabled ? config.disabledForeground :
        config.selected ? config.selectedForeground : config.normalForeground;
    layout.foreground = layout.style.foreground.value_or(fallback);
    const float opacity = std::max(0.0f, std::min(1.0f, layout.style.opacity.value_or(1.0f)));
    layout.foreground.a = static_cast<unsigned char>(static_cast<float>(layout.foreground.a) * opacity);
    return layout;
}

bool hitTestSidebarNavItem(
    const SidebarNavItemBridgeLayout& layout,
    Point point) {
    return layout.frame.contains(point);
}

} // namespace oneui
