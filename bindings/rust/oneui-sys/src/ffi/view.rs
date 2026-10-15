//! Language-neutral view protocol v1 (UTF-8 ABI 44).
use crate::{OneUiUtf8String, OneUiWidget, OneUiWindow};
use std::ffi::{c_char, c_void};
#[repr(C)]
pub struct OneUiViewElementMetrics {
    pub struct_size: u32,
    pub scrollable: u32,
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
    pub scroll_x: f32,
    pub scroll_y: f32,
    pub max_scroll_y: f32,
}
extern "C" {
    pub fn oneui_view_element_metrics(
        view: *mut OneUiView,
        name: OneUiUtf8String,
        out: *mut OneUiViewElementMetrics,
    ) -> i32;
    pub fn oneui_view_element_visible(view: *mut OneUiView, name: OneUiUtf8String,
        out: *mut i32) -> i32;
}
#[repr(C)]
pub struct OneUiViewCodeEditorState {
    pub struct_size: u32, pub cursors: u32,
    pub revision: u64, pub caret: u64, pub anchor: u64, pub line: u64, pub column: u64,
    pub length: u64, pub line_count: u64,
}
extern "C" {
    pub fn oneui_view_code_editor_state(view: *mut OneUiView, name: OneUiUtf8String,
        out: *mut OneUiViewCodeEditorState) -> i32;
    pub fn oneui_view_code_editor_text(view: *mut OneUiView, name: OneUiUtf8String, start: u64, end: u64,
        buffer: *mut c_char, capacity: usize, out_length: *mut usize) -> i32;
    pub fn oneui_view_code_editor_replace(view: *mut OneUiView, name: OneUiUtf8String, start: u64, end: u64,
        text: OneUiUtf8String, expected_revision: u64) -> i32;
    pub fn oneui_view_code_editor_select(view: *mut OneUiView, name: OneUiUtf8String, anchor: u64,
        caret: u64) -> i32;
}

pub type OneUiViewResult = i32;
pub const ONEUI_VIEW_PROTOCOL_VERSION: u32 = 1;
pub const ONEUI_VALUE_BOOL: u32 = 1;
pub const ONEUI_VALUE_I64: u32 = 2;
pub const ONEUI_VALUE_F64: u32 = 3;
pub const ONEUI_VALUE_UTF8: u32 = 4;
pub const ONEUI_LIST_RESET: u32 = 0;
pub const ONEUI_LIST_INSERT: u32 = 1;
pub const ONEUI_LIST_REMOVE: u32 = 2;
pub const ONEUI_LIST_MOVE: u32 = 3;
pub const ONEUI_LIST_UPDATE: u32 = 4;
pub const ONEUI_LIST_REORDER: u32 = 5;

