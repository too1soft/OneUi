//! Remote input bindings.

use super::key_event::RawKeyEvent;
use super::remote_frame::{
    RemoteCursorImage, RemoteCursorMode, RemotePointerEvent, VideoScaleMode,
};
use super::{
    run_callback_guarded, sys, traced_value_callback, Arc, AtomicBool, AtomicPtr, Error,
    InteractionTrace, Mutex, Ordering, UiDispatcher, Widget,
};

impl UiDispatcher {
    /// Creates a thread-safe state producer for a remote input region.
    pub fn remote_input_region_handle(
        &self,
        input_region: &RemoteInputRegion,
    ) -> RemoteInputRegionHandle {
        RemoteInputRegionHandle {
            state: Arc::clone(&input_region.state),
            dispatcher: self.clone(),
        }
    }
}

struct RemotePointerCallback {
    handler: Box<dyn FnMut(RemotePointerEvent) + 'static>,
}

struct RemoteRawKeyCallback {
    handler: Box<dyn FnMut(RawKeyEvent) + 'static>,
}

pub(super) struct RemoteTextInputCallback {
    pub(super) handler: Box<dyn FnMut(String) + 'static>,
}

#[derive(Default)]
struct PendingRemoteInputUpdate {
    remote_size: Option<(f32, f32)>,
    cursor_mode: Option<RemoteCursorMode>,
    cursor_position: Option<(f32, f32)>,
    cursor_image: Option<RemoteCursorImage>,
    release_all_inputs: bool,
}

impl PendingRemoteInputUpdate {
    fn is_empty(&self) -> bool {
        self.remote_size.is_none()
            && self.cursor_mode.is_none()
            && self.cursor_position.is_none()
            && self.cursor_image.is_none()
            && !self.release_all_inputs
    }
}

struct RemoteInputRegionState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending: Mutex<PendingRemoteInputUpdate>,
    update_scheduled: AtomicBool,
}

/// Thread-safe coalescing producer for remote size and server cursor updates.
#[derive(Clone)]
pub struct RemoteInputRegionHandle {
    state: Arc<RemoteInputRegionState>,
    dispatcher: UiDispatcher,
}

impl RemoteInputRegionHandle {
    pub fn set_remote_size(&self, width: f32, height: f32) -> Result<(), Error> {
        if !width.is_finite() || !height.is_finite() || width < 0.0 || height < 0.0 {
            return Err(Error::InvalidRemoteCursor {
                reason: "remote dimensions must be finite and non-negative",
            });
        }
        self.queue_update(|pending| pending.remote_size = Some((width, height)))
    }

    pub fn set_cursor_default(&self) -> Result<(), Error> {
        self.queue_update(|pending| pending.cursor_mode = Some(RemoteCursorMode::Default))
    }

    pub fn set_cursor_hidden(&self) -> Result<(), Error> {
        self.queue_update(|pending| pending.cursor_mode = Some(RemoteCursorMode::Hidden))
    }

    pub fn set_cursor_position(&self, remote_x: f32, remote_y: f32) -> Result<(), Error> {
        if !remote_x.is_finite() || !remote_y.is_finite() {
            return Err(Error::InvalidRemoteCursor {
                reason: "cursor position must be finite",
            });
        }
        self.queue_update(|pending| pending.cursor_position = Some((remote_x, remote_y)))
    }

    pub fn submit_cursor_image(&self, mut image: RemoteCursorImage) -> Result<(), Error> {
        image.normalize_and_validate()?;
        self.queue_update(move |pending| {
            pending.cursor_image = Some(image);
            pending.cursor_mode = Some(RemoteCursorMode::Bitmap);
        })
    }

    /// Releases every pressed key and pointer button on the UI thread. This is
    /// safe to call from a network worker during disconnect or reconnect.
    pub fn release_all_inputs(&self) -> Result<(), Error> {
        self.queue_update(|pending| pending.release_all_inputs = true)
    }

