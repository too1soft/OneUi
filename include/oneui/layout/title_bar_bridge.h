#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/icon.h"
#include "oneui/layout/product_shell.h"
#include "oneui/style_sheet.h"

#include <array>

namespace oneui {

enum class TitleBarButtonId {
    None = 0,
    Minimize = 1,
    Maximize = 2,
    Close = 3,
};

struct TitleBarBridgeConfig {
    ProductWindowChromeLayout chrome;
    bool maximized = false;
    TitleBarButtonId hoveredButton = TitleBarButtonId::None;
    TitleBarButtonId pressedButton = TitleBarButtonId::None;
    StyleNode titleBarNode{"titlebar", {"titlebar"}, StyleStateNone};
    StyleNode logoNode{"div", {"logo"}, StyleStateNone};
    StyleNode buttonNode{"button", {"chrome-button"}, StyleStateNone};
    StyleNode closeButtonNode{"button", {"chrome-button", "close"}, StyleStateNone};
};

struct TitleBarBridgeButton {
    TitleBarButtonId id = TitleBarButtonId::None;
    Rect frame{};
    Rect visual{};
    Rect icon{};
    StyleBox style;
    IconSymbol symbol = IconSymbol::Close;
    Color iconColor{178, 184, 196};
};

struct TitleBarBridgeLayout {
    StyleBox titleBarStyle;
    StyleBox logoStyle;
    Rect logo{};
    Rect logoIcon{};
    Rect title{};
    std::array<TitleBarBridgeButton, 3> buttons{};
};

ONEUI_API StylePseudoMask titleBarBridgeButtonState(
    TitleBarButtonId id,
    const TitleBarBridgeConfig& config);

ONEUI_API TitleBarBridgeLayout computeTitleBarBridgeLayout(
    const StyleSheet& sheet,
    const TitleBarBridgeConfig& config);

ONEUI_API TitleBarButtonId hitTestTitleBarButton(
    const TitleBarBridgeLayout& layout,
    Point point);

} // namespace oneui
