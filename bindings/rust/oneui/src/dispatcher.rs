//! UI dispatch ownership, cancellation and synchronized window lifetime.
use super::{run_callback_guarded, sys, Error};
use std::{
    ptr::NonNull,
    sync::{Arc, Mutex},
};

pub(crate) struct WindowState {
    pub(crate) raw: Mutex<Option<NonNull<sys::OneUiWindow>>>,
    pub(crate) ui_thread: std::thread::ThreadId,
}

// WindowState synchronizes every raw-handle access and native destruction is
// constrained to the recorded UI thread. The dispatcher only posts work.
unsafe impl Send for WindowState {}
unsafe impl Sync for WindowState {}

#[derive(Clone)]
pub struct UiDispatcher {
    pub(crate) state: Arc<WindowState>,
}

struct DispatchedTask {
    task: Option<Box<dyn FnOnce() + Send + 'static>>,
}

struct LocalDispatchedTask {
    task: Option<Box<dyn FnOnce() + 'static>>,
}

struct AnimationFrameTask {
    task: Option<Box<dyn FnOnce(f64) + 'static>>,
}

pub(crate) fn trace_ui_task(message: &str) {
    if let Some(path) = std::env::var_os("ONEUI_UI_TRACE_FILE") {
        use std::io::Write;

        if let Ok(mut file) = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open(path)
        {
            let _ = writeln!(
                file,
                "[oneui-ui] [thread={:?}] {message}",
                std::thread::current().id()
            );
        }
    } else if std::env::var_os("ONEUI_UI_TRACE").is_some() {
        eprintln!(
            "[oneui-ui] [thread={:?}] {message}",
            std::thread::current().id()
        );
    }
}

impl WindowState {
    pub(crate) fn current_raw(&self) -> Option<NonNull<sys::OneUiWindow>> {
        *self.raw.lock().expect("OneUI window state lock poisoned")
    }

    #[track_caller]
    pub(crate) fn with_raw<R>(&self, action: impl FnOnce(*mut sys::OneUiWindow) -> R) -> Option<R> {
        let caller = std::panic::Location::caller();
        let caller = format!("{}:{}:{}", caller.file(), caller.line(), caller.column());

        // Native dialogs and other platform APIs may run a nested message loop.
        // A callback reached through that loop is still on the owning UI thread
        // and must be allowed to access the same window again. Copying the raw
        // handle under the lock and releasing it before the native call keeps
        // same-thread reentrancy safe. Destruction also belongs to this thread,
        // so it cannot race between the copy and the call.
        if std::thread::current().id() == self.ui_thread {
            let raw = self.current_raw()?;
            trace_ui_task(&format!("window raw copied on UI thread caller={caller}"));
            trace_ui_task(&format!("window raw action entered caller={caller}"));
            let result = action(raw.as_ptr());
            trace_ui_task(&format!("window raw action completed caller={caller}"));
            return Some(result);
        }

        // Worker-thread calls retain the mutex for the duration of the FFI
        // operation so WindowState::destroy cannot free the native window while
        // a cross-thread post is being submitted.
        let raw = match self.raw.try_lock() {
            Ok(raw) => {
                trace_ui_task(&format!("window raw lock acquired caller={caller}"));
                raw
            }
            Err(std::sync::TryLockError::WouldBlock) => {
                trace_ui_task(&format!("window raw lock contended caller={caller}"));
                let raw = self.raw.lock().expect("OneUI window state lock poisoned");
                trace_ui_task(&format!(
                    "window raw lock acquired after contention caller={caller}"
                ));
                raw
            }
            Err(std::sync::TryLockError::Poisoned(poisoned)) => poisoned.into_inner(),
        };
        trace_ui_task(&format!("window raw action entered caller={caller}"));
        let result = raw.as_ref().map(|raw| action(raw.as_ptr()));
        trace_ui_task(&format!("window raw action completed caller={caller}"));
        result
    }

    pub(crate) fn destroy(&self) {
        let raw = self
            .raw
            .lock()
            .expect("OneUI window state lock poisoned")
            .take();
        if let Some(raw) = raw {
            unsafe { sys::oneui_window_destroy(raw.as_ptr()) };
        }
    }
}

