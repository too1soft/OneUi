//! Foundation api for the UTF-8 C ABI.

use super::abi_types::{
    OneUiClientSizeChangedCallback, OneUiErrorCode, OneUiFocusBookmark, OneUiRect, OneUiStyleSheet,
    OneUiUtf8String, OneUiWidget, WChar,
};
use std::ffi::{c_char, c_int, c_uint, c_void};

extern "C" {

    pub fn oneui_last_error() -> *const c_char;
    pub fn oneui_last_error_code() -> OneUiErrorCode;
    pub fn oneui_clear_error();
    pub fn oneui_set_builtin_text_utf8(key: c_int, text: *const c_char) -> c_int;
    pub fn oneui_widget_capture_focus(root: *const OneUiWidget) -> *mut OneUiFocusBookmark;
    pub fn oneui_focus_bookmark_restore(
        bookmark: *const OneUiFocusBookmark,
        root: *mut OneUiWidget,
    ) -> c_int;
    pub fn oneui_focus_bookmark_destroy(bookmark: *mut OneUiFocusBookmark);

    pub fn oneui_utf8_abi_version() -> c_uint;

    pub fn oneui_widget_destroy(widget: *mut OneUiWidget);
    pub fn oneui_widget_set_on_size_changed(
        widget: *mut OneUiWidget,
        callback: OneUiClientSizeChangedCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_widget_set_preferred_size(widget: *mut OneUiWidget, width: f32, height: f32);
    pub fn oneui_widget_frame(widget: *const OneUiWidget) -> OneUiRect;
    pub fn oneui_widget_set_disabled(widget: *mut OneUiWidget, disabled: c_int);
    pub fn oneui_widget_set_tab_stop(widget: *mut OneUiWidget, tab_stop: c_int);
    pub fn oneui_widget_set_visible(widget: *mut OneUiWidget, visible: c_int);
    pub fn oneui_widget_focused(widget: *const OneUiWidget) -> c_int;
    pub fn oneui_widget_set_tooltip(widget: *mut OneUiWidget, tooltip: *const WChar);
    pub fn oneui_widget_set_accessible_name(widget: *mut OneUiWidget, name: *const WChar);
    pub fn oneui_widget_set_accessible_description(
        widget: *mut OneUiWidget,
        description: *const WChar,
    );
    pub fn oneui_widget_set_classes(widget: *mut OneUiWidget, classes: *const c_char);
    pub fn oneui_widget_set_style_node(
        widget: *mut OneUiWidget,
        tag: *const c_char,
        classes: *const c_char,
    );
    pub fn oneui_widget_apply_style_sheet(
        widget: *mut OneUiWidget,
        style_sheet: *mut OneUiStyleSheet,
    );

    pub fn oneui_style_sheet_create() -> *mut OneUiStyleSheet;
    pub fn oneui_style_sheet_destroy(style_sheet: *mut OneUiStyleSheet);
    pub fn oneui_style_sheet_set_transient_style_sheet(
        style_sheet: *mut OneUiStyleSheet,
        transient: *mut OneUiStyleSheet,
    );
    pub fn oneui_style_sheet_set_custom_property(
        style_sheet: *mut OneUiStyleSheet,
        name: *const c_char,
        value: *const c_char,
    );
    pub fn oneui_style_sheet_add_css(
        style_sheet: *mut OneUiStyleSheet,
        css: *const c_char,
        error_buffer: *mut c_char,
        error_buffer_len: c_int,
    ) -> c_int;
    pub fn oneui_style_sheet_replace_css(
        style_sheet: *mut OneUiStyleSheet,
        css: *const c_char,
        error_buffer: *mut c_char,
        error_buffer_len: c_int,
    ) -> c_int;
    pub fn oneui_widget_measure(
        widget: *mut OneUiWidget,
        max_width: f32,
        max_height: f32,
        width: *mut f32,
        height: *mut f32,
    ) -> c_int;
    pub fn oneui_widget_natural_size(
        widget: *mut OneUiWidget,
        width: *mut f32,
        height: *mut f32,
    ) -> c_int;

    pub fn oneui_clipboard_set_text_utf8(text: OneUiUtf8String) -> c_int;
    pub fn oneui_clipboard_get_text_utf8(buffer: *mut c_char, buffer_len: usize) -> usize;

}
