#include "oneui/controls/text_input_bridge.h"

#include "oneui/style_adapter.h"

#include <algorithm>
#include <utility>

namespace oneui {
namespace {

Rect reserveLeading(Rect rect, float width) {
    const float reserve = std::max(0.0f, std::min(width, rect.width));
    rect.x += reserve;
    rect.width = std::max(0.0f, rect.width - reserve);
    return rect;
}

Rect insetEditor(Rect rect, const TextInputBridgeConfig& config) {
    const float horizontal = std::min(std::max(0.0f, config.maxInnerHorizontalInset), std::max(0.0f, rect.width * 0.08f));
    const float vertical = std::min(std::max(0.0f, config.maxInnerVerticalInset), std::max(0.0f, rect.height * 0.14f));
    rect = Rect{rect.x + horizontal, rect.y + vertical, std::max(0.0f, rect.width - horizontal * 2.0f), std::max(0.0f, rect.height - vertical * 2.0f)};

    const float max_height = std::max(1.0f, config.maxEditorHeight);
    if (rect.height > max_height) {
        const float shrink = (rect.height - max_height) * 0.5f;
        rect.y += shrink;
        rect.height = max_height;
    }
    return rect;
}

} // namespace

StylePseudoMask textInputBridgeState(const TextInputBridgeConfig& config) {
    StylePseudoMask state = StyleStateNone;
    if (config.hovered && !config.disabled) {
        state |= StyleStateHover;
    }
    if (config.focused && !config.disabled) {
        state |= StyleStateFocus;
    }
    if (config.disabled) {
        state |= StyleStateDisabled;
    }
    if (config.readOnly) {
        state |= StyleStateReadOnly;
    }
    return state;
}

TextInputBridgeLayout computeTextInputBridgeLayout(
    const StyleSheet& sheet,
    Rect frame,
    TextInputBridgeConfig config) {
    TextInputBridgeLayout layout;
    const StylePseudoMask state = textInputBridgeState(config);
    layout.frameStyle = textFieldStyleBoxFromStyleSheet(sheet, config.node, state);
    layout.contentRect = reserveLeading(styleContentRect(frame, layout.frameStyle), config.leadingReservedWidth);
    layout.editorRect = insetEditor(layout.contentRect, config);
    layout.hiddenEditorRect = Rect{
        layout.contentRect.x + std::min(4.0f, std::max(0.0f, layout.contentRect.width)),
        layout.contentRect.y + std::max(0.0f, layout.contentRect.height * 0.5f),
        1.0f,
        1.0f};
    layout.showNativeEditor = config.revealNativeEditor && !config.disabled && !config.readOnly;
    return layout;
}

} // namespace oneui
