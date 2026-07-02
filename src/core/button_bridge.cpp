#include "oneui/controls/button_bridge.h"

#include "oneui/style_adapter.h"

#include <algorithm>

namespace oneui {

StylePseudoMask buttonBridgeState(const ButtonBridgeConfig& config) {
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
    if (config.focusVisible && !config.disabled) {
        state |= StyleStateFocus;
    }
    return state;
}

ButtonBridgeLayout computeButtonBridgeLayout(
    const StyleSheet& sheet,
    ButtonBridgeConfig config) {
    ButtonBridgeLayout layout;
    layout.frame = config.frame;
    layout.style = buttonStyleBoxFromStyleSheet(sheet, config.node, buttonBridgeState(config));
    layout.content = styleContentRect(config.frame.inset(config.contentInset), layout.style);

    const Color fallback = config.disabled ? config.disabledForeground : config.normalForeground;
    layout.foreground = layout.style.foreground.value_or(fallback);
    const float opacity = std::max(0.0f, std::min(1.0f, layout.style.opacity.value_or(1.0f)));
    layout.foreground.a = static_cast<unsigned char>(static_cast<float>(layout.foreground.a) * opacity);
    return layout;
}

bool hitTestButtonBridge(
    const ButtonBridgeLayout& layout,
    Point point) {
    return layout.frame.contains(point);
}

} // namespace oneui
