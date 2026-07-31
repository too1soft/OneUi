//! Raw bindings for OneUI's portable UTF-8 C ABI.
//!
//! All UTF-8 strings are borrowed for the duration of the FFI call. Callback
//! bytes are also borrowed and must be copied before the callback returns.

use std::ffi::{c_char, c_int, c_uint, c_void};

pub const UTF8_ABI_VERSION: c_uint = 1;

#[repr(C)]
pub struct OneUiWindow {
    _private: [u8; 0],
}

#[repr(C)]
pub struct OneUiWidget {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiUtf8String {
    pub data: *const c_char,
    pub length: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiListItemUtf8 {
    pub title: OneUiUtf8String,
    pub detail: OneUiUtf8String,
}

impl OneUiUtf8String {
    pub fn from_str(value: &str) -> Self {
        Self {
            data: value.as_ptr().cast(),
            length: value.len(),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiWindowOptionsUtf8 {
    pub title: OneUiUtf8String,
    pub width: c_int,
    pub height: c_int,
    pub visible: c_int,
    pub borderless: c_int,
    pub fullscreen: c_int,
    pub topmost: c_int,
    pub resizable: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiInsets {
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub left: f32,
}

pub type OneUiUtf8TextCallback =
    Option<unsafe extern "C" fn(text: *const c_char, length: usize, user_data: *mut c_void)>;
pub type OneUiVoidCallback = Option<unsafe extern "C" fn(user_data: *mut c_void)>;

extern "C" {
    pub fn oneui_utf8_abi_version() -> c_uint;

    pub fn oneui_window_create_utf8(options: *const OneUiWindowOptionsUtf8) -> *mut OneUiWindow;
    pub fn oneui_window_destroy(window: *mut OneUiWindow);
    pub fn oneui_window_initialize(window: *mut OneUiWindow);
    pub fn oneui_window_show(window: *mut OneUiWindow);
    pub fn oneui_window_activate(window: *mut OneUiWindow);
    pub fn oneui_window_run(window: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_close(window: *mut OneUiWindow);
    pub fn oneui_window_request_close(window: *mut OneUiWindow);
    pub fn oneui_window_set_title_utf8(window: *mut OneUiWindow, title: OneUiUtf8String);
    pub fn oneui_window_set_content(window: *mut OneUiWindow, widget: *mut OneUiWidget);
    pub fn oneui_window_post_owned(
        window: *mut OneUiWindow,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
        cleanup: OneUiVoidCallback,
    ) -> c_int;

    pub fn oneui_widget_destroy(widget: *mut OneUiWidget);
    // 0 = column, 1 = row. These values are part of the stable C ABI.
    pub fn oneui_stack_create(direction: c_int) -> *mut OneUiWidget;
    pub fn oneui_stack_add(stack: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_stack_set_gap(stack: *mut OneUiWidget, gap: f32);
    pub fn oneui_stack_set_padding(stack: *mut OneUiWidget, insets: OneUiInsets);
    pub fn oneui_scroll_view_create() -> *mut OneUiWidget;
    pub fn oneui_scroll_view_set_content(view: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_scroll_view_set_content_height(view: *mut OneUiWidget, height: f32);
    pub fn oneui_scroll_view_set_wheel_step(view: *mut OneUiWidget, step: f32);
    pub fn oneui_scroll_view_scroll_to_bottom(view: *mut OneUiWidget);
    pub fn oneui_label_create_utf8(text: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_label_set_text_utf8(label: *mut OneUiWidget, text: OneUiUtf8String);
    pub fn oneui_label_set_font_size(label: *mut OneUiWidget, font_size: f32);
    pub fn oneui_button_create_utf8(text: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_button_set_text_utf8(button: *mut OneUiWidget, text: OneUiUtf8String);
    pub fn oneui_text_field_create_utf8(placeholder: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_text_field_set_text_utf8(text_field: *mut OneUiWidget, text: OneUiUtf8String);
    pub fn oneui_text_field_set_read_only(text_field: *mut OneUiWidget, read_only: c_int);
    pub fn oneui_text_field_set_on_changed_utf8(
        text_field: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_search_box_create_utf8(placeholder: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_list_create() -> *mut OneUiWidget;
    pub fn oneui_list_set_items_utf8(
        list: *mut OneUiWidget,
        items: *const OneUiListItemUtf8,
        count: usize,
    );
    pub fn oneui_list_set_selected_index(list: *mut OneUiWidget, index: c_int);
    pub fn oneui_list_selected_index(list: *mut OneUiWidget) -> c_int;

    pub fn oneui_clipboard_set_text_utf8(text: OneUiUtf8String) -> c_int;
    pub fn oneui_clipboard_get_text_utf8(buffer: *mut c_char, buffer_len: usize) -> usize;
}
