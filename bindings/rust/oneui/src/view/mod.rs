//! Typed language-neutral views. All owners and notifiers are UI-thread only.
#[cfg(test)]
pub(crate) mod adapter_tests;
mod callbacks;
#[cfg(feature = "view-diagnostics")]
pub mod diagnostics;
mod host;
mod mount_point;
mod metrics;
pub use metrics::{DockAction, ElementMetrics};
mod scope;
pub use scope::ScopeSegment;
mod list;
mod model;
mod notifier;
mod reload;
mod value;
pub use crate::define_view_adapter;
use crate::{sys, Widget, Window};
pub use host::HostRef;
pub use list::{ListChange, StableList};
pub use model::{ChildModel, ModelSpec, ViewSpec};
pub use notifier::{ViewNotifier, VmContext};
pub use reload::{ReloadError, ViewReloadWatcher};
use std::{cell::RefCell, marker::PhantomData, ptr::NonNull, rc::Rc};
pub use value::Value;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct PropertyId(u32);
impl PropertyId {
    pub const ALL: Self = Self(0);
    pub const fn new(value: u32) -> Self {
        Self(value)
    }
    pub const fn get(self) -> u32 {
        self.0
    }
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ViewError(i32);
impl ViewError {
    pub const INVALID_ARGUMENT: Self = Self(-1);
    pub const WRONG_THREAD: Self = Self(-2);
    pub const BAD_IR: Self = Self(-3);
    pub const VERSION: Self = Self(-4);
    pub const SCHEMA_MISMATCH: Self = Self(-5);
    pub const UNKNOWN_ID: Self = Self(-6);
    pub const TYPE: Self = Self(-7);
    pub const READ_ONLY: Self = Self(-8);
    pub const CLOSED: Self = Self(-9);
    pub const REENTRANT: Self = Self(-10);
    pub const BAD_KEY: Self = Self(-11);
    pub const STALE_CHANGE: Self = Self(-12);
    pub const FOREIGN_FAILURE: Self = Self(-13);
    pub const OUT_OF_MEMORY: Self = Self(-14);
    pub const LIMIT: Self = Self(-15);
    pub const NOT_FOUND: Self = Self(-16);
    pub const BUSY: Self = Self(-17);
    pub fn code(self) -> i32 {
        self.0
    }
}
impl std::fmt::Display for ViewError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "OneUI view error {}", self.0)
    }
}
impl std::error::Error for ViewError {}
pub type VmResult<T = ()> = Result<T, ViewError>;
pub(crate) fn check(code: i32) -> VmResult {
    if code == 0 {
        Ok(())
    } else {
        Err(ViewError(code))
    }
}

pub struct ViewMount {
    raw: NonNull<sys::OneUiViewMount>,
    _ui: PhantomData<Rc<()>>,
}
impl ViewMount {
    /// Empty attachment boundary for a routed declaration. Visible controls live in the view.
    pub fn destination() -> Result<crate::Panel, crate::Error> {
        crate::Panel::new()
    }

    /// Window-level overlay boundary that owns the routed destination and
    /// hosts popups and dialogs. Visible controls still live in views.
    pub fn overlay_boundary() -> Result<crate::OverlayHost, crate::Error> {
        crate::OverlayHost::new()
    }

    /// Compose an independent declarative VM in an empty named Surface.
    /// The returned mount closes before the parent view and cannot accept
    /// arbitrary native controls. Instantiate a typed View in it as usual.
    pub fn child(&self, name: &str) -> VmResult<Self> {
        let mut raw = std::ptr::null_mut();
        check(unsafe { sys::oneui_view_mount_child(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), &mut raw) })?;
        Ok(Self { raw: NonNull::new(raw).ok_or(ViewError::CLOSED)?, _ui: PhantomData })
    }
    /// Attach an empty, unattached Panel to an empty named Surface and mount
    /// the child View inside that Panel. Success first binds it to this window;
    /// no Rust borrow is retained. Attached Panels and duplicate mounts fail.
    /// Dropping the child View empties the Panel for reuse. Dropping this mount,
    /// or closing/reloading its parent View, also removes the Panel.
    pub fn child_with_destination(&self, name: &str, destination: &Widget) -> VmResult<Self> {
        let mut raw = std::ptr::null_mut();
        check(unsafe {
            sys::oneui_view_mount_child_with_destination(
                self.raw.as_ptr(),
                sys::OneUiUtf8String::from_str(name),
                destination.as_raw(),
                &mut raw,
            )
        })?;
        Ok(Self {
            raw: NonNull::new(raw).ok_or(ViewError::CLOSED)?,
            _ui: PhantomData,
        })
    }
    /// Mount in an empty window, or an empty Panel already in that window.
    /// The native control invalidates on window close; no Rust borrow is retained.
    pub fn new(window: &Window, destination: Option<&Widget>) -> VmResult<Self> {
        Self::from_dispatcher(&window.dispatcher(), destination)
    }
    /// Mount from an application's retained dispatcher on its owning UI thread.
    /// Destination must still be an empty Panel attached to that window.
    pub fn from_dispatcher(dispatcher: &crate::UiDispatcher, destination: Option<&Widget>) -> VmResult<Self> {
        if std::thread::current().id() != dispatcher.state.ui_thread {
            return Err(ViewError::WRONG_THREAD);
        }
        let mut raw = std::ptr::null_mut();
        let code = dispatcher
            .state
            .with_raw(|w| unsafe {
                sys::oneui_view_mount_create(
                    w,
                    destination.map_or(std::ptr::null_mut(), Widget::as_raw),
                    &mut raw,
                )
            })
            .ok_or(ViewError::CLOSED)?;
        check(code)?;
        Ok(Self {
            raw: NonNull::new(raw).ok_or(ViewError::CLOSED)?,
            _ui: PhantomData,
        })
    }
    pub fn set_styles(&self, css: &str, compact: bool) -> VmResult {
        check(unsafe {
            sys::oneui_view_mount_set_styles(
                self.raw.as_ptr(),
                sys::OneUiUtf8String::from_str(css),
                u32::from(compact),
            )
        })
    }
}
impl Drop for ViewMount {
    fn drop(&mut self) {
        unsafe { sys::oneui_view_mount_destroy(self.raw.as_ptr()) };
    }
}

