//! Abi types for the UTF-8 C ABI.

use std::ffi::{c_char, c_float, c_int, c_uint, c_ushort, c_void};

// C enums remain integer aliases so unknown values can be handled without
// creating an invalid Rust enum discriminant. The ABI suite checks their layout.
pub type OneUiErrorCode = c_int;
pub type OneUiFileDialogMode = c_int;
pub type OneUiPointerButton = c_int;
pub type OneUiTerminalPointerAction = c_int;

/// Native C wchar_t, not a fixed-width UTF-16 buffer. Prefer UTF-8 entrypoints.
#[cfg(windows)]
pub type WChar = u16;
#[cfg(not(windows))]
pub type WChar = u32;

#[repr(C)]
pub struct OneUiWindow {
    _private: [u8; 0],
}

#[repr(C)]
pub struct OneUiWidget {
    _private: [u8; 0],
}
#[repr(C)]
pub struct OneUiFocusBookmark {
    _private: [u8; 0],
}
pub type OneUiPointerDragCallback =
    Option<unsafe extern "C" fn(c_int, c_float, c_float, *const OneUiPointerEvent, *mut c_void)>;
#[repr(C)]
pub struct OneUiProgressSegmentUtf8 {
    pub fraction: f64,
    pub color: OneUiColor,
    pub label: OneUiUtf8String,
}
#[repr(C)]
pub struct OneUiTableRichCellUtf8 {
    pub text: OneUiUtf8String,
    pub detail: OneUiUtf8String,
    pub badge: OneUiUtf8String,
    pub alignment: i32,
    pub icon: i32,
    pub font_size: f32,
    pub font_weight: i32,
    pub foreground: OneUiColor,
    pub badge_foreground: OneUiColor,
    pub badge_background: OneUiColor,
    pub badge_color_flags: c_uint,
}
#[repr(C)]
pub struct OneUiTableRichRowUtf8 {
    pub cells: *const OneUiTableRichCellUtf8,
    pub count: usize,
}

