//! Mobile foundation, separate from the desktop ABI. No renderer or OS services
//! are implied. Native hosts drive lifecycle, surface availability and frame time.
//! A Runtime is UI-thread confined and cannot be sent or shared across threads.
mod ffi;
pub use ffi::{Frame, Viewport};
use std::{ffi::c_void, marker::PhantomData, ptr::NonNull, rc::Rc};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Error {
    InvalidArgument,
    InvalidState,
    WrongThread,
    Capacity,
    Failed,
    AbiMismatch,
    UnexpectedResult(i32),
}
impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{self:?}")
    }
}
impl std::error::Error for Error {}
pub type Result<T> = std::result::Result<T, Error>;
fn checked(code: i32) -> Result<()> {
    match code {
        0 => Ok(()),
        2 => Err(Error::InvalidArgument),
        3 => Err(Error::InvalidState),
        4 => Err(Error::WrongThread),
        5 => Err(Error::Capacity),
        6 => Err(Error::Failed),
        other => Err(Error::UnexpectedResult(other)),
    }
}
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Lifecycle {
    Created = 0,
    Active = 1,
    Inactive = 2,
    Background = 3,
    Destroyed = 4,
}
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Phase {
    Down = 0,
    Move = 1,
    Up = 2,
    Cancel = 3,
}
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PointerKind {
    Touch = 0,
    Pen = 1,
}
#[derive(Debug, Clone, Copy)]
pub struct Pointer {
    pub id: i32,
    pub phase: Phase,
    pub kind: PointerKind,
    /// Physical pixels on push, logical pixels on poll.
    pub x: f32,
    pub y: f32,
    pub pressure: f32,
    pub time_ns: u64,
    /// Output only; the runtime chooses the primary pointer.
    pub primary: bool,
}
#[derive(Debug, Clone, Copy)]
pub struct State {
    pub lifecycle: Lifecycle,
    pub surface_available: bool,
    pub needs_frame: bool,
    pub active_pointers: u32,
    pub surface_generation: u64,
}

