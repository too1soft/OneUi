#include "oneui/style_adapter.h"

#include "style_css_internal.h"

#include <algorithm>
#include <cmath>
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

ButtonStateStyleOverride buttonStateStyleOverrideFromStyleBox(const StyleBox& input) {
    const auto box = resolveBackgroundOpacity(input);
    ButtonStateStyleOverride override;
    override.opacity = box.opacity;
    override.padding = box.padding;
    override.iconSize = box.iconSize;
    override.iconGap = box.gap;
    override.background = box.background.color;
    override.gradient = box.background.gradient;
    override.foreground = box.foreground;
    override.border = box.borderColor;
    override.borderWidth = box.borderWidth;
    override.radius = box.radius;
    override.fontSize = box.fontSize;
    override.fontWeight = box.fontWeight;
    if (box.shadowsSpecified || !box.shadows.empty()) {
        override.shadows = controlShadowsFromStyleBox(box);
    }
    if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
        override.focusRing = focusRingOverrideFromStyleBox(box);
    }
    override.transition = transitionOverrideFromStyleBox(box);
    return override;
}

TextFieldStateStyleOverride textFieldStateStyleOverrideFromStyleBox(const StyleBox& input) {
    const auto box = resolveBackgroundOpacity(input);
    TextFieldStateStyleOverride override;
    override.background = box.background.color;
    override.gradient = box.background.gradient;
    override.foreground = box.foreground;
    override.placeholderForeground = box.placeholderColor;
    override.border = box.borderColor;
    override.selectionBackground = box.selectionColor;
    override.caretColor = box.caretColor;
    override.borderWidth = box.borderWidth;
    override.radius = box.radius;
    override.padding = box.padding;
    if (box.shadowsSpecified || !box.shadows.empty()) {
        override.shadows = controlShadowsFromStyleBox(box);
    }
    if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
        override.focusRing = focusRingOverrideFromStyleBox(box);
    }
    override.transition = transitionOverrideFromStyleBox(box);
    return override;
}

SwitchStateStyleOverride switchStateStyleOverrideFromStyleBox(const StyleBox& input) {
    const auto box = resolveBackgroundOpacity(input);
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

SelectStateStyleOverride selectStateStyleOverrideFromStyleBox(const StyleBox& input) {
    const auto box = resolveBackgroundOpacity(input);
    SelectStateStyleOverride override;
    override.fontSize = box.fontSize;
    override.background = box.background.color;
    override.foreground = box.foreground;
    override.border = box.borderColor;
    override.arrowColor = box.foreground;
    override.popupBackground = box.content.backgroundColor ? box.content.backgroundColor : box.background.color;
    override.popupBorder = box.borderColor;
    override.optionBackground = box.background.color;
    override.optionForeground = box.foreground;
    override.selectedOptionBackground = box.content.backgroundColor ? box.content.backgroundColor : box.background.color;
    override.selectedOptionForeground = box.foreground;
    override.borderWidth = box.borderWidth;
    override.radius = box.radius;
    override.popupRadius = box.content.radius ? box.content.radius : box.radius;
    override.optionRadius = box.content.radius ? box.content.radius : box.radius;
    override.padding = box.padding;
    if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
        override.focusRing = focusRingOverrideFromStyleBox(box);
    }
    return override;
}

ListStateStyleOverride listStateStyleOverrideFromStyleBox(const StyleBox& input) {
    const auto box = resolveBackgroundOpacity(input);
    ListStateStyleOverride override;
    override.background = box.background.color;
    override.border = box.borderColor;
    override.separator = box.borderColor;
    override.rowBackground = box.content.backgroundColor;
    override.titleColor = box.foreground;
    override.detailColor = box.placeholderColor ? box.placeholderColor : box.foreground;
    override.selectedRowBackground = box.content.backgroundColor;
    override.selectedTitleColor = box.foreground;
    override.selectedDetailColor = box.placeholderColor ? box.placeholderColor : box.foreground;
    override.borderWidth = box.borderWidth;
    override.radius = box.radius;
    override.rowRadius = box.content.radius;
    override.rowInset = box.padding;
    override.titleFontSize = box.fontSize;
    override.detailFontSize = box.detailFontSize;
    override.titleFontWeight = box.fontWeight;
    override.detailFontWeight = box.detailFontWeight;
    override.textInset = box.textInset;
    override.titleOffsetY = box.titleOffsetY;
    override.detailOffsetY = box.detailOffsetY;
    override.scrollbarColor = box.scrollbarColor;
    override.scrollbarWidth = box.scrollbarWidth;
    if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
        override.focusRing = focusRingOverrideFromStyleBox(box);
    }
    return override;
}

