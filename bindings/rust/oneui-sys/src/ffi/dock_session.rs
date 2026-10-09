use crate::{
    OneUiApplication, OneUiDestroyCallback, OneUiStyleSheet, OneUiUtf8String, OneUiVoidCallback,
    OneUiWidget, OneUiWindow,
};
use std::ffi::{c_float, c_int, c_ulonglong, c_void};
#[repr(C)]
pub struct OneUiDockSession {
    _private: [u8; 0],
}
extern "C" {
    pub fn oneui_dock_session_set_style_sheet(
        session: *mut OneUiDockSession,
        sheet: *mut OneUiStyleSheet,
    ) -> c_int;
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiDockDestination {
    pub host: c_ulonglong,
    pub pane: c_ulonglong,
    pub position: c_int,
    pub index: c_int,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiDockPaneLocation {
    pub host: c_ulonglong,
    pub group: c_ulonglong,
    pub index: c_int,
    pub selected: c_int,
}
pub type OneUiDockTransferCallback =
    Option<unsafe extern "C" fn(*mut c_void, *mut c_void, c_int, *mut c_void) -> c_int>;
pub type OneUiDockIdsCallback =
    Option<unsafe extern "C" fn(*const c_ulonglong, usize, *mut c_void)>;
unsafe extern "C" {
    pub fn oneui_tabs_set_on_detach_requested(
        tabs: *mut crate::OneUiWidget,
        callback: crate::OneUiIndexPointCallback,
        data: *mut c_void,
    );
    pub fn oneui_dock_session_native_drag_limitation_utf8(
        session: *mut OneUiDockSession,
        host: c_ulonglong,
        callback: crate::OneUiUtf8TextCallback,
        data: *mut c_void,
    ) -> c_int;

    pub fn oneui_dock_session_native_drag_supported(
        session: *mut OneUiDockSession,
        host: c_ulonglong,
    ) -> c_int;
    pub fn oneui_dock_session_start_native_drag(
        session: *mut OneUiDockSession,
        pane: c_ulonglong,
    ) -> c_int;
    pub fn oneui_dock_session_create(app: *mut OneUiApplication) -> *mut OneUiDockSession;
    pub fn oneui_dock_session_destroy(session: *mut OneUiDockSession);
    pub fn oneui_dock_session_attach_host(
        session: *mut OneUiDockSession,
        window: *mut OneUiWindow,
        panel: *mut OneUiWidget,
        owner: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> c_ulonglong;
    pub fn oneui_dock_session_add_pane_utf8(
        session: *mut OneUiDockSession,
        pane: c_ulonglong,
        title: OneUiUtf8String,
        content: *mut OneUiWidget,
        minimum_width: c_float,
        minimum_height: c_float,
        dest: OneUiDockDestination,
        callback: OneUiDockTransferCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> c_int;
    pub fn oneui_dock_session_remove_pane(
        session: *mut OneUiDockSession,
        pane: c_ulonglong,
    ) -> c_int;
    pub fn oneui_dock_session_move_pane(
        session: *mut OneUiDockSession,
        pane: c_ulonglong,
        dest: OneUiDockDestination,
    ) -> c_int;
    pub fn oneui_dock_session_select_pane(
        session: *mut OneUiDockSession,
        pane: c_ulonglong,
    ) -> c_int;
    pub fn oneui_dock_session_detach_pane_utf8(
        session: *mut OneUiDockSession,
        pane: c_ulonglong,
        title: OneUiUtf8String,
        width: c_int,
        height: c_int,
        new_host: *mut c_ulonglong,
    ) -> c_int;
    pub fn oneui_dock_session_begin_drag(
        session: *mut OneUiDockSession,
        pane: c_ulonglong,
    ) -> c_ulonglong;
    pub fn oneui_dock_session_commit_drag(
        session: *mut OneUiDockSession,
        token: c_ulonglong,
        dest: OneUiDockDestination,
    ) -> c_int;
    pub fn oneui_dock_session_cancel_drag(
        session: *mut OneUiDockSession,
        token: c_ulonglong,
    ) -> c_int;
    pub fn oneui_dock_session_location(
        session: *mut OneUiDockSession,
        pane: c_ulonglong,
    ) -> OneUiDockPaneLocation;
    pub fn oneui_dock_session_panes(
        session: *mut OneUiDockSession,
        host: c_ulonglong,
        callback: OneUiDockIdsCallback,
        data: *mut c_void,
    ) -> c_int;
    pub fn oneui_dock_session_hosts(
        session: *mut OneUiDockSession,
        callback: OneUiDockIdsCallback,
        data: *mut c_void,
    ) -> c_int;
    pub fn oneui_dock_session_set_on_changed(
        session: *mut OneUiDockSession,
        callback: OneUiVoidCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> c_int;
}
