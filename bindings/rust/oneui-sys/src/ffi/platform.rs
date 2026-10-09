use crate::OneUiWindow;
use std::ffi::{c_int, c_uint};

pub const WINDOW_CAPABILITY_CLIPBOARD: c_uint = 1 << 0;
pub const WINDOW_CAPABILITY_IME: c_uint = 1 << 1;
pub const WINDOW_CAPABILITY_PLACEMENT: c_uint = 1 << 2;
pub const WINDOW_CAPABILITY_ACTIVATION: c_uint = 1 << 3;
pub const WINDOW_CAPABILITY_TOPMOST: c_uint = 1 << 4;
pub const WINDOW_CAPABILITY_TRAY: c_uint = 1 << 5;
pub const WINDOW_CAPABILITY_NATIVE_DIALOGS: c_uint = 1 << 6;
pub const WINDOW_CAPABILITY_FILE_DROP: c_uint = 1 << 7;

extern "C" {
    pub fn oneui_window_backend(window: *mut OneUiWindow) -> c_uint;
    pub fn oneui_window_capabilities(window: *mut OneUiWindow) -> c_uint;
    pub fn oneui_window_initialize_checked(window: *mut OneUiWindow) -> c_int;
}

pub const WINDOW_CAPABILITY_MENU_BAR: c_uint = 1 << 8;
pub const WINDOW_CAPABILITY_TRAY_TOOLTIP: c_uint = 1 << 9;

pub const NATIVE_MENU_ENABLED: c_uint = 1;
pub const NATIVE_MENU_SEPARATOR: c_uint = 2;
pub const NATIVE_MENU_CHECK: c_uint = 4;
pub const NATIVE_MENU_RADIO: c_uint = 8;
pub const NATIVE_MENU_CHECKED: c_uint = 16;