InteractiveSurfaceStateStyle interactiveSurfaceStateStyleFromStyleBox(const StyleBox& input) {
    const auto box = resolveBackgroundOpacity(input);
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

CheckboxStyleOverride checkboxStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    const auto resolve = [&](StylePseudoMask state) {
        const auto box = resolveBackgroundOpacity(resolveState(sheet, node, state));
        CheckboxStateStyleOverride result;
        result.boxBackground = box.background.color;
        result.boxBorder = box.borderColor;
        result.checkColor = box.content.backgroundColor;
        result.labelColor = box.foreground;
        result.borderWidth = box.borderWidth;
        result.radius = box.radius;
        if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius)
            result.focusRing = focusRingOverrideFromStyleBox(box);
        return result;
    };
    CheckboxStyleOverride result;
    result.normal = resolve(StyleStateNone);
    result.hovered = resolve(StyleStateHover);
    result.pressed = resolve(StyleStateActive);
    result.disabled = resolve(StyleStateDisabled);
    result.selected = resolve(StyleStateSelected);
    result.focusVisible = resolve(StyleStateFocus);
    return result;
}

SelectStyleOverride selectStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    const auto transient = sheet.transientStyleSheet();
    const auto resolve = [&](unsigned state) {
        auto result = selectStateStyleOverrideFromStyleBox(resolveState(sheet, node, state));
        if (transient) {
            const auto menu = selectStateStyleOverrideFromStyleBox(resolveState(*transient, node, state));
            result.popupBackground = menu.popupBackground;
            result.popupBorder = menu.popupBorder;
            result.popupShadow = menu.popupShadow;
            result.optionBackground = menu.optionBackground;
            result.optionForeground = menu.optionForeground;
            result.selectedOptionBackground = menu.selectedOptionBackground;
            result.selectedOptionForeground = menu.selectedOptionForeground;
        }
        return result;
    };
    SelectStyleOverride override;
    override.normal = resolve(StyleStateNone);
    // The field's resting background must not fill every popup option; only
    // hover/press/selected states paint option rows.
    override.normal->optionBackground.reset();
    override.hovered = resolve(StyleStateHover);
    override.pressed = resolve(StyleStateActive);
    override.disabled = resolve(StyleStateDisabled);
    override.selected = resolve(StyleStateSelected);
    override.focusVisible = resolve(StyleStateFocus);
    return override;
}

PopupStyleOverride popupStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    const StyleBox box = sheet.resolve(std::move(node));
    PopupStyleOverride style;
    style.background = box.background.color;
    style.foreground = box.foreground;
    style.border = box.borderColor;
    style.borderWidth = box.borderWidth;
    style.radius = box.radius;
    style.padding = box.padding;
    style.offset = box.gap;

    const auto shadow = std::find_if(
        box.shadows.begin(),
        box.shadows.end(),
        [](const StyleShadow& candidate) { return !candidate.inset; });
    if (shadow != box.shadows.end()) {
        style.elevation = shadow->color.a == 0
            ? 0.0f
            : std::max(0.0f, shadow->blurRadius / 10.0f);
    }
    return style;
}

ProgressBarStyleOverride progressBarStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    const StyleBox normal = resolveState(sheet, node, StyleStateNone);
    const StyleBox disabled = resolveState(sheet, std::move(node), StyleStateDisabled);

    ProgressBarStyleOverride override;
    override.trackBackground = normal.background.color;
    override.fill = normal.foreground;
    override.disabledFill = disabled.foreground;
    override.radius = normal.radius;
    return override;
}

namespace {

Color mixColor(Color a, Color b, float t) {
    auto mix = [t](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t));
    };
    return Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

Color alphaColor(Color color, std::uint8_t a) {
    color.a = a;
    return color;
}

// A custom property as a color, following var() references.
std::optional<Color> customColor(const StyleSheet& sheet, const char* name) {
    auto value = sheet.customProperty(name);
    if (!value) return std::nullopt;
    for (int depth = 0; depth < 8 && value->find("var(") != std::string::npos; ++depth)
        *value = style_detail::resolveCustomProperties(*value, sheet.customProperties(), nullptr);
    return parseStyleColor(*value);
}

} // namespace

