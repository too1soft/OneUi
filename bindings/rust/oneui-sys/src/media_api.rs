//! Media api for the UTF-8 C ABI.

use super::abi_types::{
    OneUiBoolCallback, OneUiColor, OneUiFrameReleaseCallback, OneUiIntCallback,
    OneUiRawKeyCallback, OneUiRect, OneUiRemotePointerCallback, OneUiTerminalCellUtf8,
    OneUiTerminalHyperlinkCallback, OneUiTerminalPointerCallback, OneUiTerminalViewportCallback,
    OneUiUtf8String, OneUiUtf8TextCallback, OneUiVideoFramePatch, OneUiVoidCallback, OneUiWidget,
    WChar,
};
use super::{
    OneUiPixelFormat, OneUiRemoteCursorMode, OneUiTerminalAuxiliaryButtonAction,
    OneUiTerminalCursorStyle, OneUiVideoScaleMode,
};
use std::ffi::{c_char, c_float, c_int, c_ushort, c_void};

extern "C" {

    pub fn oneui_log_view_create() -> *mut OneUiWidget;
    pub fn oneui_log_view_append_line(
        view: *mut OneUiWidget,
        text: *const WChar,
        r: u8,
        g: u8,
        b: u8,
        a: u8,
    );
    pub fn oneui_log_view_clear(view: *mut OneUiWidget);
    pub fn oneui_log_view_content_height(view: *mut OneUiWidget) -> f32;
    pub fn oneui_log_view_set_font_size(view: *mut OneUiWidget, size: f32);
    pub fn oneui_log_view_set_line_height(view: *mut OneUiWidget, height: f32);
    pub fn oneui_realtime_frame_view_create() -> *mut OneUiWidget;
    pub fn oneui_realtime_frame_view_set_scale_mode(
        frame_view: *mut OneUiWidget,
        scale_mode: OneUiVideoScaleMode,
    );
    pub fn oneui_realtime_frame_view_set_content_alignment(
        frame_view: *mut OneUiWidget,
        horizontal_alignment: c_int,
        vertical_alignment: c_int,
    );
    pub fn oneui_realtime_frame_view_set_background(
        frame_view: *mut OneUiWidget,
        r: u8,
        g: u8,
        b: u8,
        a: u8,
    );
    pub fn oneui_realtime_frame_view_submit_frame(
        frame_view: *mut OneUiWidget,
        pixels: *const c_void,
        width: c_int,
        height: c_int,
        stride: c_int,
        pixel_format: OneUiPixelFormat,
        frame_id: u64,
        timestamp_us: u64,
    );
    pub fn oneui_realtime_frame_view_submit_frame_owned(
        frame_view: *mut OneUiWidget,
        pixels: *const c_void,
        pixel_bytes: usize,
        width: c_int,
        height: c_int,
        stride: c_int,
        pixel_format: OneUiPixelFormat,
        frame_id: u64,
        timestamp_us: u64,
        release_callback: OneUiFrameReleaseCallback,
        user_data: *mut c_void,
    ) -> c_int;
    pub fn oneui_realtime_frame_view_submit_damage(
        frame_view: *mut OneUiWidget,
        frame_width: c_int,
        frame_height: c_int,
        pixel_format: OneUiPixelFormat,
        patches: *const OneUiVideoFramePatch,
        patch_count: usize,
        frame_id: u64,
        timestamp_us: u64,
    ) -> c_int;
    pub fn oneui_remote_input_region_create() -> *mut OneUiWidget;
    pub fn oneui_remote_input_region_set_remote_size(
        region: *mut OneUiWidget,
        width: c_float,
        height: c_float,
    );
    pub fn oneui_remote_input_region_set_scale_mode(
        region: *mut OneUiWidget,
        scale_mode: OneUiVideoScaleMode,
    );
    pub fn oneui_remote_input_region_set_cursor_mode(
        region: *mut OneUiWidget,
        cursor_mode: OneUiRemoteCursorMode,
    );
    pub fn oneui_remote_input_region_set_cursor_position(
        region: *mut OneUiWidget,
        remote_x: f32,
        remote_y: f32,
    );
    pub fn oneui_remote_input_region_set_cursor_bitmap_rgba(
        region: *mut OneUiWidget,
        pixels: *const c_void,
        pixel_bytes: usize,
        width: c_int,
        height: c_int,
        stride: c_int,
        hotspot_x: c_int,
        hotspot_y: c_int,
    ) -> c_int;
    pub fn oneui_remote_input_region_set_on_pointer(
        region: *mut OneUiWidget,
        callback: OneUiRemotePointerCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_remote_input_region_set_on_raw_key(
        region: *mut OneUiWidget,
        callback: OneUiRawKeyCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_remote_input_region_set_on_text_input_utf8(
        region: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_remote_input_region_release_all_inputs(region: *mut OneUiWidget);
    pub fn oneui_terminal_view_create() -> *mut OneUiWidget;
    pub fn oneui_terminal_view_set_font_size(view: *mut OneUiWidget, size: f32);
    pub fn oneui_terminal_view_set_font_family_utf8(
        view: *mut OneUiWidget,
        family: OneUiUtf8String,
    );
    pub fn oneui_terminal_view_set_line_height(view: *mut OneUiWidget, multiplier: f32);
    pub fn oneui_terminal_view_set_letter_spacing(view: *mut OneUiWidget, pixels: f32);
    pub fn oneui_terminal_view_set_line_numbers_visible(view: *mut OneUiWidget, visible: c_int);
    pub fn oneui_terminal_view_set_scrollback(
        view: *mut OneUiWidget,
        history_rows: u64,
        offset: u64,
    );
    pub fn oneui_terminal_view_set_first_visible_line_number(
        view: *mut OneUiWidget,
        line_number: u64,
    );
    pub fn oneui_terminal_view_set_cursor_style(
        view: *mut OneUiWidget,
        style: OneUiTerminalCursorStyle,
    );
    pub fn oneui_terminal_view_set_cursor_blinking(view: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_terminal_view_set_copy_on_select(view: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_terminal_view_set_right_button_action(
        view: *mut OneUiWidget,
        action: OneUiTerminalAuxiliaryButtonAction,
    );
    pub fn oneui_terminal_view_set_middle_button_action(
        view: *mut OneUiWidget,
        action: OneUiTerminalAuxiliaryButtonAction,
    );
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
    pub fn oneui_terminal_view_update_cells_utf8(
        view: *mut OneUiWidget,
        first_cell: usize,
        cells: *const OneUiTerminalCellUtf8,
        cell_count: usize,
    );
    pub fn oneui_terminal_view_set_cursor(
        view: *mut OneUiWidget,
        row: c_ushort,
        column: c_ushort,
        visible: c_int,
    );
    pub fn oneui_terminal_view_text_input_caret_rect(
        view: *mut OneUiWidget,
        out_rect: *mut OneUiRect,
    ) -> c_int;
    pub fn oneui_terminal_view_select_all(view: *mut OneUiWidget);
    pub fn oneui_terminal_view_copy_selection(view: *mut OneUiWidget) -> c_int;
    pub fn oneui_terminal_view_paste_clipboard(view: *mut OneUiWidget) -> c_int;
    pub fn oneui_terminal_view_paste_utf8(view: *mut OneUiWidget, text: OneUiUtf8String) -> c_int;
    pub fn oneui_terminal_view_set_selection(
        view: *mut OneUiWidget,
        start_row: c_ushort,
        start_column: c_ushort,
        end_row: c_ushort,
        end_column: c_ushort,
    );
    pub fn oneui_terminal_view_clear_selection(view: *mut OneUiWidget);
    pub fn oneui_terminal_view_has_selection(view: *mut OneUiWidget) -> c_int;
    pub fn oneui_terminal_view_get_selected_text_utf8(
        view: *mut OneUiWidget,
        buffer: *mut c_char,
        buffer_len: usize,
    ) -> usize;
    pub fn oneui_terminal_view_set_on_text_input_utf8(
        view: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_paste_utf8(
        view: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_raw_key(
        view: *mut OneUiWidget,
        callback: OneUiRawKeyCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_scroll_rows_per_wheel(view: *mut OneUiWidget, rows: f32);
    pub fn oneui_terminal_view_set_on_scroll(
        view: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_mouse_reporting(view: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_terminal_view_set_on_pointer(
        view: *mut OneUiWidget,
        callback: OneUiTerminalPointerCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_hyperlink(
        view: *mut OneUiWidget,
        callback: OneUiTerminalHyperlinkCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_cursor_changed(
        view: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_viewport_changed(
        view: *mut OneUiWidget,
        callback: OneUiTerminalViewportCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_focus_changed(
        view: *mut OneUiWidget,
        callback: OneUiBoolCallback,
        user_data: *mut c_void,
    );
}
