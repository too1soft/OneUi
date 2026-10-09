//! Same-widget transfers between native windows on one UI thread.
use crate::{sys, Application, Panel, StyleSheet, Widget, Window};
use std::{
    ffi::c_void,
    marker::PhantomData,
    panic::{catch_unwind, AssertUnwindSafe},
    ptr::NonNull,
    rc::Rc,
};

pub type DockId = u64;
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DockPosition {
    Tab,
    Left,
    Right,
    Top,
    Bottom,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DockSessionError {
    InvalidPane,
    InvalidHost,
    InvalidTarget,
    Busy,
    StaleTransaction,
    ResourceRejected,
    Failed,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DockTransferPhase {
    Prepare,
    Commit,
    Rollback,
}
/// Addresses are borrowed platform handles for native resource integration.
/// Using them with foreign APIs requires that API's usual unsafe contract.
#[derive(Clone, Copy, Debug)]
pub struct DockTransferEvent {
    pub source_native: usize,
    pub target_native: usize,
    pub phase: DockTransferPhase,
}
#[derive(Clone, Copy, Debug)]
pub struct DockDestination {
    pub host: DockId,
    pub pane: DockId,
    pub position: DockPosition,
    pub index: Option<usize>,
}
impl DockDestination {
    pub fn tabs(host: DockId) -> Self {
        Self {
            host,
            pane: 0,
            position: DockPosition::Tab,
            index: None,
        }
    }
    fn native(self) -> sys::OneUiDockDestination {
        sys::OneUiDockDestination {
            host: self.host,
            pane: self.pane,
            position: self.position as i32,
            index: self
                .index
                .map(|i| i.min(i32::MAX as usize) as i32)
                .unwrap_or(-1),
        }
    }
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct DockPaneLocation {
    pub host: DockId,
    pub group: DockId,
    pub index: usize,
    pub selected: bool,
}
fn result(code: i32) -> Result<(), DockSessionError> {
    match code {
        0 => Ok(()),
        1 => Err(DockSessionError::InvalidPane),
        2 => Err(DockSessionError::InvalidHost),
        3 => Err(DockSessionError::InvalidTarget),
        4 => Err(DockSessionError::Busy),
        5 => Err(DockSessionError::StaleTransaction),
        6 => Err(DockSessionError::ResourceRejected),
        _ => Err(DockSessionError::Failed),
    }
}
struct PaneOwner<T> {
    _owner: Rc<T>,
    transfer: Box<dyn Fn(DockTransferEvent) -> bool>,
}
unsafe extern "C" fn destroy_owner<T>(data: *mut c_void) {
    drop(Box::from_raw(data.cast::<PaneOwner<T>>()));
}
unsafe extern "C" fn transfer<T>(
    source: *mut c_void,
    target: *mut c_void,
    phase: i32,
    data: *mut c_void,
) -> i32 {
    let owner = &*data.cast::<PaneOwner<T>>();
    let phase = match phase {
        0 => DockTransferPhase::Prepare,
        1 => DockTransferPhase::Commit,
        _ => DockTransferPhase::Rollback,
    };
    i32::from(
        catch_unwind(AssertUnwindSafe(|| {
            (owner.transfer)(DockTransferEvent {
                source_native: source as usize,
                target_native: target as usize,
                phase,
            })
        }))
        .unwrap_or(false),
    )
}
type Changed = Box<dyn Fn()>;
unsafe extern "C" fn destroy_surface(data: *mut c_void) {
    drop(Box::from_raw(data.cast::<Rc<Panel>>()));
}
unsafe extern "C" fn destroy_changed(data: *mut c_void) {
    drop(Box::from_raw(data.cast::<Changed>()));
}
unsafe extern "C" fn run_changed(data: *mut c_void) {
    let callback = &*data.cast::<Changed>();
    let _ = catch_unwind(AssertUnwindSafe(callback));
}
unsafe extern "C" fn collect_ids(ids: *const u64, length: usize, data: *mut c_void) {
    let output = &mut *data.cast::<Vec<DockId>>();
    if length != 0 {
        output.extend_from_slice(std::slice::from_raw_parts(ids, length));
    }
}
/// Keeps every registered Rust controller alive in the native pane owner. Its
/// `Application` borrow prevents teardown of the shared event loop too early.
pub struct DockSession<'app> {
    raw: NonNull<sys::OneUiDockSession>,
    _application: PhantomData<&'app Application>,
    _ui_thread: PhantomData<Rc<()>>,
}
impl<'app> DockSession<'app> {
    pub fn native_drag_supported(&self, host: DockId) -> bool {
        unsafe { sys::oneui_dock_session_native_drag_supported(self.raw.as_ptr(), host) != 0 }
    }
    /// Call from the initiating pointer event. False means the native drag did
    /// not start; no pane is moved until the OS delivers an accepted drop.
    pub fn start_native_drag(&self, pane: DockId) -> bool {
        unsafe { sys::oneui_dock_session_start_native_drag(self.raw.as_ptr(), pane) != 0 }
    }
    pub fn new(app: &'app Application) -> Result<Self, DockSessionError> {
        let raw = NonNull::new(unsafe { sys::oneui_dock_session_create(app.raw.as_ptr()) })
            .ok_or(DockSessionError::Failed)?;
        Ok(Self {
            raw,
            _application: PhantomData,
            _ui_thread: PhantomData,
        })
    }
    pub fn attach_host(
        &self,
        window: &Window,
        surface: Rc<Panel>,
    ) -> Result<DockId, DockSessionError> {
        let id = window
            .state
            .with_raw(|window| unsafe {
                let raw = surface.as_widget().as_raw();
                let owner = Box::into_raw(Box::new(surface)).cast();
                sys::oneui_dock_session_attach_host(
                    self.raw.as_ptr(),
                    window,
                    raw,
                    owner,
                    Some(destroy_surface),
                )
            })
            .unwrap_or(0);
        if id == 0 {
            return Err(DockSessionError::InvalidHost);
        }
        Ok(id)
    }
    /// The accessor must return a widget owned by `owner`. Both remain alive
    /// through reparenting, orphaning, and destruction of the original window.
    pub fn add_pane<T: 'static>(
        &self,
        id: DockId,
        title: &str,
        owner: Rc<T>,
        widget: impl FnOnce(&T) -> &Widget,
        destination: DockDestination,
    ) -> Result<(), DockSessionError> {
        self.add_pane_with_transfer(id, title, owner, widget, (120.0, 80.0), destination, |_| {
            true
        })
    }
    pub fn add_pane_with_transfer<T: 'static>(
        &self,
        id: DockId,
        title: &str,
        owner: Rc<T>,
        widget: impl FnOnce(&T) -> &Widget,
        minimum: (f32, f32),
        destination: DockDestination,
        callback: impl Fn(DockTransferEvent) -> bool + 'static,
    ) -> Result<(), DockSessionError> {
        let content = widget(&owner).as_raw();
        let data = Box::into_raw(Box::new(PaneOwner {
            _owner: owner,
            transfer: Box::new(callback),
        }))
        .cast();
        result(unsafe {
            sys::oneui_dock_session_add_pane_utf8(
                self.raw.as_ptr(),
                id,
                sys::OneUiUtf8String::from_str(title),
                content,
                minimum.0,
                minimum.1,
                destination.native(),
                Some(transfer::<T>),
                data,
                Some(destroy_owner::<T>),
            )
        })
    }
    pub fn remove_pane(&self, id: DockId) -> Result<(), DockSessionError> {
        result(unsafe { sys::oneui_dock_session_remove_pane(self.raw.as_ptr(), id) })
    }
    pub fn move_pane(
        &self,
        id: DockId,
        destination: DockDestination,
    ) -> Result<(), DockSessionError> {
        result(unsafe {
            sys::oneui_dock_session_move_pane(self.raw.as_ptr(), id, destination.native())
        })
    }
    pub fn select_pane(&self, id: DockId) -> Result<(), DockSessionError> {
        result(unsafe { sys::oneui_dock_session_select_pane(self.raw.as_ptr(), id) })
    }
    /// The SDK retains this sheet and refreshes all native hosts, including
    /// existing floating windows. Call again after replacing its CSS.
    pub fn set_style_sheet(&self, sheet: &StyleSheet) -> Result<(), DockSessionError> {
        result(unsafe {
            sys::oneui_dock_session_set_style_sheet(self.raw.as_ptr(), sheet.as_raw())
        })
    }
    pub fn detach_pane(
        &self,
        id: DockId,
        title: &str,
        width: i32,
        height: i32,
    ) -> Result<DockId, DockSessionError> {
        let mut host = 0;
        result(unsafe {
            sys::oneui_dock_session_detach_pane_utf8(
                self.raw.as_ptr(),
                id,
                sys::OneUiUtf8String::from_str(title),
                width,
                height,
                &mut host,
            )
        })?;
        Ok(host)
    }
    pub fn begin_drag(&self, id: DockId) -> Result<DockDrag<'_, 'app>, DockSessionError> {
        let token = unsafe { sys::oneui_dock_session_begin_drag(self.raw.as_ptr(), id) };
        if token == 0 {
            return Err(DockSessionError::InvalidPane);
        }
        Ok(DockDrag {
            session: self,
            token,
        })
    }
    pub fn location(&self, id: DockId) -> Option<DockPaneLocation> {
        let p = unsafe { sys::oneui_dock_session_location(self.raw.as_ptr(), id) };
        if p.host == 0 || p.index < 0 {
            return None;
        }
        Some(DockPaneLocation {
            host: p.host,
            group: p.group,
            index: p.index as usize,
            selected: p.selected != 0,
        })
    }
    pub fn panes(&self, host: Option<DockId>) -> Vec<DockId> {
        let mut ids = Vec::new();
        unsafe {
            sys::oneui_dock_session_panes(
                self.raw.as_ptr(),
                host.unwrap_or(0),
                Some(collect_ids),
                (&mut ids as *mut Vec<DockId>).cast(),
            );
        }
        ids
    }
    pub fn hosts(&self) -> Vec<DockId> {
        let mut ids = Vec::new();
        unsafe {
            sys::oneui_dock_session_hosts(
                self.raw.as_ptr(),
                Some(collect_ids),
                (&mut ids as *mut Vec<DockId>).cast(),
            );
        }
        ids
    }
    /// Empty when the backend reports no native drag restriction.
    pub fn native_drag_limitation(&self, host: DockId) -> String {
        unsafe extern "C" fn collect(
            text: *const std::ffi::c_char,
            length: usize,
            data: *mut c_void,
        ) {
            let output = unsafe { &mut *data.cast::<String>() };
            if !text.is_null() {
                *output = String::from_utf8_lossy(unsafe {
                    std::slice::from_raw_parts(text.cast::<u8>(), length)
                })
                .into_owned();
            }
        }
        let mut output = String::new();
        unsafe {
            sys::oneui_dock_session_native_drag_limitation_utf8(
                self.raw.as_ptr(),
                host,
                Some(collect),
                (&mut output as *mut String).cast(),
            );
        }
        output
    }
    pub fn set_on_changed(&self, callback: impl Fn() + 'static) -> bool {
        let data = Box::into_raw(Box::new(Box::new(callback) as Changed)).cast();
        unsafe {
            sys::oneui_dock_session_set_on_changed(
                self.raw.as_ptr(),
                Some(run_changed),
                data,
                Some(destroy_changed),
            ) != 0
        }
    }
    pub fn clear_on_changed(&self) {
        unsafe {
            sys::oneui_dock_session_set_on_changed(
                self.raw.as_ptr(),
                None,
                std::ptr::null_mut(),
                None,
            );
        }
    }
}
impl Drop for DockSession<'_> {
    fn drop(&mut self) {
        unsafe {
            sys::oneui_dock_session_destroy(self.raw.as_ptr());
        }
    }
}
/// Dropping a pending drag cancels it without altering any pane or window.
pub struct DockDrag<'session, 'app> {
    session: &'session DockSession<'app>,
    token: u64,
}
impl DockDrag<'_, '_> {
    pub fn commit(mut self, destination: DockDestination) -> Result<(), DockSessionError> {
        let token = std::mem::take(&mut self.token);
        result(unsafe {
            sys::oneui_dock_session_commit_drag(
                self.session.raw.as_ptr(),
                token,
                destination.native(),
            )
        })
    }
}
impl Drop for DockDrag<'_, '_> {
    fn drop(&mut self) {
        if self.token != 0 {
            unsafe {
                sys::oneui_dock_session_cancel_drag(self.session.raw.as_ptr(), self.token);
            }
        }
    }
}
