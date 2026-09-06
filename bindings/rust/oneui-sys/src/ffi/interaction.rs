use crate::{OneUiUtf8String, OneUiWidget, OneUiWindow};
use std::ffi::{c_int, c_uint, c_void};

#[repr(C)]
pub struct OneUiCommandRegistration {
    _private: [u8; 0],
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiKeyChordUtf8 {
    pub key: OneUiUtf8String,
    pub modifiers: c_uint,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiTextOptionsUtf8 {
    pub direction: c_uint,
    pub wrap: c_uint,
    pub locale: OneUiUtf8String,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct OneUiTextPositionUtf8 {
    pub utf8_offset: usize,
    pub affinity: c_uint,
}
pub type OneUiCommandEnabledCallback = Option<unsafe extern "C" fn(*mut c_void) -> c_int>;

extern "C" {
    pub fn oneui_widget_register_command_utf8(
        widget: *mut OneUiWidget,
        id: OneUiUtf8String,
        shortcut: *const OneUiKeyChordUtf8,
        execute: Option<unsafe extern "C" fn(*mut c_void)>,
        enabled: OneUiCommandEnabledCallback,
        user_data: *mut c_void,
        destroy: Option<unsafe extern "C" fn(*mut c_void)>,
    ) -> *mut OneUiCommandRegistration;
    pub fn oneui_window_register_command_utf8(
        window: *mut OneUiWindow,
        id: OneUiUtf8String,
        shortcut: *const OneUiKeyChordUtf8,
        execute: Option<unsafe extern "C" fn(*mut c_void)>,
        enabled: OneUiCommandEnabledCallback,
        user_data: *mut c_void,
        destroy: Option<unsafe extern "C" fn(*mut c_void)>,
    ) -> *mut OneUiCommandRegistration;
    pub fn oneui_command_registration_destroy(registration: *mut OneUiCommandRegistration);
    pub fn oneui_widget_query_command_utf8(widget: *mut OneUiWidget, id: OneUiUtf8String)
        -> c_uint;
    pub fn oneui_widget_execute_command_utf8(
        widget: *mut OneUiWidget,
        id: OneUiUtf8String,
    ) -> c_uint;
    pub fn oneui_window_query_command_utf8(window: *mut OneUiWindow, id: OneUiUtf8String)
        -> c_uint;
    pub fn oneui_window_execute_command_utf8(
        window: *mut OneUiWindow,
        id: OneUiUtf8String,
    ) -> c_uint;
    pub fn oneui_widget_set_text_options_utf8(
        widget: *mut OneUiWidget,
        options: *const OneUiTextOptionsUtf8,
    ) -> c_int;
    pub fn oneui_text_field_get_position_utf8(
        widget: *mut OneUiWidget,
        position: *mut OneUiTextPositionUtf8,
    ) -> c_int;
    pub fn oneui_text_field_set_position_utf8(
        widget: *mut OneUiWidget,
        position: OneUiTextPositionUtf8,
    ) -> c_int;
}
