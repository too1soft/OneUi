use crate::{
    OneUiApplication, OneUiDestroyCallback, OneUiUtf8String, OneUiUtf8TextCallback, OneUiWidget,
};
use std::ffi::{c_int, c_void};

#[repr(C)]
pub struct OneUiNotificationService {
    _private: [u8; 0],
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiNotificationActionUtf8 {
    pub id: OneUiUtf8String,
    pub title: OneUiUtf8String,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiNotificationUtf8 {
    pub id: OneUiUtf8String,
    pub title: OneUiUtf8String,
    pub body: OneUiUtf8String,
    pub parameter: OneUiUtf8String,
    pub actions: *const OneUiNotificationActionUtf8,
    pub action_count: usize,
}
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiToastMessageUtf8 {
    pub id: OneUiUtf8String,
    pub title: OneUiUtf8String,
    pub message: OneUiUtf8String,
    pub primary_action: OneUiUtf8String,
    pub secondary_action: OneUiUtf8String,
    pub timeout_ms: f64,
}
pub type OneUiNotificationResultCallback =
    Option<unsafe extern "C" fn(c_int, OneUiUtf8String, OneUiUtf8String, *mut c_void)>;
pub type OneUiNotificationPermissionCallback =
    Option<unsafe extern "C" fn(c_int, c_int, OneUiUtf8String, *mut c_void)>;
unsafe extern "C" {
    pub fn oneui_notification_service_create(
        application: *mut OneUiApplication,
    ) -> *mut OneUiNotificationService;
    pub fn oneui_notification_service_destroy(service: *mut OneUiNotificationService);
    pub fn oneui_notification_permission(
        service: *mut OneUiNotificationService,
        request: c_int,
        callback: OneUiNotificationPermissionCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> c_int;
    pub fn oneui_notification_send_utf8(
        service: *mut OneUiNotificationService,
        value: *const OneUiNotificationUtf8,
        callback: OneUiNotificationResultCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> c_int;
    pub fn oneui_notification_withdraw_utf8(
        service: *mut OneUiNotificationService,
        id: OneUiUtf8String,
        callback: OneUiNotificationResultCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> c_int;
    pub fn oneui_application_is_primary(application: *mut OneUiApplication) -> c_int;
    pub fn oneui_application_register_desktop_utf8(
        id: OneUiUtf8String,
        name: OneUiUtf8String,
        executable: OneUiUtf8String,
        install: c_int,
        callback: OneUiNotificationResultCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> c_int;
    pub fn oneui_toast_host_create(capacity: usize) -> *mut OneUiWidget;
    pub fn oneui_toast_host_push_utf8(
        host: *mut OneUiWidget,
        value: *const OneUiToastMessageUtf8,
        callback: OneUiUtf8TextCallback,
        data: *mut c_void,
        cleanup: OneUiDestroyCallback,
    ) -> c_int;
    pub fn oneui_toast_host_dismiss_utf8(host: *mut OneUiWidget, id: OneUiUtf8String) -> c_int;
    pub fn oneui_toast_host_clear(host: *mut OneUiWidget) -> c_int;
    pub fn oneui_toast_host_set_active(host: *mut OneUiWidget, active: c_int) -> c_int;
    pub fn oneui_toast_host_pending_count(host: *mut OneUiWidget) -> usize;
}
