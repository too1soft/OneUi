//! Optional local terminal interactions, independent of remote mouse reporting.
use super::{run_callback_guarded, sys, TerminalPointerEvent, TerminalView};

pub(super) struct LocalPointerCallback {
    handler: Box<dyn FnMut(TerminalPointerEvent) -> bool>,
}

unsafe extern "C" fn invoke(
    event: *const sys::OneUiTerminalPointerEvent,
    user_data: *mut std::ffi::c_void,
) -> std::ffi::c_int {
    if event.is_null() || user_data.is_null() {
        return 0;
    }
    let event = TerminalPointerEvent::from(unsafe { *event });
    let callback = unsafe { &mut *user_data.cast::<LocalPointerCallback>() };
    let mut consumed = false;
    run_callback_guarded("terminal.local_pointer", || {
        consumed = (callback.handler)(event)
    });
    i32::from(consumed)
}

impl TerminalView {
    /// Observe hover and intercept left presses before native text selection.
    /// Return true only for an action owned by the local overlay. Other input
    /// keeps the standard selection and remote mouse path. Outside hover uses
    /// `u16::MAX` for both row and column. Callbacks stay on the UI thread.
    pub fn set_on_local_pointer<F>(&mut self, callback: F)
    where
        F: FnMut(TerminalPointerEvent) -> bool + 'static,
    {
        self.clear_local_pointer_callback();
        self.local_pointer_callback = Some(Box::new(LocalPointerCallback {
            handler: Box::new(callback),
        }));
        let data = (self.local_pointer_callback.as_deref_mut().unwrap()
            as *mut LocalPointerCallback)
            .cast();
        unsafe {
            sys::oneui_terminal_view_set_on_local_pointer(self.widget.as_raw(), Some(invoke), data)
        };
    }

    pub fn clear_local_pointer_callback(&mut self) {
        unsafe {
            sys::oneui_terminal_view_set_on_local_pointer(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.local_pointer_callback = None;
    }
}
