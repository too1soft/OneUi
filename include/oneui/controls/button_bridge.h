#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/style_sheet.h"

namespace oneui {

struct ButtonBridgeConfig {
    Rect frame{};
    StyleNode node{"button", {"button"}, StyleStateNone};
    bool hovered = false;
    bool pressed = false;
    bool disabled = false;
    bool selected = false;
    bool focusVisible = false;
    Color normalForeground{255, 255, 255};
    Color disabledForeground{150, 154, 164};
    Insets contentInset{0.0f};
};

struct ButtonBridgeLayout {
    Rect frame{};
    Rect content{};
    StyleBox style;
    Color foreground{255, 255, 255};
};

ONEUI_API StylePseudoMask buttonBridgeState(const ButtonBridgeConfig& config);

ONEUI_API ButtonBridgeLayout computeButtonBridgeLayout(
    const StyleSheet& sheet,
    ButtonBridgeConfig config);

ONEUI_API bool hitTestButtonBridge(
    const ButtonBridgeLayout& layout,
    Point point);

} // namespace oneui