CodeEditorStyle codeEditorStyleFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    auto s = codeEditorStyleFromTheme(theme());
    const StyleBox box = resolveState(sheet, std::move(node), StyleStateNone);
    // Surface and ink first: everything not named explicitly derives from them,
    // so a dark rule alone already gives a readable dark editor.
    if (box.background.color || box.foreground) {
        const Color bg = box.background.color.value_or(s.background);
        const Color fg = box.foreground.value_or(s.foreground);
        s.background = bg;
        s.foreground = fg;
        s.caret = fg;
        s.gutterBackground = bg;
        s.gutterForeground = mixColor(fg, bg, 0.55f);
        s.gutterActiveForeground = mixColor(fg, bg, 0.2f);
        s.gutterBorder = mixColor(bg, fg, 0.12f);
        s.currentLine = mixColor(bg, fg, 0.05f);
        s.inactiveSelection = alphaColor(mixColor(bg, fg, 0.35f), 90);
        s.indentGuide = mixColor(bg, fg, 0.12f);
        s.panelBackground = mixColor(bg, fg, 0.04f);
        s.panelForeground = fg;
        s.panelMuted = mixColor(fg, bg, 0.4f);
        s.panelBorder = mixColor(bg, fg, 0.18f);
        s.scrollbar = alphaColor(mixColor(bg, fg, 0.35f), 170);
        s.tokens[static_cast<std::size_t>(CodeTokenKind::Plain)] = fg;
        s.tokens[static_cast<std::size_t>(CodeTokenKind::Identifier)] = fg;
        s.tokens[static_cast<std::size_t>(CodeTokenKind::Comment)] = mixColor(fg, bg, 0.45f);
        s.tokens[static_cast<std::size_t>(CodeTokenKind::Operator)] = mixColor(fg, bg, 0.25f);
    }
    if (box.placeholderColor) s.gutterForeground = *box.placeholderColor;
    if (box.caretColor) s.caret = *box.caretColor;
    if (box.selectionColor) s.selection = *box.selectionColor;
    if (box.content.backgroundColor) s.currentLine = *box.content.backgroundColor;
    if (box.borderColor) s.gutterBorder = s.panelBorder = *box.borderColor;
    if (box.scrollbarColor) s.scrollbar = *box.scrollbarColor;
    const std::pair<const char*, Color*> colors[] = {
        {"--code-gutter-background", &s.gutterBackground},
        {"--code-line-number", &s.gutterForeground},
        {"--code-line-number-active", &s.gutterActiveForeground},
        {"--code-current-line", &s.currentLine},
        {"--code-indent-guide", &s.indentGuide},
        {"--code-accent", &s.accent},
        {"--code-accent-soft", &s.accentSoft},
        {"--code-match", &s.match},
        {"--code-match-current", &s.currentMatch},
        {"--code-error", &s.error},
        {"--code-warning", &s.warning},
        {"--code-success", &s.success},
        {"--code-panel", &s.panelBackground},
        {"--code-panel-border", &s.panelBorder},
        {"--code-shadow", &s.shadow},
    };
    for (const auto& [name, target] : colors)
        if (const auto color = customColor(sheet, name)) *target = *color;
    const std::pair<const char*, CodeTokenKind> tokens[] = {
        {"--code-keyword", CodeTokenKind::Keyword},   {"--code-type", CodeTokenKind::Type},
        {"--code-function", CodeTokenKind::Function}, {"--code-constant", CodeTokenKind::Constant},
        {"--code-number", CodeTokenKind::Number},     {"--code-string", CodeTokenKind::String},
        {"--code-variable", CodeTokenKind::Identifier}, {"--code-comment", CodeTokenKind::Comment},
        {"--code-operator", CodeTokenKind::Operator}, {"--code-parameter", CodeTokenKind::Parameter},
    };
    for (const auto& [name, kind] : tokens)
        if (const auto color = customColor(sheet, name)) s.tokens[static_cast<std::size_t>(kind)] = *color;
    return s;
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

TableStyleOverride tableStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node) {
    const StyleBox normal = resolveState(sheet, node, StyleStateNone);
    const StyleBox hovered = resolveState(sheet, node, StyleStateHover);
    const StyleBox pressed = resolveState(sheet, node, StyleStateActive);
    const StyleBox selected = resolveState(sheet, std::move(node), StyleStateSelected);

    TableStyleOverride style;
    style.checkBackground = normal.checkBackground;
    style.checkBorder = normal.checkBorder;
    style.checkRadius = normal.checkRadius;
    style.background = normal.background.color;
    style.border = normal.borderColor;
    style.headerBackground = normal.content.backgroundColor;
    style.headerForeground = normal.placeholderColor;
    style.cellForeground = normal.foreground;
    style.gridLine = normal.borderColor;
    style.rowHovered = hovered.background.color;
    style.rowPressed = pressed.background.color;
    style.rowSelected = selected.background.color;
    style.scrollbarColor = normal.scrollbarColor;
    style.borderWidth = normal.borderWidth;
    style.radius = normal.radius;
    style.scrollbarWidth = normal.scrollbarWidth;
    style.cellPadding = normal.padding;
    style.headerTextInset = normal.textInset;
    style.cellIconGap = normal.gap;
    style.fontSize = normal.fontSize;
    style.headerFontSize = normal.detailFontSize;
    style.detailFontSize = normal.detailFontSize;
    style.detailForeground = normal.placeholderColor;
    return style;
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
    style.disabled = interactiveSurfaceStateStyleFromStyleBox(resolveState(sheet, node, StyleStateDisabled));
    style.focusVisible = interactiveSurfaceStateStyleFromStyleBox(resolveState(sheet, std::move(node), StyleStateFocus));
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
