use std::ffi::c_int;

use crate::{OneUiColor, OneUiUtf8String, WChar};

pub type OneUiCommandResult = c_int;
pub type OneUiStackDirection = c_int;
pub type OneUiStackAlign = c_int;
pub type OneUiSplitOrientation = c_int;
pub type OneUiVideoScaleMode = c_int;
pub type OneUiPixelFormat = c_int;
pub type OneUiRemoteCursorMode = c_int;
pub type OneUiButtonVariant = c_int;
pub type OneUiTerminalCursorStyle = c_int;
pub type OneUiTerminalAuxiliaryButtonAction = c_int;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiRichListItemUtf8 {
    pub title: OneUiUtf8String,
    pub detail: OneUiUtf8String,
    pub badge: OneUiUtf8String,
    pub trailing: OneUiUtf8String,
    pub indicator_color: OneUiColor,
    pub trailing_color: OneUiColor,
    pub indicator_visible: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiVirtualListRichMetrics {
    pub indicator_space: f32,
    pub indicator_diameter: f32,
    pub badge_height: f32,
    pub badge_radius: f32,
    pub badge_horizontal_padding: f32,
    pub title_badge_gap: f32,
    pub trailing_width: f32,
    pub trailing_gap: f32,
}
#[repr(C)]
pub struct OneUiTrayMenuItem {
    pub id: i32,
    pub text: *const WChar,
    pub enabled: i32,
    pub separator: i32,
}

#[repr(C)]
pub struct OneUiNativeMenuItemUtf8 {
    pub id: c_int,
    pub text: *const std::ffi::c_char,
    pub flags: std::ffi::c_uint,
    pub radio_group: std::ffi::c_uint,
    pub shortcut_key: *const std::ffi::c_char,
    pub shortcut_modifiers: std::ffi::c_uint,
    pub children: *const OneUiNativeMenuItemUtf8,
    pub child_count: usize,
}
