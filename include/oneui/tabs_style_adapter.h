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
        oneui::TabsStyleOverride style;
        style.normal = stateOverride(sheet.resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateNone}));
        style.hovered = stateOverride(sheet.resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateHover}));
        style.pressed = stateOverride(sheet.resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateActive}));
        style.selected = stateOverride(sheet.resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateSelected}));
        style.disabled = stateOverride(sheet.resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateDisabled}));
        style.focusVisible = stateOverride(sheet.resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateFocus}));
    return style;
}
} // namespace oneui
