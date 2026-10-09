use crate::{OneUiIntCallback, OneUiUtf8String, OneUiWindow};
use std::ffi::{c_int, c_void};
pub type OneUiDestroyCallback = Option<unsafe extern "C" fn(*mut c_void)>;

#[repr(C)]
pub struct OneUiApplication {
    _private: [u8; 0],
}
#[repr(C)]
pub struct OneUiCloseDecision {
    _private: [u8; 0],
}
#[repr(C)]
pub struct OneUiWindowSubscription {
    _private: [u8; 0],
}
#[repr(C)]
pub struct OneUiModalSession {
    _private: [u8; 0],
}
#[repr(C)]
#[derive(Clone, Copy, Default, Debug)]
pub struct OneUiWindowState {
    pub known_fields: u32,
    pub flags: u32,
    pub width: f32,
    pub height: f32,
    pub pixel_width: f32,
    pub pixel_height: f32,
    pub dpi_scale: f32,
}
pub type OneUiWindowStateCallback =
    Option<unsafe extern "C" fn(*const OneUiWindowState, *mut c_void)>;
pub type OneUiCloseGuardCallback =
    Option<unsafe extern "C" fn(*mut OneUiCloseDecision, *mut c_void)>;
pub type OneUiApplicationActivationCallback =
    Option<unsafe extern "C" fn(OneUiUtf8String, OneUiUtf8String, OneUiUtf8String, *mut c_void)>;

extern "C" {
    pub fn oneui_application_create_utf8(
        id: OneUiUtf8String,
        name: OneUiUtf8String,
    ) -> *mut OneUiApplication;
    pub fn oneui_application_destroy(app: *mut OneUiApplication);
    pub fn oneui_application_add_window(
        app: *mut OneUiApplication,
        window: *mut OneUiWindow,
        main: c_int,
    ) -> c_int;
    pub fn oneui_application_remove_window(
        app: *mut OneUiApplication,
        window: *mut OneUiWindow,
    ) -> c_int;
    pub fn oneui_application_set_quit_policy(app: *mut OneUiApplication, policy: c_int) -> c_int;
    pub fn oneui_application_run(app: *mut OneUiApplication) -> c_int;
    pub fn oneui_application_request_quit(app: *mut OneUiApplication) -> c_int;
    pub fn oneui_application_set_close_guard(
        app: *mut OneUiApplication,
        window: *mut OneUiWindow,
        callback: OneUiCloseGuardCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> c_int;
    pub fn oneui_application_set_on_activation(
        app: *mut OneUiApplication,
        callback: OneUiApplicationActivationCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> c_int;
    pub fn oneui_close_decision_resolve(decision: *mut OneUiCloseDecision, accept: c_int);
    pub fn oneui_close_decision_destroy(decision: *mut OneUiCloseDecision);
    pub fn oneui_window_get_state(window: *mut OneUiWindow, state: *mut OneUiWindowState) -> c_int;
    pub fn oneui_window_hide(window: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_restore(window: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_maximize(window: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_set_topmost(window: *mut OneUiWindow, enabled: c_int) -> c_int;
    pub fn oneui_window_set_resizable(window: *mut OneUiWindow, enabled: c_int) -> c_int;
    pub fn oneui_window_set_client_size(window: *mut OneUiWindow, width: f32, height: f32)
        -> c_int;
    pub fn oneui_window_set_maximum_client_size(
        window: *mut OneUiWindow,
        width: f32,
        height: f32,
    ) -> c_int;
    pub fn oneui_window_set_owner(window: *mut OneUiWindow, parent: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_observe_state(
        window: *mut OneUiWindow,
        callback: OneUiWindowStateCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> *mut OneUiWindowSubscription;
    pub fn oneui_window_subscription_destroy(subscription: *mut OneUiWindowSubscription);
    pub fn oneui_modal_show(
        child: *mut OneUiWindow,
        parent: *mut OneUiWindow,
        callback: OneUiIntCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> *mut OneUiModalSession;
    pub fn oneui_modal_finish(session: *mut OneUiModalSession, result: c_int);
    pub fn oneui_modal_destroy(session: *mut OneUiModalSession);
}
