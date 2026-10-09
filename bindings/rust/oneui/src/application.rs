//! Application and asynchronous close policy. All handles stay on their UI thread.
use super::*;
use std::ffi::c_void;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum QuitPolicy {
    #[default]
    LastWindowClosed,
    MainWindowClosed,
    Explicit,
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ApplicationActivation {
    pub notification_id: String,
    pub action_id: String,
    pub parameter: String,
}
pub struct Application {
    pub(crate) raw: NonNull<sys::OneUiApplication>,
    _ui_thread: PhantomData<Rc<()>>,
}
pub struct CloseDecision {
    raw: Option<NonNull<sys::OneUiCloseDecision>>,
    _ui_thread: PhantomData<Rc<()>>,
}
impl CloseDecision {
    /// A decision is single-use; dropping it without resolving cancels closing.
    pub fn resolve(mut self, accept: bool) {
        if let Some(raw) = self.raw.take() {
            unsafe { sys::oneui_close_decision_resolve(raw.as_ptr(), i32::from(accept)) };
        }
    }
}
impl Drop for CloseDecision {
    fn drop(&mut self) {
        if let Some(raw) = self.raw.take() {
            unsafe { sys::oneui_close_decision_destroy(raw.as_ptr()) };
        }
    }
}
type Guard = Box<dyn Fn(CloseDecision)>;
type Activation = Box<dyn Fn(ApplicationActivation)>;
unsafe extern "C" fn destroy_guard(data: *mut c_void) {
    drop(Box::from_raw(data.cast::<Guard>()));
}
unsafe extern "C" fn run_guard(decision: *mut sys::OneUiCloseDecision, data: *mut c_void) {
    let decision = CloseDecision {
        raw: NonNull::new(decision),
        _ui_thread: PhantomData,
    };
    let handler = &*data.cast::<Guard>();
    let _ = catch_unwind(AssertUnwindSafe(|| handler(decision)));
}
unsafe extern "C" fn destroy_activation(data: *mut c_void) {
    drop(Box::from_raw(data.cast::<Activation>()));
}
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
unsafe extern "C" fn run_activation(
    id: sys::OneUiUtf8String,
    action: sys::OneUiUtf8String,
    parameter: sys::OneUiUtf8String,
    data: *mut c_void,
) {
    let handler = &*data.cast::<Activation>();
    let value = ApplicationActivation {
        notification_id: string(id),
        action_id: string(action),
        parameter: string(parameter),
    };
    let _ = catch_unwind(AssertUnwindSafe(|| handler(value)));
}
impl Application {
    pub fn new(id: &str, name: &str) -> Result<Self, Error> {
        let actual = unsafe { sys::oneui_utf8_abi_version() };
        if actual != sys::UTF8_ABI_VERSION {
            return Err(Error::AbiVersionMismatch {
                expected: sys::UTF8_ABI_VERSION,
                actual,
            });
        }
        let raw = NonNull::new(unsafe {
            sys::oneui_application_create_utf8(
                sys::OneUiUtf8String::from_str(id),
                sys::OneUiUtf8String::from_str(name),
            )
        })
        .ok_or(Error::ApplicationOperationFailed)?;
        Ok(Self {
            raw,
            _ui_thread: PhantomData,
        })
    }
    pub fn add_window(&self, window: &Window, main: bool) -> bool {
        window
            .state
            .with_raw(|w| unsafe {
                sys::oneui_application_add_window(self.raw.as_ptr(), w, i32::from(main)) != 0
            })
            .unwrap_or(false)
    }
    pub fn remove_window(&self, window: &Window) -> bool {
        window
            .state
            .with_raw(|w| unsafe {
                sys::oneui_application_remove_window(self.raw.as_ptr(), w) != 0
            })
            .unwrap_or(false)
    }
    pub fn set_quit_policy(&self, policy: QuitPolicy) -> bool {
        unsafe { sys::oneui_application_set_quit_policy(self.raw.as_ptr(), policy as i32) != 0 }
    }
    pub fn run(&self) -> i32 {
        unsafe { sys::oneui_application_run(self.raw.as_ptr()) }
    }
    pub fn request_quit(&self) -> bool {
        unsafe { sys::oneui_application_request_quit(self.raw.as_ptr()) != 0 }
    }
    pub fn set_close_guard(
        &self,
        window: &Window,
        callback: impl Fn(CloseDecision) + 'static,
    ) -> bool {
        // Transfer data only after obtaining the live native window.
        window
            .state
            .with_raw(|w| {
                let data = Box::into_raw(Box::new(Box::new(callback) as Guard)).cast();
                unsafe {
                    sys::oneui_application_set_close_guard(
                        self.raw.as_ptr(),
                        w,
                        Some(run_guard),
                        data,
                        Some(destroy_guard),
                    ) != 0
                }
            })
            .unwrap_or(false)
    }
    pub fn clear_close_guard(&self, window: &Window) -> bool {
        window
            .state
            .with_raw(|w| unsafe {
                sys::oneui_application_set_close_guard(
                    self.raw.as_ptr(),
                    w,
                    None,
                    std::ptr::null_mut(),
                    None,
                ) != 0
            })
            .unwrap_or(false)
    }
    pub fn set_on_activation(&self, callback: impl Fn(ApplicationActivation) + 'static) -> bool {
        let data = Box::into_raw(Box::new(Box::new(callback) as Activation)).cast();
        unsafe {
            sys::oneui_application_set_on_activation(
                self.raw.as_ptr(),
                Some(run_activation),
                data,
                Some(destroy_activation),
            ) != 0
        }
    }
}
impl Drop for Application {
    fn drop(&mut self) {
        unsafe { sys::oneui_application_destroy(self.raw.as_ptr()) };
    }
}
