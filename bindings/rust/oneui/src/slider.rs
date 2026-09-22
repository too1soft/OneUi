//! Native slider input lifecycle, separate from programmatic value updates.
use super::{run_callback_guarded, sys, Error, UiDispatcher, Widget};
use std::ffi::c_void;
use std::rc::Rc;
use std::sync::{
    atomic::{AtomicBool, AtomicPtr, Ordering},
    Arc, Mutex,
};

struct SliderState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending: Mutex<Option<(Option<f64>, f64, bool)>>,
    scheduled: AtomicBool,
}

/// A bounded latest-value producer. Updates never move the thumb while it is
/// being dragged. `None` keeps the current thumb position during async seeks.
#[derive(Clone)]
pub struct SliderHandle {
    state: Arc<SliderState>,
    dispatcher: UiDispatcher,
}

impl SliderHandle {
    pub fn set_progress(
        &self,
        position: Option<f64>,
        duration: f64,
        enabled: bool,
    ) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        // Match the C++ setters: invalid non-finite input leaves state intact.
        if !duration.is_finite() || position.is_some_and(|value| !value.is_finite()) {
            return Ok(());
        }
        *self.state.pending.lock().unwrap_or_else(|p| p.into_inner()) =
            Some((position, duration.max(0.0001), enabled));
        if self.state.scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }
        let state = self.state.clone();
        if let Err(error) = self.dispatcher.dispatch(move || Self::drain(&state)) {
            self.state.scheduled.store(false, Ordering::Release);
            self.state
                .pending
                .lock()
                .unwrap_or_else(|p| p.into_inner())
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain(state: &SliderState) {
        loop {
            let update = state
                .pending
                .lock()
                .unwrap_or_else(|p| p.into_inner())
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some((position, duration, enabled)) = update {
                unsafe {
                    sys::oneui_widget_set_disabled(raw, i32::from(!enabled));
                    // Disabling can invoke a cancellation callback. Revalidate
                    // lifetime before performing any following native access.
                    if state.raw.load(Ordering::Acquire).is_null() {
                        return;
                    }
                    if sys::oneui_slider_is_dragging(raw) == 0 {
                        sys::oneui_slider_set_range(raw, 0.0, duration);
                        if let Some(position) = position {
                            sys::oneui_slider_set_value(raw, position);
                        }
                    }
                }
            }
            state.scheduled.store(false, Ordering::Release);
            if state
                .pending
                .lock()
                .unwrap_or_else(|p| p.into_inner())
                .is_none()
            {
                return;
            }
            if state.scheduled.swap(true, Ordering::AcqRel) {
                return;
            }
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SliderInteraction {
    Begin,
    Update,
    Commit,
    Cancel,
}

pub(super) struct InteractionCallback {
    // Fn, not FnMut: cancel/disable may synchronously reenter the handler.
    pub(super) handler: Rc<dyn Fn(SliderInteraction, f64)>,
}

pub(super) unsafe extern "C" fn run_interaction(phase: i32, value: f64, data: *mut c_void) {
    if data.is_null() || !value.is_finite() {
        return;
    }
    let phase = match phase {
        0 => SliderInteraction::Begin,
        1 => SliderInteraction::Update,
        2 => SliderInteraction::Commit,
        3 => SliderInteraction::Cancel,
        _ => return,
    };
    // A handler may disconnect itself (and free its FFI context). Retain the
    // callable before entering user code and never touch that context again.
    let handler = unsafe { Rc::clone(&(*data.cast::<InteractionCallback>()).handler) };
    run_callback_guarded("slider.interaction", || handler(phase, value));
}

/// UI-thread native slider with pointer capture managed by its parent view.
///
/// Dragging emits Begin/Update/Commit; Escape, hiding, disabling or losing
/// capture emits Cancel and restores the original value. Arrow keys and
/// Home/End each form a complete interaction. Setters emit no input events.
/// Call `cancel_interaction` before unmounting if a consumer must undo a Begin;
/// dropping the wrapper disconnects its callback without calling user code.
pub struct Slider {
    widget: Widget,
    callback: Option<Box<InteractionCallback>>,
    state: Arc<SliderState>,
}

impl Slider {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_slider_create() })?;
        Ok(Self {
            state: Arc::new(SliderState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending: Mutex::new(None),
                scheduled: AtomicBool::new(false),
            }),
            widget,
            callback: None,
        })
    }

    pub fn handle(&self, dispatcher: &UiDispatcher) -> SliderHandle {
        SliderHandle {
            state: self.state.clone(),
            dispatcher: dispatcher.clone(),
        }
    }

    pub fn set_range(&self, minimum: f64, maximum: f64) {
        unsafe { sys::oneui_slider_set_range(self.widget.as_raw(), minimum, maximum) }
    }

    pub fn set_step(&self, step: f64) {
        unsafe { sys::oneui_slider_set_step(self.widget.as_raw(), step) }
    }

    pub fn set_value(&self, value: f64) {
        unsafe { sys::oneui_slider_set_value(self.widget.as_raw(), value) }
    }

    pub fn value(&self) -> f64 {
        unsafe { sys::oneui_slider_value(self.widget.as_raw()) }
    }

    pub fn is_dragging(&self) -> bool {
        unsafe { sys::oneui_slider_is_dragging(self.widget.as_raw()) != 0 }
    }

    pub fn cancel_interaction(&self) {
        unsafe { sys::oneui_slider_cancel_interaction(self.widget.as_raw()) }
    }

    pub fn set_on_interaction(&mut self, callback: impl Fn(SliderInteraction, f64) + 'static) {
        self.clear_on_interaction();
        let mut callback = Box::new(InteractionCallback {
            handler: Rc::new(callback),
        });
        unsafe {
            sys::oneui_slider_set_on_interaction(
                self.widget.as_raw(),
                Some(run_interaction),
                (&mut *callback as *mut InteractionCallback).cast(),
            )
        }
        self.callback = Some(callback);
    }

    pub fn clear_on_interaction(&mut self) {
        unsafe {
            sys::oneui_slider_set_on_interaction(self.widget.as_raw(), None, std::ptr::null_mut())
        }
        self.callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Slider {
    fn drop(&mut self) {
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending
            .lock()
            .unwrap_or_else(|p| p.into_inner())
            .take();
        self.clear_on_interaction();
    }
}
