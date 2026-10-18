//! Text field bindings.

use super::{
    run_callback_guarded, sys, trace_ui_task, traced_value_callback, Arc, AtomicBool, AtomicPtr,
    Error, IconSymbol, InteractionTrace, Mutex, Ordering, UiDispatcher, Widget,
};

impl UiDispatcher {
    /// Creates a thread-safe text producer for a single-line field mounted in this window.
    pub fn text_field_handle(&self, text_field: &TextField) -> TextFieldHandle {
        TextFieldHandle {
            state: Arc::clone(&text_field.state),
            dispatcher: self.clone(),
        }
    }
}

pub(super) struct TextFieldChangedCallback {
    pub(super) handler: Box<dyn FnMut(String) + 'static>,
}

pub(super) unsafe extern "C" fn run_text_field_changed_callback(
    text: *const std::ffi::c_char,
    length: usize,
    user_data: *mut std::ffi::c_void,
) {
    if text.is_null() || user_data.is_null() {
        return;
    }
    let bytes = unsafe { std::slice::from_raw_parts(text.cast::<u8>(), length) };
    let value = String::from_utf8_lossy(bytes).into_owned();
    let callback = unsafe { &mut *user_data.cast::<TextFieldChangedCallback>() };
    run_callback_guarded("text.changed", || (callback.handler)(value));
}

pub(super) unsafe extern "C" fn run_text_field_submitted_callback(
    text: *const std::ffi::c_char,
    length: usize,
    user_data: *mut std::ffi::c_void,
) {
    if text.is_null() || user_data.is_null() {
        return;
    }
    let bytes = unsafe { std::slice::from_raw_parts(text.cast::<u8>(), length) };
    let value = String::from_utf8_lossy(bytes).into_owned();
    let callback = unsafe { &mut *user_data.cast::<TextFieldChangedCallback>() };
    run_callback_guarded("text.submitted", || (callback.handler)(value));
}

struct TextFieldState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_text: Mutex<Option<String>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for single-line text field values owned by a window.
///
/// Worker updates are coalesced and applied on the owning UI thread.
#[derive(Clone)]
pub struct TextFieldHandle {
    state: Arc<TextFieldState>,
    dispatcher: UiDispatcher,
}

impl TextFieldHandle {
    pub fn set_text(&self, text: impl Into<String>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }

        *self
            .state
            .pending_text
            .lock()
            .expect("text field pending text lock poisoned") = Some(text.into());
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
                .expect("text field pending text lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_text(state: &TextFieldState) {
        loop {
            let text = state
                .pending_text
                .lock()
                .expect("text field pending text lock poisoned")
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(text) = text {
                let text = sys::OneUiUtf8String::from_str(&text);
                trace_ui_task("text field update started");
                unsafe { sys::oneui_text_field_set_text_utf8(raw, text) };
                trace_ui_task("text field update completed");
            }

            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_text
                .lock()
                .expect("text field pending text lock poisoned")
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

/// A basic UTF-8 text field with an owned text-change callback.
pub struct TextField {
    pub(super) widget: Widget,
    state: Arc<TextFieldState>,
    changed_callback: Option<Box<TextFieldChangedCallback>>,
    submitted_callback: Option<Box<TextFieldChangedCallback>>,
}

impl TextField {
    pub fn new(placeholder: &str) -> Result<Self, Error> {
        let placeholder = sys::OneUiUtf8String::from_str(placeholder);
        let widget = Widget::from_raw(unsafe { sys::oneui_text_field_create_utf8(placeholder) })?;
        Ok(Self {
            state: Arc::new(TextFieldState {
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

    pub fn set_font_size(&self, font_size: f32) {
        unsafe { sys::oneui_text_field_set_font_size(self.widget.as_raw(), font_size) };
    }

    pub fn set_password_mode(&self, enabled: bool) {
        unsafe {
            sys::oneui_text_field_set_password_mode(self.widget.as_raw(), i32::from(enabled))
        };
    }

    pub fn set_password_mask(&self, mask: char) {
        unsafe { sys::oneui_text_field_set_password_mask(self.widget.as_raw(), u32::from(mask)) };
    }

    pub fn set_prefix_icon(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_text_field_set_prefix_icon(self.widget.as_raw(), symbol as i32) };
    }

    pub fn set_placeholder_visible_when_focused(&self, visible: bool) {
        unsafe {
            sys::oneui_text_field_set_focused_placeholder(self.widget.as_raw(), visible as i32)
        };
    }

    pub fn clear_prefix_icon(&self) {
        unsafe { sys::oneui_text_field_clear_prefix_icon(self.widget.as_raw()) };
    }

    pub fn set_suffix_icon(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_text_field_set_suffix_icon(self.widget.as_raw(), symbol as i32) };
    }

    pub fn clear_suffix_icon(&self) {
        unsafe { sys::oneui_text_field_clear_suffix_icon(self.widget.as_raw()) };
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        let trace = InteractionTrace::at("TextField", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(TextFieldChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("text field callback was just installed")
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

    #[track_caller]
    pub fn set_on_submitted<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        let trace = InteractionTrace::at("TextField", "submitted", std::panic::Location::caller());
        self.clear_on_submitted();
        self.submitted_callback = Some(Box::new(TextFieldChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .submitted_callback
            .as_deref_mut()
            .expect("text field submit callback was just installed")
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

impl Drop for TextField {
    fn drop(&mut self) {
        self.clear_on_changed();
        self.clear_on_submitted();
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_text
            .lock()
            .expect("text field pending text lock poisoned")
            .take();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}
