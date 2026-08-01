//! Raw bindings for OneUI's portable UTF-8 C ABI.
//!
//! All UTF-8 strings are borrowed for the duration of the FFI call. Callback
//! bytes are also borrowed and must be copied before the callback returns.

use std::ffi::{c_char, c_int, c_uint, c_ushort, c_void};

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

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiTreeItemUtf8 {
    pub id: OneUiUtf8String,
    pub parent_id: OneUiUtf8String,
    pub title: OneUiUtf8String,
    pub detail: OneUiUtf8String,
    pub expanded: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiTerminalCellUtf8 {
    pub text: OneUiUtf8String,
    pub foreground: OneUiColor,
    pub background: OneUiColor,
    pub style: c_uint,
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
pub type OneUiTreeExpansionCallback = Option<
    unsafe extern "C" fn(
        id: *const c_char,
        length: usize,
        expanded: c_int,
        user_data: *mut c_void,
    ),
>;
pub type OneUiVoidCallback = Option<unsafe extern "C" fn(user_data: *mut c_void)>;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiRawKeyEvent {
    pub virtual_key: c_uint,
    pub scan_code: c_uint,
    pub pressed: c_int,
    pub repeat: c_int,
    pub extended: c_int,
    pub alt: c_int,
    pub ctrl: c_int,
    pub shift: c_int,
    pub win: c_int,
}

pub type OneUiRawKeyCallback =
    Option<unsafe extern "C" fn(event: *const OneUiRawKeyEvent, user_data: *mut c_void)>;
pub type OneUiTerminalViewportCallback =
    Option<unsafe extern "C" fn(rows: c_ushort, columns: c_ushort, user_data: *mut c_void)>;

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
    pub fn oneui_widget_set_preferred_size(widget: *mut OneUiWidget, width: f32, height: f32);
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
    pub fn oneui_terminal_view_create() -> *mut OneUiWidget;
    pub fn oneui_terminal_view_set_font_size(view: *mut OneUiWidget, size: f32);
    pub fn oneui_terminal_view_set_palette(
        view: *mut OneUiWidget,
        background: OneUiColor,
        foreground: OneUiColor,
        cursor: OneUiColor,
    );
    pub fn oneui_terminal_view_set_grid_utf8(
        view: *mut OneUiWidget,
        rows: c_ushort,
        columns: c_ushort,
        cells: *const OneUiTerminalCellUtf8,
        cell_count: usize,
    );
    pub fn oneui_terminal_view_set_cursor(
        view: *mut OneUiWidget,
        row: c_ushort,
        column: c_ushort,
        visible: c_int,
    );
    pub fn oneui_terminal_view_set_on_text_input_utf8(
        view: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_raw_key(
        view: *mut OneUiWidget,
        callback: OneUiRawKeyCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_viewport_changed(
        view: *mut OneUiWidget,
        callback: OneUiTerminalViewportCallback,
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
    pub fn oneui_list_set_on_changed(
        list: *mut OneUiWidget,
        callback: Option<unsafe extern "C" fn(value: c_int, user_data: *mut c_void)>,
        user_data: *mut c_void,
    );
    pub fn oneui_tree_view_create() -> *mut OneUiWidget;
    pub fn oneui_tree_view_set_items_utf8(
        tree_view: *mut OneUiWidget,
        items: *const OneUiTreeItemUtf8,
        count: usize,
    );
    pub fn oneui_tree_view_set_selected_id_utf8(tree_view: *mut OneUiWidget, id: OneUiUtf8String);
    pub fn oneui_tree_view_content_height(tree_view: *mut OneUiWidget) -> f32;
    pub fn oneui_tree_view_selected_id_utf8(
        tree_view: *mut OneUiWidget,
        buffer: *mut c_char,
        buffer_len: usize,
    ) -> usize;
    pub fn oneui_tree_view_set_on_selection_changed_utf8(
        tree_view: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_tree_view_set_on_expansion_changed_utf8(
        tree_view: *mut OneUiWidget,
        callback: OneUiTreeExpansionCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_clipboard_set_text_utf8(text: OneUiUtf8String) -> c_int;
    pub fn oneui_clipboard_get_text_utf8(buffer: *mut c_char, buffer_len: usize) -> usize;
}
