//! Application-scoped system notifications. Accepted never means displayed.
use super::{sys, Application, Error};
use std::{
    ffi::c_void,
    marker::PhantomData,
    panic::{catch_unwind, AssertUnwindSafe},
    ptr::NonNull,
    rc::Rc,
};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NotificationStatus {
    Accepted,
    PermissionDenied,
    ServiceUnavailable,
    Unsupported,
    Failed,
}
impl NotificationStatus {
    fn from_raw(value: i32) -> Self {
        match value {
            0 => Self::Accepted,
            1 => Self::PermissionDenied,
            2 => Self::ServiceUnavailable,
            3 => Self::Unsupported,
            _ => Self::Failed,
        }
    }
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NotificationPermission {
    Unknown,
    NotDetermined,
    Granted,
    Denied,
    Unsupported,
}
impl NotificationPermission {
    fn from_raw(value: i32) -> Self {
        match value {
            1 => Self::NotDetermined,
            2 => Self::Granted,
            3 => Self::Denied,
            4 => Self::Unsupported,
            _ => Self::Unknown,
        }
    }
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NotificationResult {
    pub status: NotificationStatus,
    pub id: String,
    pub detail: String,
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct NotificationPermissionResult {
    pub permission: NotificationPermission,
    pub status: NotificationStatus,
    pub detail: String,
}
#[derive(Clone, Debug)]
pub struct NotificationAction {
    pub id: String,
    pub title: String,
}
#[derive(Clone, Debug, Default)]
pub struct Notification {
    pub id: String,
    pub title: String,
    pub body: String,
    pub parameter: String,
    pub actions: Vec<NotificationAction>,
}
pub struct NotificationService {
    raw: NonNull<sys::OneUiNotificationService>,
    _ui_thread: PhantomData<Rc<()>>,
}
type ResultHandler = Option<Box<dyn FnOnce(NotificationResult)>>;
type PermissionHandler = Option<Box<dyn FnOnce(NotificationPermissionResult)>>;
unsafe fn string(value: sys::OneUiUtf8String) -> String {
    if value.length == 0 {
        return String::new();
    }
    String::from_utf8_lossy(std::slice::from_raw_parts(
        value.data.cast::<u8>(),
        value.length,
    ))
    .into_owned()
}
unsafe extern "C" fn destroy_result(data: *mut c_void) {
    drop(Box::from_raw(data.cast::<ResultHandler>()));
}
unsafe extern "C" fn result(
    status: i32,
    id: sys::OneUiUtf8String,
    detail: sys::OneUiUtf8String,
    data: *mut c_void,
) {
    let callback = (&mut *data.cast::<ResultHandler>()).take();
    if let Some(callback) = callback {
        let value = NotificationResult {
            status: NotificationStatus::from_raw(status),
            id: string(id),
            detail: string(detail),
        };
        let _ = catch_unwind(AssertUnwindSafe(|| callback(value)));
    }
}
unsafe extern "C" fn destroy_permission(data: *mut c_void) {
    drop(Box::from_raw(data.cast::<PermissionHandler>()));
}
unsafe extern "C" fn permission(
    allowed: i32,
    status: i32,
    detail: sys::OneUiUtf8String,
    data: *mut c_void,
) {
    let callback = (&mut *data.cast::<PermissionHandler>()).take();
    if let Some(callback) = callback {
        let value = NotificationPermissionResult {
            permission: NotificationPermission::from_raw(allowed),
            status: NotificationStatus::from_raw(status),
            detail: string(detail),
        };
        let _ = catch_unwind(AssertUnwindSafe(|| callback(value)));
    }
}
fn result_data(callback: impl FnOnce(NotificationResult) + 'static) -> *mut c_void {
    Box::into_raw(Box::new(Some(
        Box::new(callback) as Box<dyn FnOnce(NotificationResult)>
    )))
    .cast()
}
impl NotificationService {
    pub fn new(application: &Application) -> Result<Self, Error> {
        let raw = NonNull::new(unsafe {
            sys::oneui_notification_service_create(application.raw.as_ptr())
        })
        .ok_or(Error::ApplicationOperationFailed)?;
        Ok(Self {
            raw,
            _ui_thread: PhantomData,
        })
    }
    /// False rejects immediately and releases the closure. Service destruction
    /// cancels pending callbacks and releases them on the UI thread.
    pub fn query_permission(
        &self,
        callback: impl FnOnce(NotificationPermissionResult) + 'static,
    ) -> bool {
        self.permission(false, callback)
    }
    pub fn request_permission(
        &self,
        callback: impl FnOnce(NotificationPermissionResult) + 'static,
    ) -> bool {
        self.permission(true, callback)
    }
    fn permission(
        &self,
        request: bool,
        callback: impl FnOnce(NotificationPermissionResult) + 'static,
    ) -> bool {
        let data = Box::into_raw(Box::new(Some(
            Box::new(callback) as Box<dyn FnOnce(NotificationPermissionResult)>
        )))
        .cast();
        unsafe {
            sys::oneui_notification_permission(
                self.raw.as_ptr(),
                i32::from(request),
                Some(permission),
                data,
                Some(destroy_permission),
            ) != 0
        }
    }
    pub fn send(
        &self,
        value: &Notification,
        callback: impl FnOnce(NotificationResult) + 'static,
    ) -> bool {
        let actions: Vec<_> = value
            .actions
            .iter()
            .map(|action| sys::OneUiNotificationActionUtf8 {
                id: sys::OneUiUtf8String::from_str(&action.id),
                title: sys::OneUiUtf8String::from_str(&action.title),
            })
            .collect();
        let message = sys::OneUiNotificationUtf8 {
            id: sys::OneUiUtf8String::from_str(&value.id),
            title: sys::OneUiUtf8String::from_str(&value.title),
            body: sys::OneUiUtf8String::from_str(&value.body),
            parameter: sys::OneUiUtf8String::from_str(&value.parameter),
            actions: actions.as_ptr(),
            action_count: actions.len(),
        };
        unsafe {
            sys::oneui_notification_send_utf8(
                self.raw.as_ptr(),
                &message,
                Some(result),
                result_data(callback),
                Some(destroy_result),
            ) != 0
        }
    }
    pub fn withdraw(&self, id: &str, callback: impl FnOnce(NotificationResult) + 'static) -> bool {
        unsafe {
            sys::oneui_notification_withdraw_utf8(
                self.raw.as_ptr(),
                sys::OneUiUtf8String::from_str(id),
                Some(result),
                result_data(callback),
                Some(destroy_result),
            ) != 0
        }
    }
}
impl Drop for NotificationService {
    fn drop(&mut self) {
        unsafe { sys::oneui_notification_service_destroy(self.raw.as_ptr()) };
    }
}
impl Application {
    pub fn is_primary_instance(&self) -> bool {
        unsafe { sys::oneui_application_is_primary(self.raw.as_ptr()) != 0 }
    }
    /// Synchronous installer operation, independent of a running Application.
    pub fn register_desktop(
        id: &str,
        name: &str,
        executable: &str,
        install: bool,
    ) -> Result<NotificationResult, Error> {
        let output = Rc::new(std::cell::RefCell::new(None));
        let target = Rc::clone(&output);
        let accepted = unsafe {
            sys::oneui_application_register_desktop_utf8(
                sys::OneUiUtf8String::from_str(id),
                sys::OneUiUtf8String::from_str(name),
                sys::OneUiUtf8String::from_str(executable),
                i32::from(install),
                Some(result),
                result_data(move |value| *target.borrow_mut() = Some(value)),
                Some(destroy_result),
            )
        };
        if accepted == 0 {
            return Err(Error::ApplicationOperationFailed);
        }
        let result = output
            .borrow_mut()
            .take()
            .ok_or(Error::ApplicationOperationFailed);
        result
    }
}
