use std::ffi::c_int;

use crate::{OneUiColor, OneUiUtf8String};

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