    fn queue_update(
        &self,
        update: impl FnOnce(&mut PendingRemoteInputUpdate),
    ) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        update(
            &mut self
                .state
                .pending
                .lock()
                .expect("remote input pending lock poisoned"),
        );
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }
        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            *state
                .pending
                .lock()
                .expect("remote input pending lock poisoned") = PendingRemoteInputUpdate::default();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending(state: &RemoteInputRegionState) {
        loop {
            let pending = std::mem::take(
                &mut *state
                    .pending
                    .lock()
                    .expect("remote input pending lock poisoned"),
            );
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some((width, height)) = pending.remote_size {
                unsafe { sys::oneui_remote_input_region_set_remote_size(raw, width, height) };
            }
            if let Some(image) = pending.cursor_image {
                let accepted = unsafe {
                    sys::oneui_remote_input_region_set_cursor_bitmap_rgba(
                        raw,
                        image.pixels.as_ptr().cast(),
                        image.pixels.len(),
                        image.width,
                        image.height,
                        image.stride,
                        image.hotspot_x,
                        image.hotspot_y,
                    )
                };
                debug_assert_ne!(accepted, 0, "validated remote cursor was rejected by OneUI");
            }
            if let Some((remote_x, remote_y)) = pending.cursor_position {
                unsafe {
                    sys::oneui_remote_input_region_set_cursor_position(raw, remote_x, remote_y)
                };
            }
            if let Some(mode) = pending.cursor_mode {
                unsafe { sys::oneui_remote_input_region_set_cursor_mode(raw, mode as i32) };
            }
            if pending.release_all_inputs {
                unsafe { sys::oneui_remote_input_region_release_all_inputs(raw) };
            }

            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending
                .lock()
                .expect("remote input pending lock poisoned")
                .is_empty()
            {
                return;
            }
            if !state.update_scheduled.swap(true, Ordering::AcqRel) {
                continue;
            }
            return;
        }
    }
}

unsafe extern "C" fn run_remote_pointer_callback(
    event: *const sys::OneUiRemotePointerEvent,
    user_data: *mut std::ffi::c_void,
) {
    if event.is_null() || user_data.is_null() {
        return;
    }
    let event = RemotePointerEvent::from(unsafe { *event });
    let callback = unsafe { &mut *user_data.cast::<RemotePointerCallback>() };
    run_callback_guarded("remote_input.pointer", || (callback.handler)(event));
}

unsafe extern "C" fn run_remote_raw_key_callback(
    event: *const sys::OneUiRawKeyEvent,
    user_data: *mut std::ffi::c_void,
) {
    if event.is_null() || user_data.is_null() {
        return;
    }
    let event = RawKeyEvent::from(unsafe { *event });
    let callback = unsafe { &mut *user_data.cast::<RemoteRawKeyCallback>() };
    run_callback_guarded("remote_input.raw_key", || (callback.handler)(event));
}

pub(super) unsafe extern "C" fn run_remote_text_input_callback(
    text: *const std::ffi::c_char,
    length: usize,
    user_data: *mut std::ffi::c_void,
) {
    if text.is_null() || user_data.is_null() {
        return;
    }
    let bytes = unsafe { std::slice::from_raw_parts(text.cast::<u8>(), length) };
    let value = String::from_utf8_lossy(bytes).into_owned();
    let callback = unsafe { &mut *user_data.cast::<RemoteTextInputCallback>() };
    run_callback_guarded("remote_input.text_input", || (callback.handler)(value));
}

/// Transparent input mapper for a remote framebuffer. It translates logical
/// pointer coordinates into remote pixels and preserves raw keyboard details.
pub struct RemoteInputRegion {
    widget: Widget,
    state: Arc<RemoteInputRegionState>,
    pointer_callback: Option<Box<RemotePointerCallback>>,
    raw_key_callback: Option<Box<RemoteRawKeyCallback>>,
    text_input_callback: Option<Box<RemoteTextInputCallback>>,
}

