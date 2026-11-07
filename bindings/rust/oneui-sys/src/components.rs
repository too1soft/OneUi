//! Component protocol v1. All callbacks and registry operations use the UI thread.
use crate::{OneUiUtf8String, OneUiValue, OneUiViewMount, OneUiWidget};
use std::ffi::c_void;

pub const ONEUI_COMPONENT_ABI_V1: u32 = 1;
#[repr(C)]
pub struct OneUiComponentRegistry { _private: [u8; 0] }
#[repr(C)]
pub struct OneUiComponentStyleV1 {
    pub struct_size: u32,
    pub foreground: u32, pub background: u32, pub border: u32,
    pub font_size: f32, pub radius: f32, pub border_width: f32,
}
#[repr(C)]
pub struct OneUiComponentHostV1 {
    pub struct_size: u32, pub abi_version: u32, pub token: *mut c_void,
    pub emit: Option<unsafe extern "C" fn(*mut c_void, u32, *const OneUiValue, usize) -> i32>,
    pub change: Option<unsafe extern "C" fn(*mut c_void, u32, *const OneUiValue) -> i32>,
    pub style: Option<unsafe extern "C" fn(*mut c_void, u32, *mut OneUiComponentStyleV1) -> i32>,
}
#[repr(C)]
pub struct OneUiComponentFactoryV1 {
    pub struct_size: u32, pub abi_version: u32,
    pub contract: OneUiUtf8String, pub data: *mut c_void,
    pub retain_data: Option<unsafe extern "C" fn(*mut c_void)>,
    pub release_data: Option<unsafe extern "C" fn(*mut c_void)>,
    pub create: Option<unsafe extern "C" fn(*mut c_void, *const OneUiComponentHostV1,
        *const *mut OneUiWidget, usize, *mut *mut c_void, *mut *mut OneUiWidget) -> i32>,
    pub set: Option<unsafe extern "C" fn(*mut c_void, u32, *const OneUiValue) -> i32>,
    pub disconnect: Option<unsafe extern "C" fn(*mut c_void)>,
    pub release_instance: Option<unsafe extern "C" fn(*mut c_void)>,
    pub style_changed: Option<unsafe extern "C" fn(*mut c_void)>,
}
extern "C" {
    pub fn oneui_component_abi_version() -> u32;
    pub fn oneui_component_registry_create_v1(out: *mut *mut OneUiComponentRegistry) -> i32;
    pub fn oneui_component_registry_register_v1(registry: *mut OneUiComponentRegistry,
        factory: *const OneUiComponentFactoryV1) -> i32;
    pub fn oneui_component_registry_destroy(registry: *mut OneUiComponentRegistry) -> i32;
    pub fn oneui_view_mount_set_components_v1(mount: *mut OneUiViewMount,
        registry: *mut OneUiComponentRegistry) -> i32;
}
