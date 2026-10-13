//! Sparkline bindings.

use super::{sys, Arc, AtomicBool, AtomicPtr, Error, Mutex, Ordering, UiDispatcher, Widget};

impl UiDispatcher {
    /// Creates a thread-safe sample producer for a sparkline mounted in this window.
    pub fn sparkline_handle(&self, sparkline: &Sparkline) -> SparklineHandle {
        SparklineHandle {
            state: Arc::clone(&sparkline.state),
            dispatcher: self.clone(),
        }
    }
}

struct SparklineState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_values: Mutex<Option<Vec<f64>>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for normalized sparkline samples owned by a window.
///
/// Bursts are coalesced to the latest complete sample set and applied on the
/// owning UI thread, preserving a stable frame under frequent telemetry updates.
#[derive(Clone)]
pub struct SparklineHandle {
    state: Arc<SparklineState>,
    dispatcher: UiDispatcher,
}

impl SparklineHandle {
    pub fn set_values(&self, values: impl Into<Vec<f64>>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        *self
            .state
            .pending_values
            .lock()
            .expect("sparkline pending values lock poisoned") = Some(values.into());
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }

        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_values(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_values
                .lock()
                .expect("sparkline pending values lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_values(state: &SparklineState) {
        loop {
            let values = state
                .pending_values
                .lock()
                .expect("sparkline pending values lock poisoned")
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(values) = values {
                unsafe { sys::oneui_sparkline_set_values(raw, values.as_ptr(), values.len()) };
            }
            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_values
                .lock()
                .expect("sparkline pending values lock poisoned")
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

/// A compact native time-series visualization with normalized [0, 1] samples.
pub struct Sparkline {
    widget: Widget,
    state: Arc<SparklineState>,
}

impl Sparkline {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_sparkline_create() })?;
        Ok(Self {
            state: Arc::new(SparklineState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_values: Mutex::new(None),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
        })
    }

    pub fn set_values(&self, values: &[f64]) {
        unsafe {
            sys::oneui_sparkline_set_values(self.widget.as_raw(), values.as_ptr(), values.len())
        };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Sparkline {
    fn drop(&mut self) {
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_values
            .lock()
            .expect("sparkline pending values lock poisoned")
            .take();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}
