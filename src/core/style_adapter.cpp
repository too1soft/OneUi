#include "oneui/style_adapter.h"

#include <algorithm>
#include <utility>

namespace oneui {
namespace {

FocusRingStyleOverride focusRingOverrideFromStyleBox(const StyleBox& box) {
    FocusRingStyleOverride ring;
    ring.color = box.outlineColor;
    ring.width = box.outlineWidth;
    ring.offset = box.outlineOffset;
    ring.radius = box.radius;
    if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
        ring.visible = true;
    }
    return ring;
}

std::optional<TransitionSpec> transitionOverrideFromStyleBox(const StyleBox& box) {
    if (!box.transitionDurationMs && !box.transitionEasing) {
        return std::nullopt;
    }

    TransitionSpec spec;
    if (box.transitionDurationMs) {
        spec.durationMs = *box.transitionDurationMs;
    }
    if (box.transitionEasing) {
        spec.easing = *box.transitionEasing;
    }
    return spec;
}

std::vector<ControlShadowStyle> controlShadowsFromStyleBox(const StyleBox& box) {
    std::vector<ControlShadowStyle> shadows;
    shadows.reserve(box.shadows.size());
    for (const auto& shadow : box.shadows) {
        shadows.push_back(ControlShadowStyle{
            shadow.color,
            shadow.offset,
            shadow.blurRadius,
            shadow.spreadRadius,
            shadow.inset});
    }
    return shadows;
}

StyleBox resolveState(const StyleSheet& sheet, StyleNode node, StylePseudoMask state) {
    node.state |= state;
    if ((state & StyleStateSelected) != 0 &&
        std::find(node.classes.begin(), node.classes.end(), "selected") == node.classes.end()) {
        node.classes.push_back("selected");
    }
    return sheet.resolve(node);
}

} // namespace

ButtonStateStyleOverride buttonStateStyleOverrideFromStyleBox(const StyleBox& box) {
    ButtonStateStyleOverride override;
    override.background = box.background.color;
    override.foreground = box.foreground;
    override.border = box.borderColor;
    override.borderWidth = box.borderWidth;
    override.radius = box.radius;
    override.fontSize = box.fontSize;
    override.fontWeight = box.fontWeight;
    if (!box.shadows.empty()) {
        override.shadows = controlShadowsFromStyleBox(box);
    }
    if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
        override.focusRing = focusRingOverrideFromStyleBox(box);
    }
    override.transition = transitionOverrideFromStyleBox(box);
    return override;
}

TextFieldStateStyleOverride textFieldStateStyleOverrideFromStyleBox(const StyleBox& box) {
    TextFieldStateStyleOverride override;
    override.background = box.background.color;
    override.foreground = box.foreground;
    override.placeholderForeground = box.placeholderColor;
    override.border = box.borderColor;
    override.selectionBackground = box.selectionColor;
    override.caretColor = box.caretColor;
    override.borderWidth = box.borderWidth;
    override.radius = box.radius;
    override.padding = box.padding;
    if (!box.shadows.empty()) {
        override.shadows = controlShadowsFromStyleBox(box);
    }
    if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
        override.focusRing = focusRingOverrideFromStyleBox(box);
    }
    override.transition = transitionOverrideFromStyleBox(box);
    return override;
}

SwitchStateStyleOverride switchStateStyleOverrideFromStyleBox(const StyleBox& box) {
    SwitchStateStyleOverride override;
    override.trackBackground = box.background.color;
    override.thumbBackground = box.content.backgroundColor;
    override.labelColor = box.foreground;
    override.border = box.borderColor;
    override.borderWidth = box.borderWidth;
    override.radius = box.radius;
    if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
        override.focusRing = focusRingOverrideFromStyleBox(box);
    }
    return override;
}

ListStateStyleOverride listStateStyleOverrideFromStyleBox(const StyleBox& box) {
    ListStateStyleOverride override;
    override.background = box.background.color;
    override.border = box.borderColor;
    override.separator = box.borderColor;
    override.rowBackground = box.content.backgroundColor;
    override.titleColor = box.foreground;
    override.detailColor = box.foreground;
    override.selectedRowBackground = box.content.backgroundColor;
    override.selectedTitleColor = box.foreground;
    override.selectedDetailColor = box.foreground;
    override.borderWidth = box.borderWidth;
    override.radius = box.radius;
    override.rowRadius = box.content.radius;
    override.rowInset = box.padding;
    if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
        override.focusRing = focusRingOverrideFromStyleBox(box);
    }
    return override;
}

