//! Terminal bindings.

use super::choice_controls::{run_bool_changed_callback, BoolChangedCallback};
use super::key_event::RawKeyEvent;
use super::terminal_render::{
    apply_terminal_frame, apply_terminal_grid, apply_terminal_view_options,
};
use super::terminal_types::{
    TerminalAuxiliaryButtonAction, TerminalCell, TerminalColor, TerminalCursor,
    TerminalCursorStyle, TerminalFrame, TerminalPointerEvent, TerminalSelection,
    TerminalViewOptions, TerminalViewport,
};
use super::visual_style::Rect;
use super::{
    run_callback_guarded, run_void_callback, sys, traced_value_callback, Arc, AtomicBool,
    AtomicPtr, Error, InteractionTrace, Mutex, Ordering, Rc, RefCell, UiDispatcher, VoidCallback,
    Widget,
};

impl UiDispatcher {
    /// Creates a thread-safe frame producer for a terminal in this window.
    pub fn terminal_view_handle(&self, terminal: &TerminalView) -> TerminalViewHandle {
        TerminalViewHandle {
            state: Arc::clone(&terminal.state),
            dispatcher: self.clone(),
        }
    }
}

struct TerminalTextInputCallback {
    handler: Box<dyn FnMut(String) + 'static>,
}

struct TerminalRawKeyCallback {
    handler: Box<dyn FnMut(RawKeyEvent) + 'static>,
}

struct TerminalScrollCallback {
    handler: Box<dyn FnMut(i32) + 'static>,
}

struct TerminalPointerCallback {
    handler: Box<dyn FnMut(TerminalPointerEvent) + 'static>,
}

struct TerminalHyperlinkCallback {
    handler: Box<dyn FnMut(u32) + 'static>,
}

struct TerminalViewportCallback {
    handler: Box<dyn FnMut(TerminalViewport) + 'static>,
}

struct TerminalViewState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_update: Mutex<Option<TerminalFrameUpdate>>,
    last_applied_frame: Mutex<Option<TerminalFrame>>,
    update_scheduled: AtomicBool,
    honor_application_cursor_style: AtomicBool,
}

#[derive(Debug, Clone)]
struct TerminalFrameUpdate {
    frame: TerminalFrame,
    selection: TerminalSelectionUpdate,
}

#[derive(Debug, Clone, Copy)]
enum TerminalSelectionUpdate {
    Preserve,
    Set(TerminalSelection),
    Clear,
}

/// A thread-safe producer for the latest native terminal frame.
///
/// A session worker may submit frames from any thread. OneUI applies them only
/// through the owning window's dispatcher, coalescing bursts to the newest
/// frame so terminal output cannot grow an unbounded UI work queue.
#[derive(Clone)]
pub struct TerminalViewHandle {
    state: Arc<TerminalViewState>,
    dispatcher: UiDispatcher,
}

