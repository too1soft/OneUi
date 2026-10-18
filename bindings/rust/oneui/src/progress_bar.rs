//! Progress bar bindings.

use super::visual_style::Color;
use super::{sys, Arc, AtomicBool, AtomicPtr, Error, Mutex, Ordering, UiDispatcher, Widget};

impl UiDispatcher {
    /// Creates a thread-safe value producer for a progress bar mounted in this window.
    pub fn progress_bar_handle(&self, progress_bar: &ProgressBar) -> ProgressBarHandle {
        ProgressBarHandle {
            state: Arc::clone(&progress_bar.state),
            dispatcher: self.clone(),
        }
    }
}

struct ProgressBarState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_value: Mutex<Option<f64>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for a determinate progress value owned by a window.
///
/// Worker updates are clamped by OneUI, coalesced to the latest value and
/// applied only on the owning UI thread.
#[derive(Clone)]
pub struct ProgressBarHandle {
    state: Arc<ProgressBarState>,
    dispatcher: UiDispatcher,
}

impl ProgressBarHandle {
    pub fn set_value(&self, value: f64) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        *self
            .state
            .pending_value
            .lock()
            .expect("progress bar pending value lock poisoned") = Some(value);
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }

        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_value(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_value
                .lock()
                .expect("progress bar pending value lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    pub fn set_indeterminate(&self, indeterminate: bool) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe {
                    sys::oneui_progress_bar_set_indeterminate(raw, i32::from(indeterminate));
                }
            }
        })
    }

    fn drain_pending_value(state: &ProgressBarState) {
        loop {
            let value = state
                .pending_value
                .lock()
                .expect("progress bar pending value lock poisoned")
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(value) = value {
                unsafe { sys::oneui_progress_bar_set_value(raw, value) };
            }
            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_value
                .lock()
                .expect("progress bar pending value lock poisoned")
                .is_none()
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

/// A standard determinate progress indicator with values in the [0, 1] range.
#[derive(Clone, Debug)]
pub struct ProgressSegment {
    pub fraction: f64,
    pub color: Color,
    pub label: String,
}

pub struct ProgressBar {
    widget: Widget,
    state: Arc<ProgressBarState>,
}

impl ProgressBar {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_progress_bar_create() })?;
        Ok(Self {
            state: Arc::new(ProgressBarState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_value: Mutex::new(None),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
        })
    }

    pub fn set_value(&self, value: f64) {
        unsafe { sys::oneui_progress_bar_set_value(self.widget.as_raw(), value) };
    }

    /// Replace absolute portions of the track. Fractions must be finite and nonnegative.
    pub fn set_segments(&self, segments: &[ProgressSegment]) -> bool {
        let values = segments
            .iter()
            .map(|s| sys::OneUiProgressSegmentUtf8 {
                fraction: s.fraction,
                color: s.color.into(),
                label: sys::OneUiUtf8String::from_str(&s.label),
            })
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_progress_bar_set_segments_utf8(
                self.widget.as_raw(),
                values.as_ptr(),
                values.len(),
            ) != 0
        }
    }

    pub fn value(&self) -> f64 {
        unsafe { sys::oneui_progress_bar_value(self.widget.as_raw()) }
    }

    pub fn set_smooth(&self, enabled: bool, duration_ms: f64) {
        unsafe {
            sys::oneui_progress_bar_set_smooth(
                self.widget.as_raw(),
                i32::from(enabled),
                duration_ms.clamp(0.0, 2000.0),
            )
        };
    }

    pub fn set_indeterminate(&self, indeterminate: bool) {
        unsafe {
            sys::oneui_progress_bar_set_indeterminate(
                self.widget.as_raw(),
                i32::from(indeterminate),
            )
        };
    }

    pub fn is_indeterminate(&self) -> bool {
        unsafe { sys::oneui_progress_bar_is_indeterminate(self.widget.as_raw()) != 0 }
    }

    pub fn set_animations_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_progress_bar_set_animations_enabled(self.widget.as_raw(), i32::from(enabled))
        };
    }

    pub fn set_colors(&self, track: Color, fill: Color, radius: f32) {
        unsafe {
            sys::oneui_progress_bar_set_colors(
                self.widget.as_raw(),
                track.r,
                track.g,
                track.b,
                track.a,
                fill.r,
                fill.g,
                fill.b,
                fill.a,
                radius,
            )
        };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for ProgressBar {
    fn drop(&mut self) {
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_value
            .lock()
            .expect("progress bar pending value lock poisoned")
            .take();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}
