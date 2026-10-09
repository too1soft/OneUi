use crate::OneUiRect;
use std::ffi::{c_int, c_uchar, c_ulonglong, c_void};
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiDockHitTarget {
    pub pane: c_ulonglong,
    pub bounds: OneUiRect,
    pub floating: c_int,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct OneUiDockHit {
    pub target: c_ulonglong,
    pub bounds: OneUiRect,
    pub preview: OneUiRect,
    pub placement: c_int,
}
extern "C" {
    pub fn oneui_dock_hit_test(
        source: c_ulonglong,
        x: f32,
        y: f32,
        targets: *const OneUiDockHitTarget,
        count: usize,
        mode: c_int,
        result: *mut OneUiDockHit,
    ) -> c_int;
}
#[repr(C)]
pub struct OneUiDockModel {
    _private: [u8; 0],
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct OneUiDockSize {
    pub width: f32,
    pub height: f32,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiDockTreeNode {
    pub kind: c_int,
    pub pane: c_ulonglong,
    pub axis: c_int,
    pub ratio: f32,
    pub first: c_int,
    pub second: c_int,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiDockFloating {
    pub pane: c_ulonglong,
    pub bounds: OneUiRect,
    pub pinned: c_int,
    pub order: c_ulonglong,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiDockRestore {
    pub pane: c_ulonglong,
    pub before_root: c_int,
    pub after_root: c_int,
    pub target: c_ulonglong,
    pub edge: c_int,
    pub ratio: f32,
    pub has_floating: c_int,
    pub floating: OneUiDockFloating,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiDockSnapshot {
    pub nodes: *const OneUiDockTreeNode,
    pub node_count: usize,
    pub root: c_int,
    pub floating: *const OneUiDockFloating,
    pub floating_count: usize,
    pub hidden: *const OneUiDockRestore,
    pub hidden_count: usize,
    pub maximized: c_ulonglong,
}
pub type OneUiDockSnapshotCallback =
    Option<unsafe extern "C" fn(*const OneUiDockSnapshot, *mut c_void)>;
pub type OneUiDockMinimumCallback =
    Option<unsafe extern "C" fn(c_ulonglong, *mut c_void) -> OneUiDockSize>;
pub type OneUiDockGeometryCallback = Option<
    unsafe extern "C" fn(c_int, c_ulonglong, c_int, *const c_uchar, usize, OneUiRect, *mut c_void),
>;
unsafe extern "C" {
    pub fn oneui_dock_model_minimum(
        model: *mut OneUiDockModel,
        gap: f32,
        policy: OneUiDockMinimumCallback,
        data: *mut c_void,
        result: *mut OneUiDockSize,
    ) -> c_int;
    pub fn oneui_dock_model_create(
        snapshot: *const OneUiDockSnapshot,
        error: *mut c_int,
    ) -> *mut OneUiDockModel;
    pub fn oneui_dock_model_destroy(model: *mut OneUiDockModel);
    pub fn oneui_dock_model_sequence(model: *mut OneUiDockModel) -> c_ulonglong;
    pub fn oneui_dock_model_preserve_sequence(
        model: *mut OneUiDockModel,
        sequence: c_ulonglong,
    ) -> c_int;
    pub fn oneui_dock_model_snapshot(
        model: *mut OneUiDockModel,
        callback: OneUiDockSnapshotCallback,
        data: *mut c_void,
    ) -> c_int;
    pub fn oneui_dock_model_apply(
        model: *mut OneUiDockModel,
        command: c_int,
        id: c_ulonglong,
        target: c_ulonglong,
        bounds: OneUiRect,
        value: f32,
        path: *const c_uchar,
        path_length: usize,
    ) -> c_int;
    pub fn oneui_dock_model_geometry(
        model: *mut OneUiDockModel,
        bounds: OneUiRect,
        gap: f32,
        minimum: OneUiDockMinimumCallback,
        minimum_data: *mut c_void,
        geometry: OneUiDockGeometryCallback,
        geometry_data: *mut c_void,
    ) -> c_int;
    pub fn oneui_dock_clamp_floating(
        bounds: OneUiRect,
        workspace: OneUiRect,
        minimum: OneUiDockSize,
        result: *mut OneUiRect,
    ) -> c_int;
    pub fn oneui_dock_resize_floating(
        bounds: OneUiRect,
        dx: f32,
        dy: f32,
        edge: c_int,
        workspace: OneUiRect,
        minimum: OneUiDockSize,
        result: *mut OneUiRect,
    ) -> c_int;
}
