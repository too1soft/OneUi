#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/style_sheet.h"

namespace oneui {

struct TextInputBridgeConfig {
    StyleNode node{"input", {"input"}, StyleStateNone};
    bool focused = false;
    bool hovered = false;
    bool disabled = false;
    bool readOnly = false;
    bool revealNativeEditor = false;
    float leadingReservedWidth = 0.0f;
    float maxInnerHorizontalInset = 4.0f;
    float maxInnerVerticalInset = 2.0f;
    float maxEditorHeight = 20.0f;
};

struct TextInputBridgeLayout {
    StyleBox frameStyle;
    Rect contentRect{};
    Rect editorRect{};
    Rect hiddenEditorRect{};
    bool showNativeEditor = false;
};

ONEUI_API StylePseudoMask textInputBridgeState(const TextInputBridgeConfig& config);
ONEUI_API TextInputBridgeLayout computeTextInputBridgeLayout(
    const StyleSheet& sheet,
    Rect frame,
    TextInputBridgeConfig config);

} // namespace oneui
