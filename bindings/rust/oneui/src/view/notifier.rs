use super::{ListChange, PropertyId, ViewError, VmResult};
use crate::sys;
use std::{marker::PhantomData, ptr::NonNull, rc::Rc};

/// Weak native control, owned notifier reference. Deliberately UI-thread only.
pub struct ViewNotifier {
    pub(crate) raw: NonNull<sys::OneUiViewNotifier>,
    _ui: PhantomData<Rc<()>>,
}
impl ViewNotifier {
    pub(crate) unsafe fn owned(raw: *mut sys::OneUiViewNotifier) -> VmResult<Self> {
        Ok(Self {
            raw: NonNull::new(raw).ok_or(ViewError::CLOSED)?,
            _ui: PhantomData,
        })
    }
    pub(crate) unsafe fn borrowed(raw: *mut sys::OneUiViewNotifier) -> VmResult<Self> {
        let value = Self::owned(raw)?;
        sys::oneui_view_notifier_retain(raw);
        Ok(value)
    }
    pub fn notify(&self, property: PropertyId) -> VmResult {
        #[cfg(feature = "view-diagnostics")]
        super::diagnostics::record(|s| s.notifies += 1);
        super::check(unsafe { sys::oneui_view_notifier_notify(self.raw.as_ptr(), property.get()) })
    }
    pub fn notify_list(&self, property: PropertyId, change: &ListChange) -> VmResult {
        #[cfg(feature = "view-diagnostics")]
        super::diagnostics::record(|s| s.notifies += 1);
        change.with_raw(|raw| {
            super::check(unsafe {
                sys::oneui_view_notifier_notify_list(self.raw.as_ptr(), property.get(), raw)
            })
        })
    }
}
impl Clone for ViewNotifier {
    fn clone(&self) -> Self {
        unsafe { Self::borrowed(self.raw.as_ptr()).unwrap() }
    }
}
impl Drop for ViewNotifier {
    fn drop(&mut self) {
        unsafe { sys::oneui_view_notifier_release(self.raw.as_ptr()) };
    }
}

/// The current scope's connections, never an owning VM reference.
#[derive(Clone, Default)]
pub struct VmContext {
    pub(crate) notifiers: Vec<ViewNotifier>,
}
impl VmContext {
    /// Compare connection identity without exposing native handles.
    /// Useful for removing exactly the connection passed to on_view_disconnect.
    pub fn same_connections(&self, other: &Self) -> bool {
        self.notifiers.len() == other.notifiers.len()
            && self
                .notifiers
                .iter()
                .zip(&other.notifiers)
                .all(|(a, b)| a.raw == b.raw)
    }
    pub fn notify(&self, property: PropertyId) -> VmResult {
        for notifier in &self.notifiers {
            notifier.notify(property)?;
        }
        Ok(())
    }
    pub fn notify_list(&self, property: PropertyId, change: &ListChange) -> VmResult {
        for notifier in &self.notifiers {
            notifier.notify_list(property, change)?;
        }
        Ok(())
    }
}