impl TerminalViewHandle {
    pub fn apply_options(&self, options: TerminalViewOptions) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        self.state
            .honor_application_cursor_style
            .store(options.honor_application_cursor_style, Ordering::Release);
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                apply_terminal_view_options(raw, &options);
            }
        })
    }

    /// Updates terminal metrics on the owning window thread.
    pub fn set_font_size(&self, size: f32) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe { sys::oneui_terminal_view_set_font_size(raw, size) };
            }
        })
    }

    /// Updates the preferred terminal font family on the owning window thread.
    pub fn set_font_family(&self, family: impl Into<String>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        let family = family.into();
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe {
                    sys::oneui_terminal_view_set_font_family_utf8(
                        raw,
                        sys::OneUiUtf8String::from_str(&family),
                    )
                };
            }
        })
    }

    pub fn set_letter_spacing(&self, pixels: f32) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe { sys::oneui_terminal_view_set_letter_spacing(raw, pixels) };
            }
        })
    }

    pub fn set_line_numbers_visible(&self, visible: bool) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe {
                    sys::oneui_terminal_view_set_line_numbers_visible(raw, i32::from(visible))
                };
            }
        })
    }

    pub fn submit_frame(&self, frame: TerminalFrame) -> Result<(), Error> {
        self.submit_update(frame, TerminalSelectionUpdate::Preserve)
    }

    /// Atomically applies a terminal frame and its matching search highlight.
    pub fn submit_frame_with_selection(
        &self,
        frame: TerminalFrame,
        selection: TerminalSelection,
    ) -> Result<(), Error> {
        self.submit_update(frame, TerminalSelectionUpdate::Set(selection))
    }

    /// Clears a programmatic terminal selection on the owning window thread.
    pub fn submit_frame_clearing_selection(&self, frame: TerminalFrame) -> Result<(), Error> {
        self.submit_update(frame, TerminalSelectionUpdate::Clear)
    }

    fn submit_update(
        &self,
        frame: TerminalFrame,
        selection: TerminalSelectionUpdate,
    ) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }

        *self
            .state
            .pending_update
            .lock()
            .expect("terminal pending update lock poisoned") =
            Some(TerminalFrameUpdate { frame, selection });

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
            state
                .pending_update
                .lock()
                .expect("terminal pending update lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_frame(state: &TerminalViewState) {
        loop {
            let update = state
                .pending_update
                .lock()
                .expect("terminal pending update lock poisoned")
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(update) = update {
                let mut last_applied = state
                    .last_applied_frame
                    .lock()
                    .expect("terminal last applied frame lock poisoned");
                apply_terminal_frame(
                    raw,
                    &update.frame,
                    last_applied.as_ref(),
                    state.honor_application_cursor_style.load(Ordering::Acquire),
                );
                match update.selection {
                    TerminalSelectionUpdate::Preserve => {}
                    TerminalSelectionUpdate::Set(selection) => unsafe {
                        sys::oneui_terminal_view_set_selection(
                            raw,
                            selection.start_row,
                            selection.start_column,
                            selection.end_row,
                            selection.end_column,
                        )
                    },
                    TerminalSelectionUpdate::Clear => unsafe {
                        sys::oneui_terminal_view_clear_selection(raw)
                    },
                }
                *last_applied = Some(update.frame);
            }

            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_update
                .lock()
                .expect("terminal pending update lock poisoned")
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

unsafe extern "C" fn run_terminal_text_input_callback(
    text: *const std::ffi::c_char,
    length: usize,
    user_data: *mut std::ffi::c_void,
) {
    if text.is_null() || user_data.is_null() {
        return;
    }
    let bytes = unsafe { std::slice::from_raw_parts(text.cast::<u8>(), length) };
    let value = String::from_utf8_lossy(bytes).into_owned();
    let callback = unsafe { &mut *user_data.cast::<TerminalTextInputCallback>() };
    run_callback_guarded("terminal.text_input", || (callback.handler)(value));
}

unsafe extern "C" fn run_terminal_raw_key_callback(
    event: *const sys::OneUiRawKeyEvent,
    user_data: *mut std::ffi::c_void,
) {
    if event.is_null() || user_data.is_null() {
        return;
    }
    let event = RawKeyEvent::from(unsafe { *event });
    let callback = unsafe { &mut *user_data.cast::<TerminalRawKeyCallback>() };
    run_callback_guarded("terminal.raw_key", || (callback.handler)(event));
}

unsafe extern "C" fn run_terminal_scroll_callback(rows: i32, user_data: *mut std::ffi::c_void) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<TerminalScrollCallback>() };
    run_callback_guarded("terminal.scroll", || (callback.handler)(rows));
}

unsafe extern "C" fn run_terminal_pointer_callback(
    event: *const sys::OneUiTerminalPointerEvent,
    user_data: *mut std::ffi::c_void,
) {
    if event.is_null() || user_data.is_null() {
        return;
    }
    let event = TerminalPointerEvent::from(unsafe { *event });
    let callback = unsafe { &mut *user_data.cast::<TerminalPointerCallback>() };
    run_callback_guarded("terminal.pointer", || (callback.handler)(event));
}

unsafe extern "C" fn run_terminal_hyperlink_callback(
    hyperlink_id: u32,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<TerminalHyperlinkCallback>() };
    run_callback_guarded("terminal.hyperlink", || (callback.handler)(hyperlink_id));
}

