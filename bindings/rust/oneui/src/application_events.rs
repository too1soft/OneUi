//! Process-wide open-URI events, including Cocoa warm and cold application opens.
#[cfg(target_os = "macos")]
use std::{
    cell::RefCell,
    sync::atomic::{AtomicBool, Ordering},
};
#[cfg(target_os = "macos")]
static SUBSCRIBED: AtomicBool = AtomicBool::new(false);
#[cfg(target_os = "macos")]
type Handler = RefCell<Box<dyn FnMut(String)>>;

/// Retain on the application's main thread. Dropping clears the native callback
/// before destroying its Rust handler. Only one process subscription is allowed.
pub struct OpenUriSubscription {
    #[cfg(target_os = "macos")]
    handler: std::rc::Rc<Handler>,
    _ui_thread: std::marker::PhantomData<std::rc::Rc<()>>,
}
impl OpenUriSubscription {
    pub fn new<F: FnMut(String) + 'static>(handler: F) -> std::io::Result<Self> {
        #[cfg(target_os = "macos")]
        {
            if SUBSCRIBED.swap(true, Ordering::AcqRel) {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::AlreadyExists,
                    "application URI handler already installed",
                ));
            }
            let value = Self {
                handler: std::rc::Rc::new(RefCell::new(Box::new(handler))),
                _ui_thread: std::marker::PhantomData,
            };
            let accepted = unsafe {
                crate::sys::oneui_macos_set_open_uri_callback(
                    Some(receive),
                    std::rc::Rc::as_ptr(&value.handler).cast_mut().cast(),
                )
            };
            if accepted == 0 {
                // Retain the lease until value's Drop has attempted native clear.
                // Releasing early lets another subscription race that cleanup.
                return Err(std::io::Error::other(
                    "application URI handler requires the main thread",
                ));
            }
            Ok(value)
        }
        #[cfg(not(target_os = "macos"))]
        {
            let _ = handler;
            Err(std::io::Error::new(
                std::io::ErrorKind::Unsupported,
                "this desktop delivers URIs through process arguments",
            ))
        }
    }
}
#[cfg(target_os = "macos")]
unsafe extern "C" fn receive(bytes: *const u8, length: usize, data: *mut std::ffi::c_void) {
    if bytes.is_null() || data.is_null() || length > 8192 {
        return;
    }
    let Ok(uri) = std::str::from_utf8(unsafe { std::slice::from_raw_parts(bytes, length) }) else {
        return;
    };
    // A user callback may drop its subscription. Retain the handler until this
    // delivery and its RefCell borrow have both finished.
    unsafe {
        std::rc::Rc::increment_strong_count(data.cast::<Handler>());
    }
    let handler = unsafe { std::rc::Rc::from_raw(data.cast::<Handler>()) };
    if let Ok(mut callback) = handler.try_borrow_mut() {
        crate::run_callback_guarded("application.open_uri", || callback(uri.to_owned()));
    };
}
#[cfg(target_os = "macos")]
impl Drop for OpenUriSubscription {
    fn drop(&mut self) {
        unsafe {
            crate::sys::oneui_macos_set_open_uri_callback(None, std::ptr::null_mut());
        }
        SUBSCRIBED.store(false, Ordering::Release);
    }
}
