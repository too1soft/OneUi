use super::{OneUiColor, OneUiRect, OneUiWidget};
use std::ffi::c_int;
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiCaptionPathCommand { pub verb: c_int, pub x: f32, pub y: f32 }
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiCaptionPath { pub commands: *const OneUiCaptionPathCommand, pub count: usize }
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiCaptionButtonPresentation {
    pub frame: OneUiRect, pub visual: OneUiRect, pub icon: OneUiRect,
    pub glyph: OneUiCaptionPath, pub maximized_glyph: OneUiCaptionPath,
    pub stroke_width: f32, pub corner_radius: f32,
    pub ellipse: c_int, pub glyph_on_group_hover: c_int, pub glyph_uses_foreground: c_int,
    pub fill: OneUiColor, pub pressed_fill: OneUiColor, pub glyph_color: OneUiColor,
}
#[repr(C)]
pub struct OneUiTitleBarPresentation {
    pub struct_size: usize,
    pub logo: OneUiRect, pub logo_icon: OneUiRect, pub title: OneUiRect,
    pub leading: OneUiRect, pub accessory: OneUiRect,
    pub logo_stroke_width: f32, pub title_font_size: f32, pub hide_logo_border: c_int,
    pub buttons: [OneUiCaptionButtonPresentation;3],
}
extern "C" {
    pub fn oneui_title_bar_set_presentation(bar: *mut OneUiWidget, presentation: *const OneUiTitleBarPresentation) -> c_int;
}