unsafe extern "C" fn run_dispatched_task(user_data: *mut std::ffi::c_void) {
    let mut task = unsafe { Box::from_raw(user_data.cast::<DispatchedTask>()) };
    if let Some(task) = task.task.take() {
        run_callback_guarded("dispatcher.task", task);
    }
}

unsafe extern "C" fn drop_dispatched_task(user_data: *mut std::ffi::c_void) {
    drop(unsafe { Box::from_raw(user_data.cast::<DispatchedTask>()) });
}

unsafe extern "C" fn run_local_dispatched_task(user_data: *mut std::ffi::c_void) {
    let mut task = unsafe { Box::from_raw(user_data.cast::<LocalDispatchedTask>()) };
    if let Some(task) = task.task.take() {
        run_callback_guarded("dispatcher.local_task", task);
    }
}

unsafe extern "C" fn drop_local_dispatched_task(user_data: *mut std::ffi::c_void) {
    drop(unsafe { Box::from_raw(user_data.cast::<LocalDispatchedTask>()) });
}

unsafe extern "C" fn run_animation_frame_task(now_ms: f64, user_data: *mut std::ffi::c_void) {
    let mut task = unsafe { Box::from_raw(user_data.cast::<AnimationFrameTask>()) };
    if let Some(task) = task.task.take() {
        run_callback_guarded("dispatcher.animation_frame", || task(now_ms));
    }
}

impl UiDispatcher {
    pub fn dispatch<F>(&self, task: F) -> Result<(), Error>
    where
        F: FnOnce() + Send + 'static,
    {
        let task = Box::new(DispatchedTask {
            task: Some(Box::new(task)),
        });
        let user_data = Box::into_raw(task).cast();
        let accepted = self.state.with_raw(|raw| unsafe {
            sys::oneui_window_post_owned(
                raw,
                Some(run_dispatched_task),
                user_data,
                Some(drop_dispatched_task),
            )
        });

        match accepted {
            Some(1) => Ok(()),
            Some(_) => Err(Error::WindowClosed),
            None => {
                drop(unsafe { Box::from_raw(user_data.cast::<DispatchedTask>()) });
                Err(Error::WindowClosed)
            }
        }
    }

    /// Defers non-`Send` UI-owned work until after the current native event callback returns.
    ///
    /// This is intentionally restricted to the window thread. Use [`Self::dispatch`] for
    /// background services. The queued closure is executed or destroyed on the owning
    /// window thread, so it may safely capture `Rc`-owned widget and product state.
    pub fn dispatch_local<F>(&self, task: F) -> Result<(), Error>
    where
        F: FnOnce() + 'static,
    {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        trace_ui_task("local dispatch started");
        let task = Box::new(LocalDispatchedTask {
            task: Some(Box::new(task)),
        });
        let user_data = Box::into_raw(task).cast();
        let accepted = self.state.with_raw(|raw| unsafe {
            trace_ui_task("local dispatch entered window post");
            sys::oneui_window_post_owned(
                raw,
                Some(run_local_dispatched_task),
                user_data,
                Some(drop_local_dispatched_task),
            )
        });
        trace_ui_task("local dispatch completed window post");

        match accepted {
            Some(1) => Ok(()),
            Some(_) => Err(Error::WindowClosed),
            None => {
                drop(unsafe { Box::from_raw(user_data.cast::<LocalDispatchedTask>()) });
                Err(Error::WindowClosed)
            }
        }
    }

    /// Runs a UI-owned callback on the next presentation frame.
    ///
    /// The callback executes on the window thread and may capture non-`Send`
    /// product state. This is a one-shot request; recurring work must explicitly
    /// request the next frame after deciding it still has work to do.
    pub fn request_animation_frame_local<F>(&self, task: F) -> Result<(), Error>
    where
        F: FnOnce(f64) + 'static,
    {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let task = Box::new(AnimationFrameTask {
            task: Some(Box::new(task)),
        });
        let user_data = Box::into_raw(task).cast();
        let Some(()) = self.state.with_raw(|raw| unsafe {
            sys::oneui_window_request_animation_frame(
                raw,
                Some(run_animation_frame_task),
                user_data,
            )
        }) else {
            drop(unsafe { Box::from_raw(user_data.cast::<AnimationFrameTask>()) });
            return Err(Error::WindowClosed);
        };
        Ok(())
    }
}
