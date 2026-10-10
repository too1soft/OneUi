#pragma once
#include "oneui/controls/checkbox.h"
#include "oneui/controls/list.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/controls/radio_group.h"
#include "oneui/controls/select.h"
#include "oneui/controls/slider.h"
#include "oneui/controls/switch.h"
#include "oneui/controls/table.h"
#include "oneui/controls/tabs.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/time_series_chart.h"
#include "oneui/controls/tree_view.h"
#include "oneui/controls/virtual_list.h"
#include "oneui/layout/split_view.h"
#include "oneui/style_adapter.h"
namespace oneui::ui {
struct NativePalette {
    Color surface{255, 255, 255}, ink{28, 43, 35}, muted{84, 103, 91}, line{186, 200, 191},
        selection{213, 233, 215}, accent{40, 97, 61}, hover{213, 226, 215};
};
inline NativePalette nativePalette(const StyleSheet &sheet) {
    // Resolve through the parser, including custom token indirection. No second color parser.
    if (!sheet.customProperty("--surface"))
        return {};
    StyleSheet tokens;
    for (const auto &item : sheet.customProperties())
        tokens.setCustomProperty(item.first, item.second);
    std::string error;
    // A partial palette is valid. One absent token must not discard all other
    // resolved colors and silently turn native controls back to light defaults.
    std::string css = "native {";
    for (const auto &entry : {std::pair{"--surface", "background-color"},
                             {"--ink", "color"}, {"--muted", "placeholder-color"},
                             {"--line", "border-color"}, {"--selection", "selection-color"},
                             {"--accent", "caret-color"}, {"--hover", "content-background-color"}}) {
        if (sheet.customProperty(entry.first))
            css += std::string(entry.second) + ":var(" + entry.first + ");";
    }
    if (!tokens.addRulesFromCss(css + "}", &error))
        return {};
    const auto box = tokens.resolve({"native", {}, 0});
    NativePalette p;
    p.surface = box.background.color.value_or(p.surface);
    p.ink = box.foreground.value_or(p.ink);
    p.muted = box.placeholderColor.value_or(p.muted);
    p.line = box.borderColor.value_or(p.line);
    p.selection = box.selectionColor.value_or(p.selection);
    p.accent = box.caretColor.value_or(p.accent);
    p.hover = box.content.backgroundColor.value_or(p.hover);
    return p;
}
inline void applyNativeTheme(Widget &widget, const StyleSheet &sheet, bool animate = true) {
    if (auto field = dynamic_cast<TextField *>(&widget)) {
        auto style = textFieldStyleOverrideFromStyleSheet(sheet, {"input", {"one-Input"}, 0});
        if (!animate) {
            // A newly wrapped widget already has a forwarding scheduler, but no
            // window can tick it yet. Install initial colors without a transition.
            auto initial = style;
            for (auto *state : {&initial.normal, &initial.hovered, &initial.disabled,
                                &initial.readOnly, &initial.focusVisible})
                if (*state)
                    (*state)->transition = TransitionSpec{0};
            field->setStyleOverride(std::move(initial));
        }
        field->setStyleOverride(std::move(style));
        return;
    }
    if (auto table = dynamic_cast<Table *>(&widget)) {
        table->setStyleOverride(
            tableStyleOverrideFromStyleSheet(sheet, {"table", {"one-DataTable"}, 0}));
        return;
    }
    const auto p = nativePalette(sheet);
    FocusRingStyleOverride focus;
    focus.color = p.accent;
    if (auto select = dynamic_cast<Select *>(&widget)) {
        SelectStateStyleOverride n;
        n.background = p.surface;
        n.foreground = p.ink;
        n.border = p.line;
        n.arrowColor = p.muted;
        n.popupBackground = p.surface;
        n.popupBorder = p.line;
        n.optionForeground = p.ink;
        n.selectedOptionBackground = p.selection;
        n.selectedOptionForeground = p.ink;
        n.focusRing = focus;
        SelectStyleOverride style;
        style.normal = n;
        auto hover = n;
        hover.optionBackground = p.hover;
        style.hovered = hover;
        auto disabled = n;
        disabled.foreground = p.muted;
        style.disabled = disabled;
        select->setStyleOverride(style);
        return;
    }
    if (auto slider = dynamic_cast<Slider *>(&widget)) {
        SliderStateStyleOverride n;
        n.trackBackground = p.line;
        n.trackFill = p.accent;
        n.thumbBackground = p.accent;
        n.thumbBorder = p.accent;
        n.focusRing = focus;
        SliderStyleOverride style;
        style.normal = n;
        auto disabled = n;
        disabled.trackFill = p.muted;
        disabled.thumbBackground = p.muted;
        style.disabled = disabled;
        slider->setStyleOverride(style);
        return;
    }
    if (auto check = dynamic_cast<Checkbox *>(&widget)) {
        CheckboxStateStyleOverride n;
        n.boxBackground = p.surface;
        n.boxBorder = p.line;
        n.labelColor = p.ink;
        n.checkColor = p.surface;
        n.focusRing = focus;
        CheckboxStyleOverride style;
        style.normal = n;
        auto selected = n;
        selected.boxBackground = p.accent;
        selected.boxBorder = p.accent;
        style.selected = selected;
        auto disabled = n;
        disabled.labelColor = p.muted;
        style.disabled = disabled;
        check->setStyleOverride(style);
        return;
    }
    if (auto radio = dynamic_cast<RadioGroup *>(&widget)) {
        RadioGroupStateStyleOverride n;
        n.indicatorBackground = p.surface;
        n.indicatorBorder = p.line;
        n.indicatorFill = p.accent;
        n.labelColor = p.ink;
        n.selectedLabelColor = p.ink;
        n.focusRing = focus;
        RadioGroupStyleOverride style;
        style.normal = n;
        auto selected = n;
        selected.indicatorBorder = p.accent;
        style.selected = selected;
        auto hovered = n;
        hovered.itemBackground = p.hover;
        style.hovered = hovered;
        radio->setStyleOverride(style);
        return;
    }
    if (auto toggle = dynamic_cast<Switch *>(&widget)) {
        SwitchStateStyleOverride n;
        n.trackBackground = p.line;
        n.thumbBackground = p.surface;
        n.labelColor = p.ink;
        n.focusRing = focus;
        SwitchStyleOverride style;
        style.normal = n;
        auto selected = n;
        selected.trackBackground = p.accent;
        style.selected = selected;
        toggle->setStyleOverride(style);
        return;
    }
    if (auto tree = dynamic_cast<TreeView *>(&widget)) {
        tree->setStyleOverride(
            treeViewStyleOverrideFromStyleSheet(sheet, {"tree", {"one-Tree"}, 0}));
        return;
    }
    if (auto list = dynamic_cast<List *>(&widget)) {
        list->setStyleOverride(listStyleOverrideFromStyleSheet(sheet, {"list", {"one-List"}, 0}));
        return;
    }
    if (auto progress = dynamic_cast<ProgressBar *>(&widget)) {
        ProgressBarStyleOverride style;
        style.trackBackground = p.line;
        style.fill = p.accent;
        style.radius = 2;
        progress->setStyleOverride(style);
        return;
    }
    if (auto tabs = dynamic_cast<Tabs *>(&widget)) {
        TabsStateStyleOverride n;
        n.background = p.surface;
        n.border = p.line;
        n.borderWidth = 0;
        n.radius = 0;
        n.itemRadius = 5;
        n.itemBackground = Color{0, 0, 0, 0};
        n.itemForeground = p.muted;
        n.selectedItemBackground = p.selection;
        n.selectedItemForeground = p.ink;
        n.selectedItemBorder = p.accent;
        FocusRingStyleOverride focus;
        focus.color = p.accent;
        focus.width = 1;
        n.focusRing = focus;
        n.fontSize = 13;
        n.fontWeight = 500;
        TabsStyleOverride style;
        style.normal = n;
        auto hover = n;
        hover.itemBackground = p.hover;
        style.hovered = hover;
        style.pressed = hover;
        tabs->setStyleOverride(style);
    } else if (auto list = dynamic_cast<VirtualList *>(&widget)) {
        ListStateStyleOverride n;
        n.background = p.surface;
        n.borderWidth = 0;
        n.radius = 0;
        n.rowRadius = 6;
        n.separator = Color{0, 0, 0, 0};
        n.titleColor = p.ink;
        n.detailColor = p.muted;
        n.selectedRowBackground = p.selection;
        n.selectedTitleColor = p.ink;
        n.selectedDetailColor = p.muted;
        n.rowInset = Insets{3, 6};
        n.titleFontSize = 13;
        n.detailFontSize = 11;
        n.titleFontWeight = 500;
        n.titleOffsetY = 10;
        n.detailOffsetY = 30;
        n.scrollbarColor = p.line;
        n.scrollbarWidth = 3;
        ListStyleOverride style;
        style.normal = n;
        auto hover = n;
        hover.rowBackground = p.hover;
        style.hovered = hover;
        list->setStyleOverride(style);
    } else if (auto chart = dynamic_cast<TimeSeriesChart *>(&widget)) {
        StyleBox box;
        box.borderColor = Color{p.line.r, p.line.g, p.line.b, 90};
        box.foreground = p.muted;
        box.fontSize = 10;
        chart->setStyleBox(box);
    } else if (auto split = dynamic_cast<SplitView *>(&widget))
        split->setDividerColors(p.line, p.accent);
}
} // namespace oneui::ui