InteractiveSurfaceStateStyle interactiveSurfaceStateStyleFromStyleBox(const StyleBox& box) {
    InteractiveSurfaceStateStyle style;
    if (box.background.color) {
        style.background = *box.background.color;
    }
    if (box.borderColor) {
        style.border = *box.borderColor;
    }
    if (box.borderWidth) {
        style.borderWidth = *box.borderWidth;
    }
    if (box.radius) {
        style.radius = *box.radius;
    }
    return style;
}

StyleBox buttonStyleBoxFromStyleSheet(const StyleSheet& sheet, StyleNode node, StylePseudoMask state) {
    return resolveState(sheet, std::move(node), state);
}

StyleBox textFieldStyleBoxFromStyleSheet(const StyleSheet& sheet, StyleNode node, StylePseudoMask state) {
    return resolveState(sheet, std::move(node), state);
}

ButtonStyleOverride buttonStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    ButtonStyleOverride override;
    override.normal = buttonStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateNone));
    override.hovered = buttonStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateHover));
    override.pressed = buttonStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateActive));
    override.disabled = buttonStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateDisabled));
    override.selected = buttonStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateSelected));
    override.focusVisible = buttonStateStyleOverrideFromStyleBox(resolveState(sheet, std::move(node), StyleStateFocus));
    return override;
}

TextFieldStyleOverride textFieldStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    TextFieldStyleOverride override;
    override.normal = textFieldStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateNone));
    override.hovered = textFieldStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateHover));
    override.disabled = textFieldStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateDisabled));
    override.readOnly = textFieldStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateReadOnly));
    override.focusVisible = textFieldStateStyleOverrideFromStyleBox(resolveState(sheet, std::move(node), StyleStateFocus));
    return override;
}

SwitchStyleOverride switchStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    SwitchStyleOverride override;
    override.normal = switchStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateNone));
    override.hovered = switchStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateHover));
    override.pressed = switchStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateActive));
    override.disabled = switchStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateDisabled));
    override.selected = switchStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateSelected));
    override.focusVisible = switchStateStyleOverrideFromStyleBox(resolveState(sheet, std::move(node), StyleStateFocus));
    return override;
}

ListStyleOverride listStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    ListStyleOverride override;
    override.normal = listStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateNone));
    override.hovered = listStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateHover));
    override.pressed = listStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateActive));
    override.selected = listStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateSelected));
    override.disabled = listStateStyleOverrideFromStyleBox(resolveState(sheet, node, StyleStateDisabled));
    override.focusVisible = listStateStyleOverrideFromStyleBox(resolveState(sheet, std::move(node), StyleStateFocus));
    return override;
}

TreeViewStyleOverride treeViewStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    return listStyleOverrideFromStyleSheet(sheet, std::move(node));
}

InteractiveSurfaceStyle interactiveSurfaceStyleFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    InteractiveSurfaceStyle style;
    const StyleBox normal = resolveState(sheet, node, StyleStateNone);
    style.normal = interactiveSurfaceStateStyleFromStyleBox(normal);
    style.hovered = interactiveSurfaceStateStyleFromStyleBox(resolveState(sheet, node, StyleStateHover));
    style.pressed = interactiveSurfaceStateStyleFromStyleBox(resolveState(sheet, node, StyleStateActive));
    style.disabled = interactiveSurfaceStateStyleFromStyleBox(resolveState(sheet, std::move(node), StyleStateDisabled));
    if (const auto transition = transitionOverrideFromStyleBox(normal)) {
        style.transition = *transition;
    }
    return style;
}

StyleBox cardStyleBoxFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    return sheet.resolve(std::move(node));
}

ProductShellStyle productShellStyleFromStyleSheet(const StyleSheet& sheet) {
    ProductShellStyle style;
    style.window = sheet.resolve(StyleNode{"window", {"window"}, StyleStateNone});
    style.titleBar = sheet.resolve(StyleNode{"titlebar", {"titlebar"}, StyleStateNone});
    style.sidebar = sheet.resolve(StyleNode{"aside", {"sidebar"}, StyleStateNone});
    style.header = sheet.resolve(StyleNode{"header", {"topbar"}, StyleStateNone});
    style.content = sheet.resolve(StyleNode{"main", {"content"}, StyleStateNone});
    style.footer = sheet.resolve(StyleNode{"footer", {"footer"}, StyleStateNone});
    style.card = sheet.resolve(StyleNode{"section", {"card"}, StyleStateNone});
    style.navItem = sheet.resolve(StyleNode{"button", {"nav-item"}, StyleStateNone});
    style.selectedNavItem = sheet.resolve(StyleNode{"button", {"nav-item", "selected"}, StyleStateSelected});
    style.statusStrip = sheet.resolve(StyleNode{"section", {"status-strip"}, StyleStateNone});
    return style;
}

} // namespace oneui