#[repr(C)]
pub struct OneUiView {
    _private: [u8; 0],
}
#[repr(C)]
pub struct OneUiViewMount {
    _private: [u8; 0],
}
#[repr(C)]
pub struct OneUiViewNotifier {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiValueUtf8 {
    pub data: *const c_char,
    pub length: usize,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub union OneUiValueData {
    pub boolean: u32,
    pub integer: i64,
    pub number: f64,
    pub utf8: OneUiValueUtf8,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiValue {
    pub tag: u32,
    pub reserved: u32,
    pub data: OneUiValueData,
}
impl Default for OneUiValue {
    fn default() -> Self {
        unsafe { std::mem::zeroed() }
    }
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiViewModelRef {
    pub vtable: *const OneUiViewModelVTable,
    pub vm: *mut c_void,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct OneUiViewListInfo {
    pub revision: u64,
    pub length: u64,
}
#[repr(C)]
pub struct OneUiViewListChange {
    pub struct_size: u32,
    pub kind: u32,
    pub before_revision: u64,
    pub after_revision: u64,
    pub index: u64,
    pub count: u64,
    pub to_index: u64,
    pub keys: *const OneUiUtf8String,
    pub key_count: usize,
    pub properties: *const u32,
    pub property_count: usize,
}
#[repr(C)]
pub struct OneUiViewModelVTable {
    pub struct_size: u32,
    pub protocol_version: u32,
    pub schema_hash: [u8; 32],
    pub model_id: u32,
    pub flags: u32,
    pub retain: Option<unsafe extern "C" fn(*mut c_void)>,
    pub release: Option<unsafe extern "C" fn(*mut c_void)>,
    pub connect: Option<unsafe extern "C" fn(*mut c_void, *mut OneUiViewNotifier) -> i32>,
    pub disconnect: Option<unsafe extern "C" fn(*mut c_void, *mut OneUiViewNotifier)>,
    pub get: Option<unsafe extern "C" fn(*mut c_void, u32, *mut OneUiValue) -> i32>,
    pub set: Option<unsafe extern "C" fn(*mut c_void, u32, *const OneUiValue) -> i32>,
    pub invoke: Option<unsafe extern "C" fn(*mut c_void, u32, *const OneUiValue, usize) -> i32>,
    pub list_info: Option<unsafe extern "C" fn(*mut c_void, u32, *mut OneUiViewListInfo) -> i32>,
    pub list_key: Option<unsafe extern "C" fn(*mut c_void, u32, u64, *mut OneUiValue) -> i32>,
    pub list_acquire:
        Option<unsafe extern "C" fn(*mut c_void, u32, u64, *mut OneUiViewModelRef) -> i32>,
}
#[repr(C)]
pub struct OneUiViewError {
    pub struct_size: u32,
    pub result: i32,
    pub sequence: u64,
    pub section_kind: u32,
    pub record_index: u32,
    pub source_map_index: u32,
    pub reserved: u32,
    pub message: [c_char; 512],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiViewScopeSegment {
    pub instance_id: OneUiUtf8String,
    pub key: OneUiUtf8String,
}
extern "C" {
    pub fn oneui_view_resolve_scope(
        view: *mut OneUiView,
        segments: *const OneUiViewScopeSegment,
        segment_count: usize,
        out_scope: *mut *mut OneUiViewNotifier,
    ) -> i32;
    pub fn oneui_view_protocol_version() -> u32;
    pub fn oneui_view_mount_create(
        window: *mut OneUiWindow,
        destination: *mut OneUiWidget,
        out: *mut *mut OneUiViewMount,
    ) -> i32;
    pub fn oneui_view_mount_destroy(mount: *mut OneUiViewMount);
    pub fn oneui_view_dock_action(
        view: *mut OneUiView,
        name: OneUiUtf8String,
        action: i32,
        pane: OneUiUtf8String,
        flag: i32,
    ) -> i32;
    pub fn oneui_view_dock_item_metrics(
        view: *mut OneUiView,
        name: OneUiUtf8String,
        pane: OneUiUtf8String,
        out: *mut OneUiViewElementMetrics,
    ) -> i32;
    pub fn oneui_view_element_focused(
        view: *mut OneUiView,
        name: OneUiUtf8String,
        out: *mut i32,
    ) -> i32;
    pub fn oneui_view_mount_child(
        parent: *mut OneUiViewMount,
        name: OneUiUtf8String,
        out: *mut *mut OneUiViewMount,
    ) -> i32;
    pub fn oneui_view_mount_child_with_destination(
        parent: *mut OneUiViewMount,
        name: OneUiUtf8String,
        destination: *mut OneUiWidget,
        out: *mut *mut OneUiViewMount,
    ) -> i32;
    pub fn oneui_view_mount_set_styles(
        mount: *mut OneUiViewMount,
        css: OneUiUtf8String,
        density: u32,
    ) -> i32;
    pub fn oneui_view_instantiate(
        mount: *mut OneUiViewMount,
        ir: *const u8,
        len: usize,
        table: *const OneUiViewModelVTable,
        vm: *mut c_void,
        out: *mut *mut OneUiView,
    ) -> i32;
    pub fn oneui_view_root(view: *mut OneUiView) -> *mut OneUiWidget;
    pub fn oneui_view_notify(view: *mut OneUiView, property: u32) -> i32;
    pub fn oneui_view_notify_list(
        view: *mut OneUiView,
        property: u32,
        change: *const OneUiViewListChange,
    ) -> i32;
    pub fn oneui_view_flush(view: *mut OneUiView) -> i32;
    pub fn oneui_view_reload(view: *mut OneUiView, ir: *const u8, len: usize) -> i32;
    pub fn oneui_view_disconnect(view: *mut OneUiView) -> i32;
    pub fn oneui_view_destroy(view: *mut OneUiView);
    pub fn oneui_view_get_error(view: *mut OneUiView, out: *mut OneUiViewError) -> i32;
    pub fn oneui_view_notifier(view: *mut OneUiView, out: *mut *mut OneUiViewNotifier) -> i32;
    pub fn oneui_view_notifier_retain(notifier: *mut OneUiViewNotifier);
    pub fn oneui_view_notifier_release(notifier: *mut OneUiViewNotifier);
    pub fn oneui_view_notifier_notify(notifier: *mut OneUiViewNotifier, property: u32) -> i32;
    pub fn oneui_view_notifier_notify_list(
        notifier: *mut OneUiViewNotifier,
        property: u32,
        change: *const OneUiViewListChange,
    ) -> i32;
    pub fn oneui_view_find_host(
        view: *mut OneUiView,
        scope: *mut OneUiViewNotifier,
        name: OneUiUtf8String,
        out: *mut *mut OneUiWidget,
    ) -> i32;
    pub fn oneui_view_host_attach(
        host: *mut OneUiWidget,
        content: *mut OneUiWidget,
        owner: *mut c_void,
        retain: Option<unsafe extern "C" fn(*mut c_void)>,
        release: Option<unsafe extern "C" fn(*mut c_void)>,
    ) -> i32;
    pub fn oneui_view_host_detach(host: *mut OneUiWidget) -> i32;
}

extern "C" {
    pub fn oneui_view_mount_focus_element(
        mount: *mut OneUiViewMount,
        name: OneUiUtf8String,
        focus_visible: i32,
        out_focused: *mut i32,
    ) -> i32;
}

extern "C" {
    pub fn oneui_view_mount_register_workspace_tabs(
        mount: *mut OneUiViewMount,
        domain: OneUiUtf8String,
        id: OneUiUtf8String,
        tabs: *mut OneUiWidget,
    ) -> i32;
    pub fn oneui_view_workspace_tab_drag(
        view: *mut OneUiView,
        domain: OneUiUtf8String,
        source: OneUiUtf8String,
        phase: i32,
        x: f32,
        y: f32,
    ) -> i32;
}
extern "C" {
    pub fn oneui_view_mount_point(view: *mut OneUiView, name: OneUiUtf8String,
        out: *mut *mut OneUiWidget) -> OneUiViewResult;
}
