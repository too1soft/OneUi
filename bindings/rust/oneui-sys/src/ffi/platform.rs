use crate::OneUiWindow;
use std::ffi::{c_int, c_uint};

pub const WINDOW_CAPABILITY_CLIPBOARD: c_uint = 1 << 0;
pub const WINDOW_CAPABILITY_IME: c_uint = 1 << 1;
pub const WINDOW_CAPABILITY_PLACEMENT: c_uint = 1 << 2;
pub const WINDOW_CAPABILITY_ACTIVATION: c_uint = 1 << 3;
pub const WINDOW_CAPABILITY_TOPMOST: c_uint = 1 << 4;
pub const WINDOW_CAPABILITY_TRAY: c_uint = 1 << 5;
pub const WINDOW_CAPABILITY_NATIVE_DIALOGS: c_uint = 1 << 6;

extern "C" {
    pub fn oneui_window_backend(window: *mut OneUiWindow) -> c_uint;
    pub fn oneui_window_capabilities(window: *mut OneUiWindow) -> c_uint;
    pub fn oneui_window_initialize_checked(window: *mut OneUiWindow) -> c_int;
}
