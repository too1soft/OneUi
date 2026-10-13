//! Realtime frame view bindings.

use super::images::ImageContentAlignment;
use super::remote_frame::{RemoteFrame, RemoteFrameDamage, VideoScaleMode};
use super::visual_style::Color;
use super::{sys, Arc, AtomicBool, AtomicPtr, Error, Mutex, Ordering, UiDispatcher, Widget};

impl UiDispatcher {
    /// Creates a thread-safe latest-frame producer for a realtime frame view.
    pub fn realtime_frame_view_handle(
        &self,
        frame_view: &RealtimeFrameView,
    ) -> RealtimeFrameViewHandle {
        RealtimeFrameViewHandle {
            state: Arc::clone(&frame_view.state),
            dispatcher: self.clone(),
        }
    }
}

#[derive(Default)]
struct PendingRemoteFrameBatch {
    full: Option<RemoteFrame>,
    damages: Vec<RemoteFrameDamage>,
}

struct RealtimeFrameViewState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_batch: Mutex<PendingRemoteFrameBatch>,
    update_scheduled: AtomicBool,
}

/// Thread-safe latest-frame producer for a mounted [`RealtimeFrameView`].
///
/// Decoder bursts are coalesced before reaching the UI thread. At most one
/// dispatcher task and one pending decoded frame are retained at a time.
#[derive(Clone)]
pub struct RealtimeFrameViewHandle {
    state: Arc<RealtimeFrameViewState>,
    dispatcher: UiDispatcher,
}

impl RealtimeFrameViewHandle {
    pub fn submit_frame(&self, mut frame: RemoteFrame) -> Result<(), Error> {
        frame.normalize_and_validate()?;
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        {
            let mut pending = self
                .state
                .pending_batch
                .lock()
                .expect("realtime frame pending lock poisoned");
            pending.full = Some(frame);
            pending.damages.clear();
        }
        self.schedule_pending_update()
    }

    pub fn submit_damage(&self, mut damage: RemoteFrameDamage) -> Result<(), Error> {
        damage.normalize_and_validate()?;
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        self.state
            .pending_batch
            .lock()
            .expect("realtime frame pending lock poisoned")
            .damages
            .push(damage);
        self.schedule_pending_update()
    }

    fn schedule_pending_update(&self) -> Result<(), Error> {
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }

        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_frame(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            *state
                .pending_batch
                .lock()
                .expect("realtime frame pending lock poisoned") =
                PendingRemoteFrameBatch::default();
            return Err(error);
        }
        Ok(())
    }

    pub fn set_scale_mode(&self, scale_mode: VideoScaleMode) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe { sys::oneui_realtime_frame_view_set_scale_mode(raw, scale_mode as i32) };
            }
        })
    }

    pub fn set_background(&self, color: Color) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe {
                    sys::oneui_realtime_frame_view_set_background(
                        raw, color.r, color.g, color.b, color.a,
                    )
                };
            }
        })
    }

    fn drain_pending_frame(state: &RealtimeFrameViewState) {
        loop {
            let batch = {
                let mut pending = state
                    .pending_batch
                    .lock()
                    .expect("realtime frame pending lock poisoned");
                std::mem::take(&mut *pending)
            };
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(frame) = batch.full {
                let accepted = apply_remote_frame(raw, frame);
                debug_assert!(accepted, "validated remote frame was rejected by OneUI");
            }
            for damage in batch.damages {
                let accepted = apply_remote_frame_damage(raw, &damage);
                debug_assert!(
                    accepted,
                    "validated remote frame damage was rejected by OneUI"
                );
            }

            state.update_scheduled.store(false, Ordering::Release);
            let pending_is_empty = {
                let pending = state
                    .pending_batch
                    .lock()
                    .expect("realtime frame pending lock poisoned");
                pending.full.is_none() && pending.damages.is_empty()
            };
            if pending_is_empty {
                return;
            }
            if !state.update_scheduled.swap(true, Ordering::AcqRel) {
                continue;
            }
            return;
        }
    }
}

