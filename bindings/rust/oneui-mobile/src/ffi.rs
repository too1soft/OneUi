use std::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Viewport {
    pub width_px: f32,
    pub height_px: f32,
    pub scale: f32,
    pub safe_left_px: f32,
    pub safe_top_px: f32,
    pub safe_right_px: f32,
    pub safe_bottom_px: f32,
    pub keyboard_bottom_px: f32,
}
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Frame {
    pub time_ns: u64,
    pub surface_generation: u64,
    pub content_x: f32,
    pub content_y: f32,
    pub content_width: f32,
    pub content_height: f32,
    pub scale: f32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub(crate) struct RawPointer {
    pub id: i32,
    pub phase: i32,
    pub kind: i32,
    pub x: f32,
    pub y: f32,
    pub pressure: f32,
    pub time_ns: u64,
    pub primary: u32,
}
#[repr(C)]
#[derive(Default)]
pub(crate) struct RawState {
    pub lifecycle: i32,
    pub surface_available: u32,
    pub needs_frame: u32,
    pub active_pointers: u32,
    pub surface_generation: u64,
}
extern "C" {
    pub fn oneui_mobile_abi_version() -> u32;
    pub fn oneui_mobile_create(out: *mut *mut c_void) -> i32;
    pub fn oneui_mobile_destroy(runtime: *mut c_void) -> i32;
    pub fn oneui_mobile_set_lifecycle(runtime: *mut c_void, state: i32) -> i32;
    pub fn oneui_mobile_set_viewport_v1(runtime: *mut c_void, viewport: *const Viewport) -> i32;
    pub fn oneui_mobile_set_surface(runtime: *mut c_void, available: u32) -> i32;
    pub fn oneui_mobile_invalidate(runtime: *mut c_void) -> i32;
    pub fn oneui_mobile_set_animating(runtime: *mut c_void, enabled: u32) -> i32;
    pub fn oneui_mobile_push_pointer_v1(runtime: *mut c_void, pointer: *const RawPointer) -> i32;
    pub fn oneui_mobile_cancel_pointers(runtime: *mut c_void) -> i32;
    pub fn oneui_mobile_poll_pointer_v1(runtime: *mut c_void, pointer: *mut RawPointer) -> i32;
    pub fn oneui_mobile_take_frame_v1(runtime: *mut c_void, time: u64, frame: *mut Frame) -> i32;
    pub fn oneui_mobile_get_state_v1(runtime: *mut c_void, state: *mut RawState) -> i32;
}
