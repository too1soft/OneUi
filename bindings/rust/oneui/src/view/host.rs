use super::{check, View, VmResult};
use crate::{sys, Widget};
use std::{ffi::c_void, marker::PhantomData, ptr::NonNull, rc::Rc};

/// Borrowed host capability. Cannot outlive the View or overlap mutable reload.
pub struct HostRef<'view> {
    pub(crate) raw: NonNull<sys::OneUiWidget>,
    pub(crate) _view: PhantomData<&'view View>,
}
unsafe extern "C" fn retain<T>(raw: *mut c_void) {
    super::super::run_callback_guarded("view.host.retain", || {
        Rc::increment_strong_count(raw.cast::<T>())
    });
}
unsafe extern "C" fn release<T>(raw: *mut c_void) {
    super::super::run_callback_guarded("view.host.release", || drop(Rc::from_raw(raw.cast::<T>())));
}
impl HostRef<'_> {
    /// Keeps the complete Rust callback owner alive, not just its native widget.
    pub fn attach<T: 'static>(&self, owner: Rc<T>, widget: fn(&T) -> &Widget) -> VmResult {
        check(unsafe {
            sys::oneui_view_host_attach(
                self.raw.as_ptr(),
                widget(&owner).as_raw(),
                Rc::as_ptr(&owner).cast_mut().cast(),
                Some(retain::<T>),
                Some(release::<T>),
            )
        })
    }
    pub fn detach(&self) -> VmResult {
        check(unsafe { sys::oneui_view_host_detach(self.raw.as_ptr()) })
    }
}
