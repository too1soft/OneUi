#include "oneui/layout/title_bar_bridge.h"

#include "oneui/style_adapter.h"

#include <algorithm>

namespace oneui {
namespace {

Rect inset(Rect rect, float x, float y) {
    rect.x += x;
    rect.y += y;
    rect.width = std::max(0.0f, rect.width - x * 2.0f);
    rect.height = std::max(0.0f, rect.height - y * 2.0f);
    return rect;
}

StyleNode buttonNodeFor(TitleBarButtonId id, const TitleBarBridgeConfig& config) {
    return id == TitleBarButtonId::Close ? config.closeButtonNode : config.buttonNode;
}

IconSymbol buttonSymbolFor(TitleBarButtonId id, bool maximized) {
    switch (id) {
    case TitleBarButtonId::Minimize:
        return IconSymbol::Minimize;
    case TitleBarButtonId::Maximize:
        return maximized ? IconSymbol::Restore : IconSymbol::Maximize;
    case TitleBarButtonId::Close:
    case TitleBarButtonId::None:
        return IconSymbol::Close;
    }
    return IconSymbol::Close;
}

Color iconColorFor(TitleBarButtonId id, StylePseudoMask state) {
    if (id == TitleBarButtonId::Close && (state & StyleStateHover) != 0) {
        return Color{255, 255, 255};
    }
    if ((state & StyleStateHover) != 0) {
        return Color{236, 239, 246};
    }
    return Color{178, 184, 196};
}

TitleBarBridgeButton buildButton(
    const StyleSheet& sheet,
    const TitleBarBridgeConfig& config,
    TitleBarButtonId id,
    Rect frame) {
    const StylePseudoMask state = titleBarBridgeButtonState(id, config);
    TitleBarBridgeButton button;
    button.id = id;
    button.frame = frame;
    button.visual = inset(frame, 6.0f, 5.0f);
    button.icon = inset(frame, 15.0f, 10.0f);
    button.style = buttonStyleBoxFromStyleSheet(sheet, buttonNodeFor(id, config), state);
    button.symbol = buttonSymbolFor(id, config.maximized);
    button.iconColor = iconColorFor(id, state);
    return button;
}

} // namespace

StylePseudoMask titleBarBridgeButtonState(
    TitleBarButtonId id,
    const TitleBarBridgeConfig& config) {
    StylePseudoMask state = StyleStateNone;
    if (config.hoveredButton == id) {
        state |= StyleStateHover;
    }
    if (config.pressedButton == id) {
        state |= StyleStateActive;
    }
    return state;
}

TitleBarBridgeLayout computeTitleBarBridgeLayout(
    const StyleSheet& sheet,
    const TitleBarBridgeConfig& config) {
    TitleBarBridgeLayout layout;
    layout.titleBarStyle = sheet.resolve(config.titleBarNode);
    layout.logoStyle = sheet.resolve(config.logoNode);
    // logo 与图标按 caption 高度垂直居中，而不是写死 y 偏移（否则标题栏高度非默认 34 时会偏上）。
    const float capY = config.chrome.caption.y;
    const float capH = config.chrome.caption.height;
    const float logoBox = 16.0f;
    const float logoIconBox = 10.0f;
    layout.logo = Rect{
        config.chrome.caption.x + 6.0f,
        capY + (capH - logoBox) / 2.0f,
        logoBox,
        logoBox};
    layout.logoIcon = Rect{
        config.chrome.caption.x + 9.0f,
        capY + (capH - logoIconBox) / 2.0f,
        logoIconBox,
        logoIconBox};
    layout.title = Rect{
        config.chrome.caption.x + 30.0f,
        config.chrome.caption.y,
        std::max(0.0f, config.chrome.caption.width - 36.0f),
        config.chrome.caption.height};

    layout.buttons = {
        buildButton(sheet, config, TitleBarButtonId::Minimize, config.chrome.minimizeButton),
        buildButton(sheet, config, TitleBarButtonId::Maximize, config.chrome.maximizeButton),
        buildButton(sheet, config, TitleBarButtonId::Close, config.chrome.closeButton),
    };
    return layout;
}

TitleBarButtonId hitTestTitleBarButton(
    const TitleBarBridgeLayout& layout,
    Point point) {
    for (const auto& button : layout.buttons) {
        if (button.frame.contains(point)) {
            return button.id;
        }
    }
    return TitleBarButtonId::None;
}

} // namespace oneui