unsafe extern "C" fn run_terminal_viewport_callback(
    rows: u16,
    columns: u16,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<TerminalViewportCallback>() };
    run_callback_guarded("terminal.viewport", || {
        (callback.handler)(TerminalViewport { rows, columns })
    });
}

/// Native terminal grid with explicit frame and input boundaries.
///
/// The `TerminalView` itself stays on the UI thread. Its callbacks are owned
/// by the view and are cleared in `Drop` before OneUI destroys the widget.
pub struct TerminalView {
    widget: Widget,
    state: Arc<TerminalViewState>,
    text_input_callback: Option<Box<TerminalTextInputCallback>>,
    paste_callback: Option<Box<TerminalTextInputCallback>>,
    raw_key_callback: Option<Box<TerminalRawKeyCallback>>,
    scroll_callback: Option<Box<TerminalScrollCallback>>,
    pointer_callback: Option<Box<TerminalPointerCallback>>,
    hyperlink_callback: Option<Box<TerminalHyperlinkCallback>>,
    viewport_callback: Option<Box<TerminalViewportCallback>>,
    cursor_callback: Option<Box<VoidCallback>>,
    focus_callback: Option<Box<BoolChangedCallback>>,
}

impl TerminalView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_terminal_view_create() })?;
        Ok(Self {
            state: Arc::new(TerminalViewState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_update: Mutex::new(None),
                last_applied_frame: Mutex::new(None),
                update_scheduled: AtomicBool::new(false),
                honor_application_cursor_style: AtomicBool::new(false),
            }),
            widget,
            text_input_callback: None,
            paste_callback: None,
            raw_key_callback: None,
            scroll_callback: None,
            pointer_callback: None,
            hyperlink_callback: None,
            viewport_callback: None,
            cursor_callback: None,
            focus_callback: None,
        })
    }

    pub fn set_font_size(&self, size: f32) {
        unsafe { sys::oneui_terminal_view_set_font_size(self.widget.as_raw(), size) };
    }

    pub fn apply_options(&self, options: &TerminalViewOptions) {
        self.state
            .honor_application_cursor_style
            .store(options.honor_application_cursor_style, Ordering::Release);
        apply_terminal_view_options(self.widget.as_raw(), options);
    }

    pub fn set_font_family(&self, family: &str) {
        unsafe {
            sys::oneui_terminal_view_set_font_family_utf8(
                self.widget.as_raw(),
                sys::OneUiUtf8String::from_str(family),
            )
        };
    }

    pub fn set_line_height(&self, multiplier: f32) {
        unsafe { sys::oneui_terminal_view_set_line_height(self.widget.as_raw(), multiplier) };
    }

    pub fn set_letter_spacing(&self, pixels: f32) {
        unsafe { sys::oneui_terminal_view_set_letter_spacing(self.widget.as_raw(), pixels) };
    }

    pub fn set_line_numbers_visible(&self, visible: bool) {
        unsafe {
            sys::oneui_terminal_view_set_line_numbers_visible(
                self.widget.as_raw(),
                i32::from(visible),
            )
        };
    }

    pub fn set_first_visible_line_number(&self, line_number: u64) {
        unsafe {
            sys::oneui_terminal_view_set_first_visible_line_number(
                self.widget.as_raw(),
                line_number,
            )
        };
    }

    pub fn set_cursor_style(&self, style: TerminalCursorStyle) {
        unsafe { sys::oneui_terminal_view_set_cursor_style(self.widget.as_raw(), style as i32) };
    }

    pub fn set_cursor_blinking(&self, enabled: bool) {
        unsafe {
            sys::oneui_terminal_view_set_cursor_blinking(self.widget.as_raw(), i32::from(enabled))
        };
    }

    /// Visual bell: a short fading tint over the terminal.
    pub fn flash_bell(&self) {
        unsafe { sys::oneui_terminal_view_flash_bell(self.widget.as_raw()) };
    }

    pub fn set_copy_on_select(&self, enabled: bool) {
        unsafe {
            sys::oneui_terminal_view_set_copy_on_select(self.widget.as_raw(), i32::from(enabled))
        };
    }

    pub fn set_right_button_action(&self, action: TerminalAuxiliaryButtonAction) {
        unsafe {
            sys::oneui_terminal_view_set_right_button_action(self.widget.as_raw(), action as i32)
        };
    }

    pub fn set_middle_button_action(&self, action: TerminalAuxiliaryButtonAction) {
        unsafe {
            sys::oneui_terminal_view_set_middle_button_action(self.widget.as_raw(), action as i32)
        };
    }

    pub fn set_palette(
        &self,
        background: TerminalColor,
        foreground: TerminalColor,
        cursor: TerminalColor,
    ) {
        unsafe {
            sys::oneui_terminal_view_set_palette(
                self.widget.as_raw(),
                background.into(),
                foreground.into(),
                cursor.into(),
            )
        };
    }

    pub fn set_grid(&self, rows: u16, columns: u16, cells: &[TerminalCell]) {
        apply_terminal_grid(self.widget.as_raw(), rows, columns, cells);
    }

    pub fn set_cursor(&self, cursor: TerminalCursor) {
        unsafe {
            sys::oneui_terminal_view_set_cursor(
                self.widget.as_raw(),
                cursor.row,
                cursor.column,
                i32::from(cursor.visible),
            )
        };
    }

    /// Returns the native text-input caret rectangle in logical window coordinates.
    ///
    /// Anchored overlays such as completion lists and IME candidates should use
    /// this geometry instead of reconstructing terminal cell metrics.
    pub fn text_input_caret_rect(&self) -> Option<Rect> {
        let mut rect = sys::OneUiRect::default();
        let available = unsafe {
            sys::oneui_terminal_view_text_input_caret_rect(self.widget.as_raw(), &mut rect)
        };
        (available != 0).then_some(Rect {
            x: rect.x,
            y: rect.y,
            width: rect.width,
            height: rect.height,
        })
    }

    pub fn set_scroll_rows_per_wheel(&self, rows: f32) {
        unsafe { sys::oneui_terminal_view_set_scroll_rows_per_wheel(self.widget.as_raw(), rows) };
    }

    pub fn set_mouse_reporting(&self, enabled: bool) {
        unsafe {
            sys::oneui_terminal_view_set_mouse_reporting(self.widget.as_raw(), i32::from(enabled))
        };
    }

    pub fn select_all(&self) {
        unsafe { sys::oneui_terminal_view_select_all(self.widget.as_raw()) };
    }

    pub fn copy_selection(&self) -> bool {
        unsafe { sys::oneui_terminal_view_copy_selection(self.widget.as_raw()) != 0 }
    }

    pub fn paste_clipboard(&self) -> bool {
        unsafe { sys::oneui_terminal_view_paste_clipboard(self.widget.as_raw()) != 0 }
    }

    /// Routes programmatic paste text through the same callback used by the
    /// native clipboard command, preserving application paste policy.
    pub fn paste_text(&self, text: &str) -> bool {
        unsafe {
            sys::oneui_terminal_view_paste_utf8(
                self.widget.as_raw(),
                sys::OneUiUtf8String::from_str(text),
            ) != 0
        }
    }

    pub fn set_selection(&self, selection: TerminalSelection) {
        unsafe {
            sys::oneui_terminal_view_set_selection(
                self.widget.as_raw(),
                selection.start_row,
                selection.start_column,
                selection.end_row,
                selection.end_column,
            )
        };
    }

    pub fn clear_selection(&self) {
        unsafe { sys::oneui_terminal_view_clear_selection(self.widget.as_raw()) };
    }

    pub fn has_selection(&self) -> bool {
        unsafe { sys::oneui_terminal_view_has_selection(self.widget.as_raw()) != 0 }
    }

    pub fn selected_text(&self) -> String {
        let required = unsafe {
            sys::oneui_terminal_view_get_selected_text_utf8(
                self.widget.as_raw(),
                std::ptr::null_mut(),
                0,
            )
        };
        if required == 0 {
            return String::new();
        }
        let mut buffer = vec![0u8; required];
        let written = unsafe {
            sys::oneui_terminal_view_get_selected_text_utf8(
                self.widget.as_raw(),
                buffer.as_mut_ptr().cast(),
                buffer.len(),
            )
        };
        if written == 0 {
            return String::new();
        }
        if buffer.last() == Some(&0) {
            buffer.pop();
        }
        String::from_utf8_lossy(&buffer).into_owned()
    }

    #[track_caller]
    pub fn set_on_text_input<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        let trace =
            InteractionTrace::at("TerminalView", "text_input", std::panic::Location::caller());
        self.clear_text_input_callback();
        self.text_input_callback = Some(Box::new(TerminalTextInputCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .text_input_callback
            .as_deref_mut()
            .expect("terminal text callback was just installed")
            as *mut TerminalTextInputCallback)
            .cast();
        unsafe {
            sys::oneui_terminal_view_set_on_text_input_utf8(
                self.widget.as_raw(),
                Some(run_terminal_text_input_callback),
                user_data,
            )
        };
    }

    #[track_caller]
    pub fn set_on_paste<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        let trace = InteractionTrace::at("TerminalView", "paste", std::panic::Location::caller());
        self.clear_paste_callback();
        self.paste_callback = Some(Box::new(TerminalTextInputCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .paste_callback
            .as_deref_mut()
            .expect("terminal paste callback was just installed")
            as *mut TerminalTextInputCallback)
            .cast();
        unsafe {
            sys::oneui_terminal_view_set_on_paste_utf8(
                self.widget.as_raw(),
                Some(run_terminal_text_input_callback),
                user_data,
            )
        };
    }

    #[track_caller]
    pub fn set_on_raw_key<F>(&mut self, callback: F)
    where
        F: FnMut(RawKeyEvent) + 'static,
    {
        let trace = InteractionTrace::at("TerminalView", "raw_key", std::panic::Location::caller());
        self.clear_raw_key_callback();
        self.raw_key_callback = Some(Box::new(TerminalRawKeyCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .raw_key_callback
            .as_deref_mut()
            .expect("terminal raw key callback was just installed")
            as *mut TerminalRawKeyCallback)
            .cast();
        unsafe {
            sys::oneui_terminal_view_set_on_raw_key(
                self.widget.as_raw(),
                Some(run_terminal_raw_key_callback),
                user_data,
            )
        };
    }

    #[track_caller]
    pub fn set_on_scroll<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("TerminalView", "scroll", std::panic::Location::caller());
        self.clear_scroll_callback();
        self.scroll_callback = Some(Box::new(TerminalScrollCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .scroll_callback
            .as_deref_mut()
            .expect("terminal scroll callback was just installed")
            as *mut TerminalScrollCallback)
            .cast();
        unsafe {
            sys::oneui_terminal_view_set_on_scroll(
                self.widget.as_raw(),
                Some(run_terminal_scroll_callback),
                user_data,
            )
        };
    }

    #[track_caller]
    pub fn set_on_pointer<F>(&mut self, callback: F)
    where
        F: FnMut(TerminalPointerEvent) + 'static,
    {
        let trace = InteractionTrace::at("TerminalView", "pointer", std::panic::Location::caller());
        self.clear_pointer_callback();
        self.pointer_callback = Some(Box::new(TerminalPointerCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .pointer_callback
            .as_deref_mut()
            .expect("terminal pointer callback was just installed")
            as *mut TerminalPointerCallback)
            .cast();
        unsafe {
            sys::oneui_terminal_view_set_on_pointer(
                self.widget.as_raw(),
                Some(run_terminal_pointer_callback),
                user_data,
            )
        };
    }

    /// Installs the activation callback for OSC 8 cells. OneUI emits this
    /// only after a completed Ctrl+left-click on the same hyperlink.
    #[track_caller]
    pub fn set_on_hyperlink<F>(&mut self, callback: F)
    where
        F: FnMut(u32) + 'static,
    {
        let trace =
            InteractionTrace::at("TerminalView", "hyperlink", std::panic::Location::caller());
        self.clear_hyperlink_callback();
        self.hyperlink_callback = Some(Box::new(TerminalHyperlinkCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .hyperlink_callback
            .as_deref_mut()
            .expect("terminal hyperlink callback was just installed")
            as *mut TerminalHyperlinkCallback)
            .cast();
        unsafe {
            sys::oneui_terminal_view_set_on_hyperlink(
                self.widget.as_raw(),
                Some(run_terminal_hyperlink_callback),
                user_data,
            )
        };
    }

    #[track_caller]
    pub fn set_on_viewport_changed<F>(&mut self, callback: F)
    where
        F: FnMut(TerminalViewport) + 'static,
    {
        let trace = InteractionTrace::at(
            "TerminalView",
            "viewport_changed",
            std::panic::Location::caller(),
        );
        self.clear_viewport_callback();
        self.viewport_callback = Some(Box::new(TerminalViewportCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .viewport_callback
            .as_deref_mut()
            .expect("terminal viewport callback was just installed")
            as *mut TerminalViewportCallback)
            .cast();
        unsafe {
            sys::oneui_terminal_view_set_on_viewport_changed(
                self.widget.as_raw(),
                Some(run_terminal_viewport_callback),
                user_data,
            )
        };
    }

    #[track_caller]
    pub fn set_on_focus_changed<F>(&mut self, callback: F)
    where
        F: FnMut(bool) + 'static,
    {
        let trace = InteractionTrace::at(
            "TerminalView",
            "focus_changed",
            std::panic::Location::caller(),
        );
        self.clear_focus_callback();
        self.focus_callback = Some(Box::new(BoolChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .focus_callback
            .as_deref_mut()
            .expect("terminal focus callback was just installed")
            as *mut BoolChangedCallback)
            .cast();
        unsafe {
            sys::oneui_terminal_view_set_on_focus_changed(
                self.widget.as_raw(),
                Some(run_bool_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_text_input_callback(&mut self) {
        unsafe {
            sys::oneui_terminal_view_set_on_text_input_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.text_input_callback = None;
    }

    pub fn clear_paste_callback(&mut self) {
        unsafe {
            sys::oneui_terminal_view_set_on_paste_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.paste_callback = None;
    }

    pub fn clear_raw_key_callback(&mut self) {
        unsafe {
            sys::oneui_terminal_view_set_on_raw_key(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.raw_key_callback = None;
    }

    pub fn clear_scroll_callback(&mut self) {
        unsafe {
            sys::oneui_terminal_view_set_on_scroll(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.scroll_callback = None;
    }

    pub fn clear_pointer_callback(&mut self) {
        unsafe {
            sys::oneui_terminal_view_set_on_pointer(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.pointer_callback = None;
    }

    pub fn clear_hyperlink_callback(&mut self) {
        unsafe {
            sys::oneui_terminal_view_set_on_hyperlink(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.hyperlink_callback = None;
    }

    /// UI-thread notification after the terminal cursor moves (including remote echo).
    pub fn set_on_cursor_changed<F: FnMut() + 'static>(&mut self, callback: F) {
        self.clear_cursor_callback();
        self.cursor_callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(callback))),
        }));
        let data = (self.cursor_callback.as_deref_mut().unwrap() as *mut VoidCallback).cast();
        unsafe {
            sys::oneui_terminal_view_set_on_cursor_changed(
                self.widget.as_raw(),
                Some(run_void_callback),
                data,
            );
        }
    }

    pub fn clear_cursor_callback(&mut self) {
        unsafe {
            sys::oneui_terminal_view_set_on_cursor_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            );
        }
        self.cursor_callback = None;
    }

    pub fn clear_viewport_callback(&mut self) {
        unsafe {
            sys::oneui_terminal_view_set_on_viewport_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.viewport_callback = None;
    }

    pub fn clear_focus_callback(&mut self) {
        unsafe {
            sys::oneui_terminal_view_set_on_focus_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.focus_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for TerminalView {
    fn drop(&mut self) {
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_update
            .lock()
            .expect("terminal pending update lock poisoned")
            .take();
        self.state
            .last_applied_frame
            .lock()
            .expect("terminal last applied frame lock poisoned")
            .take();
        self.state.update_scheduled.store(false, Ordering::Release);
        self.clear_text_input_callback();
        self.clear_paste_callback();
        self.clear_raw_key_callback();
        self.clear_scroll_callback();
        self.clear_pointer_callback();
        self.clear_hyperlink_callback();
        self.clear_viewport_callback();
        self.clear_cursor_callback();
        self.clear_focus_callback();
    }
}
