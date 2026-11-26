#pragma once
#include "oneui/controls/tabs.h"
#include "oneui/style_sheet.h"

namespace oneui {
// Shared by legacy C and declarative adapters: preserve the same state mapping.
inline TabsStyleOverride tabsStyleOverrideFromStyleSheet(const StyleSheet& sheet, const StyleNode& node) {
        auto stateOverride = [](const oneui::StyleBox& box) {
            oneui::TabsStateStyleOverride state;
            state.background = box.background.color;
            state.border = box.borderColor;
            state.itemBackground = box.content.backgroundColor ? box.content.backgroundColor : box.background.color;
            state.itemForeground = box.foreground;
            state.selectedItemBackground = box.content.backgroundColor ? box.content.backgroundColor : box.background.color;
            state.selectedItemForeground = box.foreground;
            state.selectedItemBorder = box.borderColor;
            state.borderWidth = box.borderWidth;
            state.radius = box.radius;
            state.itemRadius = box.content.radius ? box.content.radius : box.radius;
            state.itemBorderWidth = box.borderWidth;
            state.textInset = box.textInset;
            state.indicatorColor = box.tabIndicatorColor;
            state.indicatorInset = box.tabIndicatorInset;
            state.indicatorHeight = box.tabIndicatorHeight;
            state.documentOutlineWidth = box.tabOutlineWidth;
            state.trailingInset = box.tabTrailingInset;
            state.fontSize = box.fontSize;
            state.fontWeight = box.fontWeight;
            if (box.padding) {
                state.itemInset = *box.padding;
            }
            if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
                oneui::FocusRingStyleOverride ring;
                ring.color = box.outlineColor;
                ring.width = box.outlineWidth;
                ring.offset = box.outlineOffset;
                ring.radius = box.radius;
                ring.visible = true;
                state.focusRing = ring;
            }
            return state;
        };
        StyleNode stateNode = node;
        const auto resolveState = [&](StylePseudoMask state) {
            stateNode.state = state;
            return stateOverride(sheet.resolve(stateNode));
        };
        oneui::TabsStyleOverride style;
        style.normal = resolveState(StyleStateNone);
        style.hovered = resolveState(StyleStateHover);
        style.pressed = resolveState(StyleStateActive);
        style.selected = resolveState(StyleStateSelected);
        style.disabled = resolveState(StyleStateDisabled);
        style.focusVisible = resolveState(StyleStateFocus);
    return style;
}
} // namespace oneui
