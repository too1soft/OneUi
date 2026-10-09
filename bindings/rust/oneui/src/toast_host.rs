//! Bounded application Toast queue, independent of system notifications.
use super::{sys, Error, Widget};
use std::{
    ffi::{c_char, c_void},
    panic::{catch_unwind, AssertUnwindSafe},
};

#[derive(Clone, Debug)]
pub struct ToastMessage {
    pub id: String,
    pub title: String,
    pub message: String,
    pub primary_action: String,
    pub secondary_action: String,
    pub timeout_ms: f64,
}
impl Default for ToastMessage {
    fn default() -> Self {
        Self {
            id: String::new(),
            title: String::new(),
            message: String::new(),
            primary_action: String::new(),
            secondary_action: String::new(),
            timeout_ms: 5000.0,
        }
    }
}
pub struct ToastHost {
    widget: Widget,
}
type Action = Box<dyn Fn(&str)>;
unsafe extern "C" fn destroy(data: *mut c_void) {
    drop(Box::from_raw(data.cast::<Action>()));
}
unsafe extern "C" fn action(text: *const c_char, size: usize, data: *mut c_void) {
    let text = if size == 0 {
        String::new()
    } else {
        String::from_utf8_lossy(std::slice::from_raw_parts(text.cast::<u8>(), size)).into_owned()
    };
    let callback = &*data.cast::<Action>();
    let _ = catch_unwind(AssertUnwindSafe(|| callback(&text)));
}
impl ToastHost {
    pub fn new(capacity: usize) -> Result<Self, Error> {
        Ok(Self {
            widget: Widget::from_raw(unsafe { sys::oneui_toast_host_create(capacity) })?,
        })
    }
    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
    pub fn push(&self, message: &ToastMessage, callback: impl Fn(&str) + 'static) -> bool {
        let value = sys::OneUiToastMessageUtf8 {
            id: sys::OneUiUtf8String::from_str(&message.id),
            title: sys::OneUiUtf8String::from_str(&message.title),
            message: sys::OneUiUtf8String::from_str(&message.message),
            primary_action: sys::OneUiUtf8String::from_str(&message.primary_action),
            secondary_action: sys::OneUiUtf8String::from_str(&message.secondary_action),
            timeout_ms: message.timeout_ms,
        };
        let data = Box::into_raw(Box::new(Box::new(callback) as Action)).cast();
        unsafe {
            sys::oneui_toast_host_push_utf8(
                self.widget.as_raw(),
                &value,
                Some(action),
                data,
                Some(destroy),
            ) != 0
        }
    }
    pub fn dismiss(&self, id: &str) -> bool {
        unsafe {
            sys::oneui_toast_host_dismiss_utf8(
                self.widget.as_raw(),
                sys::OneUiUtf8String::from_str(id),
            ) != 0
        }
    }
    pub fn clear(&self) {
        unsafe { sys::oneui_toast_host_clear(self.widget.as_raw()) };
    }
    pub fn set_active(&self, active: bool) {
        unsafe { sys::oneui_toast_host_set_active(self.widget.as_raw(), i32::from(active)) };
    }
    pub fn pending_count(&self) -> usize {
        unsafe { sys::oneui_toast_host_pending_count(self.widget.as_raw()) }
    }
}