pub struct View {
    pub(crate) raw: NonNull<sys::OneUiView>,
    _ui: PhantomData<Rc<()>>,
}
impl View {
    pub fn instantiate<V: 'static>(
        mount: &ViewMount,
        spec: ViewSpec<V>,
        vm: Rc<RefCell<V>>,
    ) -> VmResult<Self> {
        let owner = spec.model.bind(vm).0;
        let reference = model::Control::borrowed(&owner);
        let mut raw = std::ptr::null_mut();
        check(unsafe {
            sys::oneui_view_instantiate(
                mount.raw.as_ptr(),
                spec.ir.as_ptr(),
                spec.ir.len(),
                reference.vtable,
                reference.vm,
                &mut raw,
            )
        })?;
        Ok(Self {
            raw: NonNull::new(raw).ok_or(ViewError::CLOSED)?,
            _ui: PhantomData,
        })
    }
    /// A retained weak notifier remains callable after close and returns CLOSED.
    pub fn notifier(&self) -> VmResult<ViewNotifier> {
        let mut raw = std::ptr::null_mut();
        check(unsafe { sys::oneui_view_notifier(self.raw.as_ptr(), &mut raw) })?;
        unsafe { ViewNotifier::owned(raw) }
    }
    pub fn flush(&self) -> VmResult {
        check(unsafe { sys::oneui_view_flush(self.raw.as_ptr()) })
    }
    /// Replace the IR while preserving the VM. Exclusive borrow prevents HostRef
    /// use across a successful structural reload.
    pub fn reload(&mut self, ir: &[u8]) -> VmResult {
        check(unsafe { sys::oneui_view_reload(self.raw.as_ptr(), ir.as_ptr(), ir.len()) })
    }
    pub fn disconnect(&mut self) -> VmResult {
        check(unsafe { sys::oneui_view_disconnect(self.raw.as_ptr()) })
    }
    pub fn find_host(&self, name: &str) -> VmResult<HostRef<'_>> {
        let mut raw = std::ptr::null_mut();
        check(unsafe {
            sys::oneui_view_find_host(
                self.raw.as_ptr(),
                std::ptr::null_mut(),
                sys::OneUiUtf8String::from_str(name),
                &mut raw,
            )
        })?;
        Ok(HostRef {
            raw: NonNull::new(raw).ok_or(ViewError::NOT_FOUND)?,
            _view: PhantomData,
        })
    }
    pub fn last_error(&self) -> VmResult<Option<ViewDiagnostic>> {
        let mut raw: sys::OneUiViewError = unsafe { std::mem::zeroed() };
        raw.struct_size = std::mem::size_of::<sys::OneUiViewError>() as u32;
        check(unsafe { sys::oneui_view_get_error(self.raw.as_ptr(), &mut raw) })?;
        if raw.result == 0 {
            return Ok(None);
        }
        let message = unsafe { std::ffi::CStr::from_ptr(raw.message.as_ptr()) }
            .to_string_lossy()
            .into_owned();
        Ok(Some(ViewDiagnostic {
            error: ViewError(raw.result),
            sequence: raw.sequence,
            section: raw.section_kind,
            record: raw.record_index,
            source: raw.source_map_index,
            message,
        }))
    }
}
impl Drop for View {
    fn drop(&mut self) {
        unsafe { sys::oneui_view_destroy(self.raw.as_ptr()) };
    }
}
#[derive(Clone, Debug)]
pub struct ViewDiagnostic {
    pub error: ViewError,
    pub sequence: u64,
    pub section: u32,
    pub record: u32,
    pub source: u32,
    pub message: String,
}

mod workspace;