impl RemoteInputRegion {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_remote_input_region_create() })?;
        Ok(Self {
            state: Arc::new(RemoteInputRegionState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending: Mutex::new(PendingRemoteInputUpdate::default()),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
            pointer_callback: None,
            raw_key_callback: None,
            text_input_callback: None,
        })
    }

    pub fn set_remote_size(&self, width: f32, height: f32) {
        unsafe {
            sys::oneui_remote_input_region_set_remote_size(self.widget.as_raw(), width, height)
        };
    }

    pub fn set_scale_mode(&self, scale_mode: VideoScaleMode) {
        unsafe {
            sys::oneui_remote_input_region_set_scale_mode(self.widget.as_raw(), scale_mode as i32)
        };
    }

    pub fn set_cursor_default(&self) {
        unsafe {
            sys::oneui_remote_input_region_set_cursor_mode(
                self.widget.as_raw(),
                RemoteCursorMode::Default as i32,
            )
        };
    }

    pub fn set_cursor_hidden(&self) {
        unsafe {
            sys::oneui_remote_input_region_set_cursor_mode(
                self.widget.as_raw(),
                RemoteCursorMode::Hidden as i32,
            )
        };
    }

    pub fn set_cursor_position(&self, remote_x: f32, remote_y: f32) {
        unsafe {
            sys::oneui_remote_input_region_set_cursor_position(
                self.widget.as_raw(),
                remote_x,
                remote_y,
            )
        };
    }

    pub fn set_cursor_image(&self, mut image: RemoteCursorImage) -> Result<(), Error> {
        image.normalize_and_validate()?;
        let accepted = unsafe {
            sys::oneui_remote_input_region_set_cursor_bitmap_rgba(
                self.widget.as_raw(),
                image.pixels.as_ptr().cast(),
                image.pixels.len(),
                image.width,
                image.height,
                image.stride,
                image.hotspot_x,
                image.hotspot_y,
            )
        };
        if accepted != 0 {
            Ok(())
        } else {
            Err(Error::InvalidRemoteCursor {
                reason: "native remote input region rejected the cursor",
            })
        }
    }

    #[track_caller]
    pub fn set_on_pointer<F>(&mut self, callback: F)
    where
        F: FnMut(RemotePointerEvent) + 'static,
    {
        let trace = InteractionTrace::at(
            "RemoteInputRegion",
            "pointer",
            std::panic::Location::caller(),
        );
        self.clear_pointer_callback();
        self.pointer_callback = Some(Box::new(RemotePointerCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .pointer_callback
            .as_deref_mut()
            .expect("remote pointer callback was just installed")
            as *mut RemotePointerCallback)
            .cast();
        unsafe {
            sys::oneui_remote_input_region_set_on_pointer(
                self.widget.as_raw(),
                Some(run_remote_pointer_callback),
                user_data,
            )
        };
    }

    #[track_caller]
    pub fn set_on_raw_key<F>(&mut self, callback: F)
    where
        F: FnMut(RawKeyEvent) + 'static,
    {
        let trace = InteractionTrace::at(
            "RemoteInputRegion",
            "raw_key",
            std::panic::Location::caller(),
        );
        self.clear_raw_key_callback();
        self.raw_key_callback = Some(Box::new(RemoteRawKeyCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .raw_key_callback
            .as_deref_mut()
            .expect("remote raw-key callback was just installed")
            as *mut RemoteRawKeyCallback)
            .cast();
        unsafe {
            sys::oneui_remote_input_region_set_on_raw_key(
                self.widget.as_raw(),
                Some(run_remote_raw_key_callback),
                user_data,
            )
        };
    }

    /// Receives committed UTF-8 text, including IME results. While this
    /// callback is installed, ordinary printable keys are delivered here
    /// instead of being duplicated through the raw-key callback. Modified and
    /// non-text keys continue to use `set_on_raw_key`.
    #[track_caller]
    pub fn set_on_text_input<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        let trace = InteractionTrace::at(
            "RemoteInputRegion",
            "text_input",
            std::panic::Location::caller(),
        );
        self.clear_text_input_callback();
        self.text_input_callback = Some(Box::new(RemoteTextInputCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .text_input_callback
            .as_deref_mut()
            .expect("remote text-input callback was just installed")
            as *mut RemoteTextInputCallback)
            .cast();
        unsafe {
            sys::oneui_remote_input_region_set_on_text_input_utf8(
                self.widget.as_raw(),
                Some(run_remote_text_input_callback),
                user_data,
            )
        };
    }

    pub fn release_all_inputs(&self) {
        unsafe { sys::oneui_remote_input_region_release_all_inputs(self.widget.as_raw()) };
    }

    pub fn clear_pointer_callback(&mut self) {
        unsafe {
            sys::oneui_remote_input_region_set_on_pointer(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.pointer_callback = None;
    }

    pub fn clear_raw_key_callback(&mut self) {
        unsafe {
            sys::oneui_remote_input_region_set_on_raw_key(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.raw_key_callback = None;
    }

    pub fn clear_text_input_callback(&mut self) {
        unsafe {
            sys::oneui_remote_input_region_set_on_text_input_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.text_input_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for RemoteInputRegion {
    fn drop(&mut self) {
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        *self
            .state
            .pending
            .lock()
            .expect("remote input pending lock poisoned") = PendingRemoteInputUpdate::default();
        self.state.update_scheduled.store(false, Ordering::Release);
        self.release_all_inputs();
        self.clear_pointer_callback();
        self.clear_raw_key_callback();
        self.clear_text_input_callback();
    }
}
