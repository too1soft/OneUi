#pragma once
#include "oneui/style_adapter.h"
#include "oneui/controls/button.h"
namespace oneui {
inline StyleBox pickerColors(const StyleSheet& sheet) {
    if(!sheet.customProperty("--surface"))return {};
    StyleSheet colors;
    for(auto& property:sheet.customProperties())colors.setCustomProperty(property.first,property.second);
    // A partial application palette must not discard every supplied color just
    // because an unrelated optional token is absent.
    colors.addRulesFromCss("picker {background-color:var(--surface,#f5f7f4);color:var(--ink,#1c2b23);selection-color:var(--selection,#d6eadb);border-color:var(--line,#c6d0c8);}");
    auto box=colors.resolve({"picker",{},0});
    if(!sheet.customProperty("--selection")) {
        auto bg=box.background.color.value_or(Color{245,247,244});
        auto fg=box.foreground.value_or(Color{28,43,35});
        box.selectionColor=Color{uint8_t((3*int(bg.r)+fg.r)/4),uint8_t((3*int(bg.g)+fg.g)/4),uint8_t((3*int(bg.b)+fg.b)/4),255};
    }
    return box;
}
inline void applyPickerButton(Button& button,const StyleSheet& sheet) {
    button.setStyleOverride(buttonStyleOverrideFromStyleSheet(sheet,{"button",{"one-Button"},0}));
}
} // namespace oneui
