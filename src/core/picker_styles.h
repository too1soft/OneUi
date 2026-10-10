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
    return colors.resolve({"picker",{},0});
}
inline void applyPickerButton(Button& button,const StyleSheet& sheet) {
    button.setStyleOverride(buttonStyleOverrideFromStyleSheet(sheet,{"button",{"one-Button"},0}));
}
} // namespace oneui