/// Owns the C runtime and destroys it exactly once on the creating thread.
///
/// ```compile_fail
/// let runtime = oneui_mobile::Runtime::new().unwrap();
/// std::thread::spawn(move || drop(runtime));
/// ```
pub struct Runtime {
    handle: NonNull<c_void>,
    _ui_thread: PhantomData<Rc<()>>,
}
impl Runtime {
    pub fn new() -> Result<Self> {
        // SAFETY: ABI query has no arguments or lifetime requirements.
        if unsafe { ffi::oneui_mobile_abi_version() } != 1 {
            return Err(Error::AbiMismatch);
        }
        let mut raw = std::ptr::null_mut();
        // SAFETY: out pointer is live; ownership passes to this wrapper on success.
        checked(unsafe { ffi::oneui_mobile_create(&mut raw) })?;
        Ok(Self {
            handle: NonNull::new(raw).ok_or(Error::Failed)?,
            _ui_thread: PhantomData,
        })
    }
    pub fn set_lifecycle(&mut self, state: Lifecycle) -> Result<()> {
        // SAFETY: unique live handle, thread-confined wrapper, validated enum.
        checked(unsafe { ffi::oneui_mobile_set_lifecycle(self.handle.as_ptr(), state as i32) })
    }
    pub fn set_viewport(&mut self, value: Viewport) -> Result<()> {
        // SAFETY: V1 repr(C) record remains live throughout the synchronous call.
        checked(unsafe { ffi::oneui_mobile_set_viewport_v1(self.handle.as_ptr(), &value) })
    }
    pub fn set_surface(&mut self, available: bool) -> Result<()> {
        // SAFETY: live thread-confined handle and canonical boolean.
        checked(unsafe { ffi::oneui_mobile_set_surface(self.handle.as_ptr(), available as u32) })
    }
    pub fn invalidate(&mut self) -> Result<()> {
        // SAFETY: handle lives until Drop and cannot leave this UI thread.
        checked(unsafe { ffi::oneui_mobile_invalidate(self.handle.as_ptr()) })
    }
    pub fn set_animating(&mut self, enabled: bool) -> Result<()> {
        // SAFETY: live thread-confined handle and canonical boolean.
        checked(unsafe { ffi::oneui_mobile_set_animating(self.handle.as_ptr(), enabled as u32) })
    }
    pub fn push_pointer(&mut self, pointer: Pointer) -> Result<()> {
        let raw = ffi::RawPointer {
            id: pointer.id,
            phase: pointer.phase as i32,
            kind: pointer.kind as i32,
            x: pointer.x,
            y: pointer.y,
            pressure: pointer.pressure,
            time_ns: pointer.time_ns,
            primary: 0,
        };
        // SAFETY: no retained pointer; runtime validates input values and state.
        checked(unsafe { ffi::oneui_mobile_push_pointer_v1(self.handle.as_ptr(), &raw) })
    }
    pub fn cancel_pointers(&mut self) -> Result<()> {
        // SAFETY: handle lives until Drop and cannot leave this UI thread.
        checked(unsafe { ffi::oneui_mobile_cancel_pointers(self.handle.as_ptr()) })
    }
    pub fn poll_pointer(&mut self) -> Result<Option<Pointer>> {
        let mut raw = ffi::RawPointer::default();
        // SAFETY: writable correctly aligned V1 record, not retained by C.
        let code = unsafe { ffi::oneui_mobile_poll_pointer_v1(self.handle.as_ptr(), &mut raw) };
        if code == 1 {
            return Ok(None);
        }
        checked(code)?;
        let phase = match raw.phase {
            0 => Phase::Down,
            1 => Phase::Move,
            2 => Phase::Up,
            3 => Phase::Cancel,
            _ => return Err(Error::AbiMismatch),
        };
        let kind = match raw.kind {
            0 => PointerKind::Touch,
            1 => PointerKind::Pen,
            _ => return Err(Error::AbiMismatch),
        };
        Ok(Some(Pointer {
            id: raw.id,
            phase,
            kind,
            x: raw.x,
            y: raw.y,
            pressure: raw.pressure,
            time_ns: raw.time_ns,
            primary: raw.primary != 0,
        }))
    }
    pub fn take_frame(&mut self, time_ns: u64) -> Result<Option<Frame>> {
        let mut value = Frame::default();
        // SAFETY: writable V1 record and live thread-confined handle.
        let code =
            unsafe { ffi::oneui_mobile_take_frame_v1(self.handle.as_ptr(), time_ns, &mut value) };
        if code == 1 {
            return Ok(None);
        }
        checked(code)?;
        Ok(Some(value))
    }
    pub fn state(&self) -> Result<State> {
        let mut raw = ffi::RawState::default();
        // SAFETY: state query does not mutate runtime, output is live and aligned.
        checked(unsafe { ffi::oneui_mobile_get_state_v1(self.handle.as_ptr(), &mut raw) })?;
        let lifecycle = match raw.lifecycle {
            0 => Lifecycle::Created,
            1 => Lifecycle::Active,
            2 => Lifecycle::Inactive,
            3 => Lifecycle::Background,
            4 => Lifecycle::Destroyed,
            _ => return Err(Error::AbiMismatch),
        };
        Ok(State {
            lifecycle,
            surface_available: raw.surface_available != 0,
            needs_frame: raw.needs_frame != 0,
            active_pointers: raw.active_pointers,
            surface_generation: raw.surface_generation,
        })
    }
}
impl Drop for Runtime {
    fn drop(&mut self) {
        // SAFETY: unique handle, !Send/!Sync guarantees destruction on its UI thread.
        let code = unsafe { ffi::oneui_mobile_destroy(self.handle.as_ptr()) };
        debug_assert_eq!(code, 0);
    }
}
