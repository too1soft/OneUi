//! Existing portable C ABI slots for application and toolbar composition.
use crate::{OneUiInsets, OneUiWidget};
use std::ffi::{c_float, c_int};

extern "C" {
    pub fn oneui_app_shell_create() -> *mut OneUiWidget;
    pub fn oneui_app_shell_set_sidebar(shell: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_app_shell_set_header(shell: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_app_shell_set_content(shell: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_app_shell_set_footer(shell: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_app_shell_set_sidebar_width(shell: *mut OneUiWidget, width: c_float);
    pub fn oneui_app_shell_set_header_height(shell: *mut OneUiWidget, height: c_float);
    pub fn oneui_app_shell_set_footer_height(shell: *mut OneUiWidget, height: c_float);
    pub fn oneui_app_shell_set_footer_span_sidebar(shell: *mut OneUiWidget, span: c_int);
    pub fn oneui_app_shell_set_gap(shell: *mut OneUiWidget, gap: c_float);
    pub fn oneui_app_shell_set_padding(shell: *mut OneUiWidget, insets: OneUiInsets);
    pub fn oneui_app_shell_set_sidebar_visible(shell: *mut OneUiWidget, visible: c_int);
    pub fn oneui_top_bar_create() -> *mut OneUiWidget;
    pub fn oneui_top_bar_set_leading(top_bar: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_top_bar_add_action(top_bar: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_top_bar_clear_actions(top_bar: *mut OneUiWidget);
    pub fn oneui_top_bar_set_gap(top_bar: *mut OneUiWidget, gap: c_float);
    pub fn oneui_top_bar_set_padding(top_bar: *mut OneUiWidget, insets: OneUiInsets);
    pub fn oneui_top_bar_set_leading_width(top_bar: *mut OneUiWidget, width: c_float);
}
