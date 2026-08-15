#pragma once

#include "oneui/export.h"
#include "oneui/controls/interactive_surface.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/layout/product_shell.h"
#include "oneui/style.h"
#include "oneui/style_sheet.h"

namespace oneui {

ONEUI_API ButtonStateStyleOverride buttonStateStyleOverrideFromStyleBox(const StyleBox& box);
ONEUI_API TextFieldStateStyleOverride textFieldStateStyleOverrideFromStyleBox(const StyleBox& box);
ONEUI_API SwitchStateStyleOverride switchStateStyleOverrideFromStyleBox(const StyleBox& box);
ONEUI_API ListStateStyleOverride listStateStyleOverrideFromStyleBox(const StyleBox& box);
ONEUI_API InteractiveSurfaceStateStyle interactiveSurfaceStateStyleFromStyleBox(const StyleBox& box);

ONEUI_API StyleBox buttonStyleBoxFromStyleSheet(const StyleSheet& sheet, StyleNode node, StylePseudoMask state);
ONEUI_API StyleBox textFieldStyleBoxFromStyleSheet(const StyleSheet& sheet, StyleNode node, StylePseudoMask state);
ONEUI_API ButtonStyleOverride buttonStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node);
ONEUI_API TextFieldStyleOverride textFieldStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node);
ONEUI_API SwitchStyleOverride switchStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node);
ONEUI_API SelectStyleOverride selectStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node);
ONEUI_API ProgressBarStyleOverride progressBarStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node);
ONEUI_API PopupStyleOverride popupStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node);
ONEUI_API ListStyleOverride listStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node);
ONEUI_API TreeViewStyleOverride treeViewStyleOverrideFromStyleSheet(const StyleSheet& sheet, StyleNode node);
ONEUI_API InteractiveSurfaceStyle interactiveSurfaceStyleFromStyleSheet(const StyleSheet& sheet, StyleNode node);
ONEUI_API StyleBox cardStyleBoxFromStyleSheet(const StyleSheet& sheet, StyleNode node);
ONEUI_API ProductShellStyle productShellStyleFromStyleSheet(const StyleSheet& sheet);

} // namespace oneui
