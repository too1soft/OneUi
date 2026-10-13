//! Text area bindings.

use super::text_field::{
    run_text_field_changed_callback, run_text_field_submitted_callback, TextFieldChangedCallback,
};
use super::{
    sys, trace_ui_task, traced_value_callback, Arc, AtomicBool, AtomicPtr, Error, InteractionTrace,
    Mutex, Ordering, UiDispatcher, Widget,
};

impl UiDispatcher {
    /// Creates a thread-safe text producer for a multiline editor mounted in this window.
    pub fn text_area_handle(&self, text_area: &TextArea) -> TextAreaHandle {
        TextAreaHandle {
            state: Arc::clone(&text_area.state),
            dispatcher: self.clone(),
        }
    }
}

struct TextAreaState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_text: Mutex<Option<String>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for multiline editor text owned by a window.
///
/// Worker threads submit only the latest value. OneUI coalesces bursts and
/// applies text on the owning UI thread while respecting the editor lifetime.
#[derive(Clone)]
pub struct TextAreaHandle {
    state: Arc<TextAreaState>,
    dispatcher: UiDispatcher,
}

impl TextAreaHandle {
    pub fn set_text(&self, text: impl Into<String>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }

        *self
            .state
            .pending_text
            .lock()
            .expect("text area pending text lock poisoned") = Some(text.into());
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }

        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_text(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_text
                .lock()
                .expect("text area pending text lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    /// Returns the view to the first line on the owning UI thread.
    pub fn scroll_to_top(&self) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe { sys::oneui_text_area_scroll_to_top(raw) };
            }
        })
    }

    fn drain_pending_text(state: &TextAreaState) {
        loop {
            let text = state
                .pending_text
                .lock()
                .expect("text area pending text lock poisoned")
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(text) = text {
                let text = sys::OneUiUtf8String::from_str(&text);
                trace_ui_task("text area update started");
                unsafe { sys::oneui_text_field_set_text_utf8(raw, text) };
                trace_ui_task("text area update completed");
            }

            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_text
                .lock()
                .expect("text area pending text lock poisoned")
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

/// A native UTF-8 multiline editor with owned change callbacks.
pub struct TextArea {
    pub(super) widget: Widget,
    state: Arc<TextAreaState>,
    changed_callback: Option<Box<TextFieldChangedCallback>>,
    submitted_callback: Option<Box<TextFieldChangedCallback>>,
}

impl TextArea {
    pub fn new(placeholder: &str) -> Result<Self, Error> {
        let placeholder = sys::OneUiUtf8String::from_str(placeholder);
        let widget = Widget::from_raw(unsafe { sys::oneui_text_area_create_utf8(placeholder) })?;
        unsafe { sys::oneui_text_field_set_multiline(widget.as_raw(), 1) };
        Ok(Self {
            state: Arc::new(TextAreaState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_text: Mutex::new(None),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
            changed_callback: None,
            submitted_callback: None,
        })
    }

    pub fn set_text(&self, text: &str) {
        let text = sys::OneUiUtf8String::from_str(text);
        unsafe { sys::oneui_text_field_set_text_utf8(self.widget.as_raw(), text) };
    }

    pub fn undo(&self) -> bool {
        unsafe { sys::oneui_text_field_undo(self.widget.as_raw()) != 0 }
    }

    pub fn redo(&self) -> bool {
        unsafe { sys::oneui_text_field_redo(self.widget.as_raw()) != 0 }
    }

    pub fn set_read_only(&self, read_only: bool) {
        unsafe { sys::oneui_text_field_set_read_only(self.widget.as_raw(), i32::from(read_only)) };
    }

    pub fn set_line_height(&self, line_height: f32) {
        unsafe { sys::oneui_text_field_set_line_height(self.widget.as_raw(), line_height) };
    }

    pub fn set_vertical_scroll_offset(&self, offset: f32) {
        unsafe { sys::oneui_text_area_set_vertical_scroll_offset(self.widget.as_raw(), offset) };
    }

    pub fn vertical_scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_text_area_vertical_scroll_offset(self.widget.as_raw()) }
    }

    pub fn max_vertical_scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_text_area_max_vertical_scroll_offset(self.widget.as_raw()) }
    }

    pub fn scroll_to_top(&self) {
        unsafe { sys::oneui_text_area_scroll_to_top(self.widget.as_raw()) };
    }

    /// Chooses the chat-style multiline contract for this editor: Enter
    /// submits, while Shift+Enter continues to insert a newline. Ctrl+Enter
    /// remains available as an explicit submit chord in either mode.
    pub fn set_submit_on_enter(&self, submit_on_enter: bool) {
        unsafe {
            sys::oneui_text_field_set_submit_on_enter(
                self.widget.as_raw(),
                i32::from(submit_on_enter),
            )
        };
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        let trace = InteractionTrace::at("TextArea", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(TextFieldChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("text area callback was just installed")
            as *mut TextFieldChangedCallback)
            .cast();
        unsafe {
            sys::oneui_text_field_set_on_changed_utf8(
                self.widget.as_raw(),
                Some(run_text_field_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_text_field_set_on_changed_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.changed_callback = None;
    }

    /// Invokes `callback` for Ctrl+Enter, and also for plain Enter when
    /// `set_submit_on_enter(true)` is active. Shift+Enter remains a multiline
    /// edit. Active IME composition is never submitted through either path.
    #[track_caller]
    pub fn set_on_submitted<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        let trace = InteractionTrace::at("TextArea", "submitted", std::panic::Location::caller());
        self.clear_on_submitted();
        self.submitted_callback = Some(Box::new(TextFieldChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .submitted_callback
            .as_deref_mut()
            .expect("text area submit callback was just installed")
            as *mut TextFieldChangedCallback)
            .cast();
        unsafe {
            sys::oneui_text_field_set_on_submitted_utf8(
                self.widget.as_raw(),
                Some(run_text_field_submitted_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_submitted(&mut self) {
        unsafe {
            sys::oneui_text_field_set_on_submitted_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.submitted_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for TextArea {
    fn drop(&mut self) {
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_text
            .lock()
            .expect("text area pending text lock poisoned")
            .take();
        self.state.update_scheduled.store(false, Ordering::Release);
        self.clear_on_changed();
        self.clear_on_submitted();
    }
}
