//! Window api for the UTF-8 C ABI.

use super::abi_types::{
    OneUiClientSizeChangedCallback, OneUiFileDialogOptionsUtf8, OneUiFileDropCallback,
    OneUiFrameCallback, OneUiIntCallback, OneUiStyleSheet, OneUiUtf8String, OneUiVoidCallback,
    OneUiWidget, OneUiWindow, OneUiWindowActivationChangedCallback, OneUiWindowOptionsUtf8,
    OneUiWindowPlacement, OneUiWindowRawKeyCallback, WChar,
};
use super::{OneUiNativeMenuItemUtf8, OneUiTrayMenuItem};
use std::ffi::{c_char, c_float, c_int, c_uint, c_void};

extern "C" {

    pub fn oneui_font_register_memory(
        data: *const u8,
        size: usize,
        family_alias: OneUiUtf8String,
    ) -> c_int;

    pub fn oneui_window_create_utf8(options: *const OneUiWindowOptionsUtf8) -> *mut OneUiWindow;
    pub fn oneui_window_destroy(window: *mut OneUiWindow);
    pub fn oneui_window_initialize(window: *mut OneUiWindow);
    pub fn oneui_window_show(window: *mut OneUiWindow);
    pub fn oneui_window_center_on_active_monitor(window: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_show_with_fade(window: *mut OneUiWindow, duration_ms: c_uint);
    pub fn oneui_window_client_area_animations_enabled(window: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_activate(window: *mut OneUiWindow);
    pub fn oneui_window_file_dialog_utf8(
        window: *mut OneUiWindow,
        options: *const OneUiFileDialogOptionsUtf8,
        buffer: *mut c_char,
        buffer_len: usize,
        required_len: *mut usize,
    ) -> c_int;
    pub fn oneui_window_file_dialog_multiple_utf8(
        window: *mut OneUiWindow,
        options: *const OneUiFileDialogOptionsUtf8,
        buffer: *mut c_char,
        buffer_len: usize,
        required_len: *mut usize,
    ) -> c_int;
    pub fn oneui_window_confirm(
        window: *mut OneUiWindow,
        title: *const WChar,
        message: *const WChar,
    ) -> c_int;
    pub fn oneui_window_prompt_text(
        window: *mut OneUiWindow,
        title: *const WChar,
        message: *const WChar,
        initial_value: *const WChar,
        placeholder: *const WChar,
        password: c_int,
        out: *mut WChar,
        out_len: c_int,
    ) -> c_int;
    pub fn oneui_window_run(window: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_close(window: *mut OneUiWindow);
    pub fn oneui_window_request_close(window: *mut OneUiWindow);
    pub fn oneui_window_minimize(window: *mut OneUiWindow);
    pub fn oneui_window_toggle_maximize(window: *mut OneUiWindow);
    pub fn oneui_window_set_fullscreen(window: *mut OneUiWindow, fullscreen: c_int);
    pub fn oneui_window_is_fullscreen(window: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_get_placement(
        window: *mut OneUiWindow,
        placement: *mut OneUiWindowPlacement,
    ) -> c_int;
    pub fn oneui_window_set_placement(
        window: *mut OneUiWindow,
        placement: *const OneUiWindowPlacement,
    ) -> c_int;
    pub fn oneui_window_set_borderless(window: *mut OneUiWindow, borderless: c_int);
    pub fn oneui_window_set_title_bar_drag_metrics(
        window: *mut OneUiWindow,
        title_bar_height: f32,
        reserved_button_width: f32,
    );
    pub fn oneui_window_set_title_bar_interactive_insets(
        window: *mut OneUiWindow,
        leading_width: f32,
        trailing_width: f32,
    );
    pub fn oneui_window_set_on_client_size_changed(
        window: *mut OneUiWindow,
        callback: OneUiClientSizeChangedCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_window_set_on_activation_changed(
        window: *mut OneUiWindow,
        callback: OneUiWindowActivationChangedCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_window_set_on_file_drop(
        window: *mut OneUiWindow,
        callback: OneUiFileDropCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_window_set_on_close_requested(
        window: *mut OneUiWindow,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_window_set_minimum_client_size(
        window: *mut OneUiWindow,
        width: c_float,
        height: c_float,
    );
    pub fn oneui_window_set_corner_radius(window: *mut OneUiWindow, radius: f32);
    pub fn oneui_window_set_close_to_tray(window: *mut OneUiWindow, close_to_tray: c_int);
    pub fn oneui_window_set_tray_enabled(window: *mut OneUiWindow, enabled: c_int) -> c_int;
    pub fn oneui_window_set_tray_menu(
        window: *mut OneUiWindow,
        items: *const OneUiTrayMenuItem,
        count: usize,
    ) -> c_int;
    pub fn oneui_window_set_tray_menu_utf8(
        window: *mut OneUiWindow,
        items: *const OneUiNativeMenuItemUtf8,
        count: usize,
    ) -> c_int;
    pub fn oneui_window_set_menu_bar_utf8(
        window: *mut OneUiWindow,
        items: *const OneUiNativeMenuItemUtf8,
        count: usize,
    ) -> c_int;
    pub fn oneui_window_set_on_menu_action(
        window: *mut OneUiWindow,
        callback: Option<unsafe extern "C" fn(c_int, *mut c_void)>,
        user_data: *mut c_void,
    );
    pub fn oneui_window_set_tray_icon_rgba(
        window: *mut OneUiWindow,
        pixels: *const u8,
        width: c_uint,
        height: c_uint,
        row_stride: usize,
        template_image: c_int,
    ) -> c_int;
    pub fn oneui_window_set_tray_tooltip(window: *mut OneUiWindow, text: *const WChar) -> c_int;
    pub fn oneui_window_set_on_tray_action(
        window: *mut OneUiWindow,
        callback: Option<unsafe extern "C" fn(c_int, *mut c_void)>,
        user_data: *mut c_void,
    );
    pub fn oneui_window_tray_icon_visible(window: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_notify_tray(
        window: *mut OneUiWindow,
        title: *const WChar,
        message: *const WChar,
    ) -> c_int;
    pub fn oneui_window_set_title_utf8(window: *mut OneUiWindow, title: OneUiUtf8String);
    pub fn oneui_window_set_content(window: *mut OneUiWindow, widget: *mut OneUiWidget);
    pub fn oneui_window_native_handle(window: *mut OneUiWindow) -> *mut c_void;
    pub fn oneui_window_layout_snapshot_utf8(
        window: *mut OneUiWindow,
        buffer: *mut c_char,
        buffer_len: usize,
        required_len: *mut usize,
    ) -> c_int;
    pub fn oneui_window_capture_frame_png_utf8(
        window: *mut OneUiWindow,
        path_utf8: *const c_char,
    ) -> c_int;
    pub fn oneui_window_export_developer_report_utf8(
        window: *mut OneUiWindow,
        path_utf8: *const c_char,
    ) -> c_int;
    pub fn oneui_window_request_focus(
        window: *mut OneUiWindow,
        widget: *mut OneUiWidget,
        focus_visible: c_int,
    ) -> c_int;
    pub fn oneui_window_set_on_raw_key(
        window: *mut OneUiWindow,
        callback: OneUiWindowRawKeyCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_window_set_style_sheet(
        window: *mut OneUiWindow,
        style_sheet: *mut OneUiStyleSheet,
    );
    pub fn oneui_window_refresh_style_sheet(window: *mut OneUiWindow);
    pub fn oneui_window_set_default_font_family_utf8(
        window: *mut OneUiWindow,
        family: OneUiUtf8String,
    );
    pub fn oneui_window_post_owned(
        window: *mut OneUiWindow,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
        cleanup: OneUiVoidCallback,
    ) -> c_int;
    pub fn oneui_window_request_animation_frame(
        window: *mut OneUiWindow,
        callback: OneUiFrameCallback,
        user_data: *mut c_void,
    );
}