#[repr(C)]
pub struct OneUiStyleSheet {
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
pub struct OneUiTableColumnUtf8 {
    pub header: OneUiUtf8String,
    pub width: f32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiTableRowUtf8 {
    pub cells: *const OneUiUtf8String,
    pub cell_count: usize,
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
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct OneUiColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct OneUiLabelTextSpanUtf8 {
    pub start_utf8_offset: usize,
    pub end_utf8_offset: usize,
    pub font_size: f32,
    pub font_weight: c_int,
    pub italic: c_int,
    pub monospace: c_int,
    pub underline: c_int,
    pub strikethrough: c_int,
    pub foreground: OneUiColor,
    pub background: OneUiColor,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiTimeSeriesUtf8 {
    pub name: OneUiUtf8String,
    pub color: OneUiColor,
    pub values: *const f64,
    pub value_count: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiTimeSeriesThreshold {
    pub value: f64,
    pub color: OneUiColor,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct OneUiRect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct OneUiVideoFramePatch {
    pub pixels: *const c_void,
    pub pixel_bytes: usize,
    pub x: c_int,
    pub y: c_int,
    pub width: c_int,
    pub height: c_int,
    pub stride: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiPointerEvent {
    pub x: f32,
    pub y: f32,
    pub button: c_int,
    pub click_count: c_int,
    pub shift: c_int,
    pub control: c_int,
    pub alt: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiFocusRingStyle {
    pub color: OneUiColor,
    pub width: f32,
    pub offset: f32,
    pub radius: f32,
    pub visible: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiButtonStateStyle {
    pub background: OneUiColor,
    pub foreground: OneUiColor,
    pub border: OneUiColor,
    pub border_width: f32,
    pub radius: f32,
    pub focus_ring: OneUiFocusRingStyle,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiButtonStyle {
    pub normal: OneUiButtonStateStyle,
    pub hovered: OneUiButtonStateStyle,
    pub pressed: OneUiButtonStateStyle,
    pub disabled: OneUiButtonStateStyle,
    pub focus_visible: OneUiButtonStateStyle,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiInteractiveSurfaceStateStyle {
    pub background: OneUiColor,
    pub border: OneUiColor,
    pub border_width: f32,
    pub radius: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiInteractiveSurfaceStyle {
    pub normal: OneUiInteractiveSurfaceStateStyle,
    pub hovered: OneUiInteractiveSurfaceStateStyle,
    pub pressed: OneUiInteractiveSurfaceStateStyle,
    pub disabled: OneUiInteractiveSurfaceStateStyle,
    pub focus_visible: OneUiInteractiveSurfaceStateStyle,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiTerminalCellUtf8 {
    pub text: OneUiUtf8String,
    pub foreground: OneUiColor,
    pub background: OneUiColor,
    pub style: c_uint,
    pub hyperlink_id: c_uint,
    pub underline_style: c_uint,
    pub underline_color: OneUiColor,
    pub underline_color_set: c_int,
}

impl OneUiUtf8String {
    // This ABI view borrows the input bytes, so the owned FromStr contract is
    // intentionally not applicable.
    #[allow(clippy::should_implement_trait)]
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

pub const FILE_DIALOG_OPEN_FILE: c_int = 0;
pub const FILE_DIALOG_SAVE_FILE: c_int = 1;
pub const FILE_DIALOG_SELECT_FOLDER: c_int = 2;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiFileDialogFilterUtf8 {
    pub name: OneUiUtf8String,
    pub pattern: OneUiUtf8String,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiFileDialogOptionsUtf8 {
    pub mode: OneUiFileDialogMode,
    pub title: OneUiUtf8String,
    pub initial_directory: OneUiUtf8String,
    pub default_name: OneUiUtf8String,
    pub default_extension: OneUiUtf8String,
    pub filters: *const OneUiFileDialogFilterUtf8,
    pub filter_count: usize,
    pub confirm_overwrite: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct OneUiWindowPlacement {
    pub x: c_int,
    pub y: c_int,
    pub width: c_int,
    pub height: c_int,
    pub maximized: c_int,
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
    unsafe extern "C" fn(id: *const c_char, length: usize, expanded: c_int, user_data: *mut c_void),
>;
pub type OneUiVoidCallback = Option<unsafe extern "C" fn(user_data: *mut c_void)>;
pub type OneUiFrameCallback = Option<unsafe extern "C" fn(now_ms: f64, user_data: *mut c_void)>;
pub type OneUiBoolCallback = Option<unsafe extern "C" fn(value: c_int, user_data: *mut c_void)>;
pub type OneUiIntCallback = Option<unsafe extern "C" fn(value: c_int, user_data: *mut c_void)>;
pub type OneUiFloatCallback = Option<unsafe extern "C" fn(value: f32, user_data: *mut c_void)>;
pub type OneUiSliderInteractionCallback =
    Option<unsafe extern "C" fn(phase: c_int, value: f64, user_data: *mut c_void)>;
pub type OneUiIndexPointCallback =
    Option<unsafe extern "C" fn(index: c_int, x: f32, y: f32, user_data: *mut c_void)>;
pub type OneUiReorderRequestedCallback =
    Option<unsafe extern "C" fn(source_index: c_int, target_index: c_int, user_data: *mut c_void)>;
pub type OneUiTreeReorderRequestedCallback = Option<
    unsafe extern "C" fn(
        source_id: *const c_char,
        source_length: usize,
        target_id: *const c_char,
        target_length: usize,
        user_data: *mut c_void,
    ),
>;
pub type OneUiGridReorderRequestedCallback = Option<
    unsafe extern "C" fn(
        source_id: *const c_char,
        source_length: usize,
        target_index: c_int,
        user_data: *mut c_void,
    ),
>;
pub type OneUiItemDragCallback = Option<
    unsafe extern "C" fn(
        source_id: *const c_char,
        source_length: usize,
        phase: c_int,
        x: f32,
        y: f32,
        user_data: *mut c_void,
    ),
>;
pub type OneUiPointerCallback =
    Option<unsafe extern "C" fn(event: *const OneUiPointerEvent, user_data: *mut c_void)>;

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

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiRemotePointerEvent {
    pub window_x: c_float,
    pub window_y: c_float,
    pub content_x: c_float,
    pub content_y: c_float,
    pub normalized_x: c_float,
    pub normalized_y: c_float,
    pub remote_x: c_float,
    pub remote_y: c_float,
    pub button: OneUiPointerButton,
    pub pressed: c_int,
    pub wheel_delta_x: c_int,
    pub wheel_delta_y: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiTerminalPointerEvent {
    pub action: OneUiTerminalPointerAction,
    pub button: OneUiPointerButton,
    pub x: c_float,
    pub y: c_float,
    pub row: c_ushort,
    pub column: c_ushort,
    pub wheel_delta: c_int,
    pub shift: c_int,
    pub control: c_int,
    pub alt: c_int,
}

pub type OneUiRawKeyCallback =
    Option<unsafe extern "C" fn(event: *const OneUiRawKeyEvent, user_data: *mut c_void)>;
pub type OneUiRemotePointerCallback =
    Option<unsafe extern "C" fn(event: *const OneUiRemotePointerEvent, user_data: *mut c_void)>;
pub type OneUiFrameReleaseCallback =
    Option<unsafe extern "C" fn(pixels: *const c_void, user_data: *mut c_void)>;
pub type OneUiWindowRawKeyCallback =
    Option<unsafe extern "C" fn(event: *const OneUiRawKeyEvent, user_data: *mut c_void) -> c_int>;
pub type OneUiClientSizeChangedCallback =
    Option<unsafe extern "C" fn(width: c_float, height: c_float, user_data: *mut c_void)>;
pub type OneUiWindowActivationChangedCallback =
    Option<unsafe extern "C" fn(active: c_int, user_data: *mut c_void)>;
pub type OneUiFileDropCallback = Option<
    unsafe extern "C" fn(
        paths: *const OneUiUtf8String,
        count: usize,
        x: c_float,
        y: c_float,
        user_data: *mut c_void,
    ),
>;
pub type OneUiTerminalPointerCallback =
    Option<unsafe extern "C" fn(event: *const OneUiTerminalPointerEvent, user_data: *mut c_void)>;
pub type OneUiTerminalHyperlinkCallback =
    Option<unsafe extern "C" fn(hyperlink_id: c_uint, user_data: *mut c_void)>;
pub type OneUiTerminalViewportCallback =
    Option<unsafe extern "C" fn(rows: c_ushort, columns: c_ushort, user_data: *mut c_void)>;