unsafe extern "C" fn release_remote_frame(
    _pixels: *const std::ffi::c_void,
    user_data: *mut std::ffi::c_void,
) {
    if !user_data.is_null() {
        drop(unsafe { Box::from_raw(user_data.cast::<Arc<[u8]>>()) });
    }
}

fn apply_remote_frame(raw: *mut sys::OneUiWidget, frame: RemoteFrame) -> bool {
    let RemoteFrame {
        pixels,
        width,
        height,
        stride,
        format,
        frame_id,
        timestamp_us,
    } = frame;
    let pixels = Box::new(pixels);
    let pixel_pointer = pixels.as_ptr().cast();
    let pixel_bytes = pixels.len();
    let user_data = Box::into_raw(pixels).cast();
    unsafe {
        sys::oneui_realtime_frame_view_submit_frame_owned(
            raw,
            pixel_pointer,
            pixel_bytes,
            width,
            height,
            stride,
            format as i32,
            frame_id,
            timestamp_us,
            Some(release_remote_frame),
            user_data,
        ) != 0
    }
}

fn apply_remote_frame_damage(raw: *mut sys::OneUiWidget, damage: &RemoteFrameDamage) -> bool {
    let patches = damage
        .patches
        .iter()
        .map(|patch| sys::OneUiVideoFramePatch {
            pixels: patch.pixels.as_ptr().cast(),
            pixel_bytes: patch.pixels.len(),
            x: patch.x,
            y: patch.y,
            width: patch.width,
            height: patch.height,
            stride: patch.stride,
        })
        .collect::<Vec<_>>();
    unsafe {
        sys::oneui_realtime_frame_view_submit_damage(
            raw,
            damage.width,
            damage.height,
            damage.format as i32,
            patches.as_ptr(),
            patches.len(),
            damage.frame_id,
            damage.timestamp_us,
        ) != 0
    }
}

/// Native video surface. Mutating methods are UI-thread bound; decoders should
/// obtain a [`RealtimeFrameViewHandle`] from the owning window or dispatcher.
pub struct RealtimeFrameView {
    widget: Widget,
    state: Arc<RealtimeFrameViewState>,
}

impl RealtimeFrameView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_realtime_frame_view_create() })?;
        Ok(Self {
            state: Arc::new(RealtimeFrameViewState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_batch: Mutex::new(PendingRemoteFrameBatch::default()),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
        })
    }

    pub fn set_scale_mode(&self, scale_mode: VideoScaleMode) {
        unsafe {
            sys::oneui_realtime_frame_view_set_scale_mode(self.widget.as_raw(), scale_mode as i32)
        };
    }

    pub fn set_content_alignment(
        &self,
        horizontal: ImageContentAlignment,
        vertical: ImageContentAlignment,
    ) {
        unsafe {
            sys::oneui_realtime_frame_view_set_content_alignment(
                self.widget.as_raw(),
                horizontal as i32,
                vertical as i32,
            )
        };
    }

    pub fn set_background(&self, color: Color) {
        unsafe {
            sys::oneui_realtime_frame_view_set_background(
                self.widget.as_raw(),
                color.r,
                color.g,
                color.b,
                color.a,
            )
        };
    }

    pub fn submit_frame(&self, mut frame: RemoteFrame) -> Result<(), Error> {
        frame.normalize_and_validate()?;
        if apply_remote_frame(self.widget.as_raw(), frame) {
            Ok(())
        } else {
            Err(Error::InvalidVideoFrame {
                reason: "native frame view rejected the frame",
            })
        }
    }

    pub fn submit_damage(&self, mut damage: RemoteFrameDamage) -> Result<(), Error> {
        damage.normalize_and_validate()?;
        if apply_remote_frame_damage(self.widget.as_raw(), &damage) {
            Ok(())
        } else {
            Err(Error::InvalidVideoFrame {
                reason: "native frame view rejected the damage batch",
            })
        }
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for RealtimeFrameView {
    fn drop(&mut self) {
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_batch
            .lock()
            .expect("realtime frame pending lock poisoned")
            .full = None;
        self.state
            .pending_batch
            .lock()
            .expect("realtime frame pending lock poisoned")
            .damages
            .clear();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}
