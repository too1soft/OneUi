//! Safe foundation bindings for OneUI.
//!
//! This crate owns the safe Rust boundary for OneUI's UTF-8 C ABI. It begins
//! with window lifetime and grows reusable controls without exposing raw FFI to
//! product applications.

use std::marker::PhantomData;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr::NonNull;
use std::rc::Rc;
use std::sync::{
    atomic::{AtomicBool, AtomicPtr, Ordering},
    Arc, Mutex,
};

pub use oneui_sys as sys;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    AbiVersionMismatch { expected: u32, actual: u32 },
    WindowCreationFailed,
    WidgetCreationFailed,
    WidgetDestroyed,
    WindowClosed,
}

#[derive(Debug, Clone)]
pub struct WindowOptions {
    pub title: String,
    pub width: i32,
    pub height: i32,
    pub visible: bool,
    pub borderless: bool,
    pub fullscreen: bool,
    pub topmost: bool,
    pub resizable: bool,
}

impl Default for WindowOptions {
    fn default() -> Self {
        Self {
            title: "OneUI".to_owned(),
            width: 1280,
            height: 800,
            visible: false,
            borderless: false,
            fullscreen: false,
            topmost: false,
            resizable: true,
        }
    }
}

pub struct Window {
    state: Arc<WindowState>,
    _ui_thread: PhantomData<Rc<()>>,
}

struct WindowState {
    raw: Mutex<Option<NonNull<sys::OneUiWindow>>>,
}

#[derive(Clone)]
pub struct UiDispatcher {
    state: Arc<WindowState>,
}

struct DispatchedTask {
    task: Option<Box<dyn FnOnce() + Send + 'static>>,
}

unsafe impl Send for UiDispatcher {}
unsafe impl Sync for UiDispatcher {}

impl WindowState {
    fn current_raw(&self) -> Option<NonNull<sys::OneUiWindow>> {
        *self.raw.lock().expect("OneUI window state lock poisoned")
    }

    fn with_raw<R>(&self, action: impl FnOnce(*mut sys::OneUiWindow) -> R) -> Option<R> {
        let raw = self.raw.lock().expect("OneUI window state lock poisoned");
        raw.as_ref().map(|raw| action(raw.as_ptr()))
    }

    fn destroy(&self) {
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
        let _ = catch_unwind(AssertUnwindSafe(task));
    }
}

unsafe extern "C" fn drop_dispatched_task(user_data: *mut std::ffi::c_void) {
    drop(unsafe { Box::from_raw(user_data.cast::<DispatchedTask>()) });
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

    pub fn request_close(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_request_close(raw);
        });
    }

    pub fn request_activate(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_activate(raw);
        });
    }
}

pub struct Widget {
    raw: NonNull<sys::OneUiWidget>,
}

impl Widget {
    fn from_raw(raw: *mut sys::OneUiWidget) -> Result<Self, Error> {
        let raw = NonNull::new(raw).ok_or(Error::WidgetCreationFailed)?;
        Ok(Self { raw })
    }

    fn as_raw(&self) -> *mut sys::OneUiWidget {
        self.raw.as_ptr()
    }
}

impl Drop for Widget {
    fn drop(&mut self) {
        unsafe { sys::oneui_widget_destroy(self.raw.as_ptr()) };
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StackDirection {
    Column,
    Row,
}

#[derive(Debug, Clone, Copy, Default)]
pub struct Insets {
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub left: f32,
}

impl From<Insets> for sys::OneUiInsets {
    fn from(value: Insets) -> Self {
        Self {
            top: value.top,
            right: value.right,
            bottom: value.bottom,
            left: value.left,
        }
    }
}

pub struct Stack {
    widget: Widget,
}

impl Stack {
    pub fn new(direction: StackDirection) -> Result<Self, Error> {
        let direction = match direction {
            StackDirection::Column => 0,
            StackDirection::Row => 1,
        };
        let widget = Widget::from_raw(unsafe { sys::oneui_stack_create(direction) })?;
        Ok(Self { widget })
    }

    pub fn add(&self, child: &Widget) {
        unsafe { sys::oneui_stack_add(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_gap(&self, gap: f32) {
        unsafe { sys::oneui_stack_set_gap(self.widget.as_raw(), gap) };
    }

    pub fn set_padding(&self, padding: Insets) {
        unsafe { sys::oneui_stack_set_padding(self.widget.as_raw(), padding.into()) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

/// A scroll container whose child remains owned by the Rust composition tree.
pub struct ScrollView {
    widget: Widget,
}

impl ScrollView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_scroll_view_create() })?;
        Ok(Self { widget })
    }

    pub fn set_content(&self, child: &Widget) {
        unsafe { sys::oneui_scroll_view_set_content(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_content_height(&self, height: f32) {
        unsafe { sys::oneui_scroll_view_set_content_height(self.widget.as_raw(), height) };
    }

    pub fn set_wheel_step(&self, step: f32) {
        unsafe { sys::oneui_scroll_view_set_wheel_step(self.widget.as_raw(), step) };
    }

    pub fn scroll_to_bottom(&self) {
        unsafe { sys::oneui_scroll_view_scroll_to_bottom(self.widget.as_raw()) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

pub struct Label {
    widget: Widget,
}

impl Label {
    pub fn new(text: &str) -> Result<Self, Error> {
        let text = sys::OneUiUtf8String::from_str(text);
        let widget = Widget::from_raw(unsafe { sys::oneui_label_create_utf8(text) })?;
        Ok(Self { widget })
    }

    pub fn set_text(&self, text: &str) {
        let text = sys::OneUiUtf8String::from_str(text);
        unsafe { sys::oneui_label_set_text_utf8(self.widget.as_raw(), text) };
    }

    pub fn set_font_size(&self, font_size: f32) {
        unsafe { sys::oneui_label_set_font_size(self.widget.as_raw(), font_size) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

/// A standard command button without an application-specific callback model.
pub struct Button {
    widget: Widget,
}

impl Button {
    pub fn new(text: &str) -> Result<Self, Error> {
        let text = sys::OneUiUtf8String::from_str(text);
        let widget = Widget::from_raw(unsafe { sys::oneui_button_create_utf8(text) })?;
        Ok(Self { widget })
    }

    pub fn set_text(&self, text: &str) {
        let text = sys::OneUiUtf8String::from_str(text);
        unsafe { sys::oneui_button_set_text_utf8(self.widget.as_raw(), text) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

/// A basic UTF-8 text field. Callback ownership is intentionally deferred
/// until the safe callback lifetime model is added for all controls together.
pub struct TextField {
    widget: Widget,
}

impl TextField {
    pub fn new(placeholder: &str) -> Result<Self, Error> {
        let placeholder = sys::OneUiUtf8String::from_str(placeholder);
        let widget = Widget::from_raw(unsafe { sys::oneui_text_field_create_utf8(placeholder) })?;
        Ok(Self { widget })
    }

    pub fn set_text(&self, text: &str) {
        let text = sys::OneUiUtf8String::from_str(text);
        unsafe { sys::oneui_text_field_set_text_utf8(self.widget.as_raw(), text) };
    }

    pub fn set_read_only(&self, read_only: bool) {
        unsafe { sys::oneui_text_field_set_read_only(self.widget.as_raw(), i32::from(read_only)) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

/// RGBA color used by the native terminal grid.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl TerminalColor {
    pub const fn rgb(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b, a: 255 }
    }
}

impl Default for TerminalColor {
    fn default() -> Self {
        Self::rgb(220, 226, 240)
    }
}

impl From<TerminalColor> for sys::OneUiColor {
    fn from(value: TerminalColor) -> Self {
        Self {
            r: value.r,
            g: value.g,
            b: value.b,
            a: value.a,
        }
    }
}

/// Terminal cell style flags mirrored by OneUI's stable C ABI.
pub mod terminal_style {
    pub const BOLD: u32 = 1 << 0;
    pub const DIM: u32 = 1 << 1;
    pub const ITALIC: u32 = 1 << 2;
    pub const UNDERLINE: u32 = 1 << 3;
    pub const INVERSE: u32 = 1 << 4;
    pub const WIDE: u32 = 1 << 5;
    pub const WIDE_CONTINUATION: u32 = 1 << 6;
}

/// One terminal cell. It deliberately holds text and colors separately so a
/// caller never has to encode values into a string boundary.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TerminalCell {
    pub text: String,
    pub foreground: TerminalColor,
    pub background: TerminalColor,
    pub style: u32,
}

impl Default for TerminalCell {
    fn default() -> Self {
        Self {
            text: String::new(),
            foreground: TerminalColor::default(),
            background: TerminalColor::rgb(20, 24, 36),
            style: 0,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalCursor {
    pub row: u16,
    pub column: u16,
    pub visible: bool,
}

/// An owned terminal snapshot that can be submitted from a session worker.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TerminalFrame {
    pub rows: u16,
    pub columns: u16,
    pub cells: Vec<TerminalCell>,
    pub cursor: TerminalCursor,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RawKeyEvent {
    pub virtual_key: u32,
    pub scan_code: u32,
    pub pressed: bool,
    pub repeat: bool,
    pub extended: bool,
    pub alt: bool,
    pub ctrl: bool,
    pub shift: bool,
    pub win: bool,
}

impl From<sys::OneUiRawKeyEvent> for RawKeyEvent {
    fn from(value: sys::OneUiRawKeyEvent) -> Self {
        Self {
            virtual_key: value.virtual_key,
            scan_code: value.scan_code,
            pressed: value.pressed != 0,
            repeat: value.repeat != 0,
            extended: value.extended != 0,
            alt: value.alt != 0,
            ctrl: value.ctrl != 0,
            shift: value.shift != 0,
            win: value.win != 0,
        }
    }
}

struct TerminalTextInputCallback {
    handler: Box<dyn FnMut(String) + 'static>,
}

struct TerminalRawKeyCallback {
    handler: Box<dyn FnMut(RawKeyEvent) + 'static>,
}

struct TerminalViewState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_frame: Mutex<Option<TerminalFrame>>,
    update_scheduled: AtomicBool,
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
    pub fn submit_frame(&self, frame: TerminalFrame) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }

        *self
            .state
            .pending_frame
            .lock()
            .expect("terminal pending frame lock poisoned") = Some(frame);

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
                .pending_frame
                .lock()
                .expect("terminal pending frame lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_frame(state: &TerminalViewState) {
        loop {
            let frame = state
                .pending_frame
                .lock()
                .expect("terminal pending frame lock poisoned")
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(frame) = frame {
                apply_terminal_frame(raw, &frame);
            }

            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_frame
                .lock()
                .expect("terminal pending frame lock poisoned")
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
    let _ = catch_unwind(AssertUnwindSafe(|| (callback.handler)(value)));
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
    let _ = catch_unwind(AssertUnwindSafe(|| (callback.handler)(event)));
}

/// Native terminal grid with explicit frame and input boundaries.
///
/// The `TerminalView` itself stays on the UI thread. Its callbacks are owned
/// by the view and are cleared in `Drop` before OneUI destroys the widget.
pub struct TerminalView {
    widget: Widget,
    state: Arc<TerminalViewState>,
    text_input_callback: Option<Box<TerminalTextInputCallback>>,
    raw_key_callback: Option<Box<TerminalRawKeyCallback>>,
}

impl TerminalView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_terminal_view_create() })?;
        Ok(Self {
            state: Arc::new(TerminalViewState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_frame: Mutex::new(None),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
            text_input_callback: None,
            raw_key_callback: None,
        })
    }

    pub fn set_font_size(&self, size: f32) {
        unsafe { sys::oneui_terminal_view_set_font_size(self.widget.as_raw(), size) };
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

    pub fn set_on_text_input<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        self.clear_text_input_callback();
        self.text_input_callback = Some(Box::new(TerminalTextInputCallback {
            handler: Box::new(callback),
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

    pub fn set_on_raw_key<F>(&mut self, callback: F)
    where
        F: FnMut(RawKeyEvent) + 'static,
    {
        self.clear_raw_key_callback();
        self.raw_key_callback = Some(Box::new(TerminalRawKeyCallback {
            handler: Box::new(callback),
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
            .pending_frame
            .lock()
            .expect("terminal pending frame lock poisoned")
            .take();
        self.state.update_scheduled.store(false, Ordering::Release);
        self.clear_text_input_callback();
        self.clear_raw_key_callback();
    }
}

fn apply_terminal_frame(raw: *mut sys::OneUiWidget, frame: &TerminalFrame) {
    apply_terminal_grid(raw, frame.rows, frame.columns, &frame.cells);
    unsafe {
        sys::oneui_terminal_view_set_cursor(
            raw,
            frame.cursor.row,
            frame.cursor.column,
            i32::from(frame.cursor.visible),
        )
    };
}

fn apply_terminal_grid(
    raw: *mut sys::OneUiWidget,
    rows: u16,
    columns: u16,
    cells: &[TerminalCell],
) {
    let native_cells: Vec<sys::OneUiTerminalCellUtf8> = cells
        .iter()
        .map(|cell| sys::OneUiTerminalCellUtf8 {
            text: sys::OneUiUtf8String::from_str(&cell.text),
            foreground: cell.foreground.into(),
            background: cell.background.into(),
            style: cell.style,
        })
        .collect();
    unsafe {
        sys::oneui_terminal_view_set_grid_utf8(
            raw,
            rows,
            columns,
            native_cells.as_ptr(),
            native_cells.len(),
        )
    };
}

/// Structured list data. Every field is UTF-8 and may contain punctuation,
/// tabs, or newlines without relying on a delimiter encoding.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ListItem {
    pub title: String,
    pub detail: String,
}

/// A selectable native list.
pub struct List {
    widget: Widget,
}

impl List {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_list_create() })?;
        Ok(Self { widget })
    }

    pub fn set_items(&self, items: &[ListItem]) {
        let native_items: Vec<sys::OneUiListItemUtf8> = items
            .iter()
            .map(|item| sys::OneUiListItemUtf8 {
                title: sys::OneUiUtf8String::from_str(&item.title),
                detail: sys::OneUiUtf8String::from_str(&item.detail),
            })
            .collect();
        unsafe {
            sys::oneui_list_set_items_utf8(
                self.widget.as_raw(),
                native_items.as_ptr(),
                native_items.len(),
            )
        };
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_list_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_list_selected_index(self.widget.as_raw()) }
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Window {
    pub fn new(options: &WindowOptions) -> Result<Self, Error> {
        let actual = unsafe { sys::oneui_utf8_abi_version() };
        if actual != sys::UTF8_ABI_VERSION {
            return Err(Error::AbiVersionMismatch {
                expected: sys::UTF8_ABI_VERSION,
                actual,
            });
        }

        let native_options = sys::OneUiWindowOptionsUtf8 {
            title: sys::OneUiUtf8String::from_str(&options.title),
            width: options.width,
            height: options.height,
            visible: i32::from(options.visible),
            borderless: i32::from(options.borderless),
            fullscreen: i32::from(options.fullscreen),
            topmost: i32::from(options.topmost),
            resizable: i32::from(options.resizable),
        };
        let raw = unsafe { sys::oneui_window_create_utf8(&native_options) };
        let raw = NonNull::new(raw).ok_or(Error::WindowCreationFailed)?;
        unsafe { sys::oneui_window_initialize(raw.as_ptr()) };
        Ok(Self {
            state: Arc::new(WindowState {
                raw: Mutex::new(Some(raw)),
            }),
            _ui_thread: PhantomData,
        })
    }

    pub fn set_title(&self, title: &str) {
        let title = sys::OneUiUtf8String::from_str(title);
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_title_utf8(raw, title);
        });
    }

    pub fn set_content(&self, content: &Widget) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_content(raw, content.as_raw());
        });
    }

    pub fn show(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_show(raw);
        });
    }

    pub fn activate(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_activate(raw);
        });
    }

    pub fn run(&self) -> i32 {
        self.state
            .current_raw()
            .map(|raw| unsafe { sys::oneui_window_run(raw.as_ptr()) })
            .unwrap_or(-1)
    }

    pub fn close(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_close(raw);
        });
    }

    pub fn dispatcher(&self) -> UiDispatcher {
        UiDispatcher {
            state: Arc::clone(&self.state),
        }
    }

    /// Returns the only thread-safe update path for a terminal mounted in this
    /// window. The terminal itself remains UI-thread bound.
    pub fn terminal_view_handle(&self, terminal: &TerminalView) -> TerminalViewHandle {
        TerminalViewHandle {
            state: Arc::clone(&terminal.state),
            dispatcher: self.dispatcher(),
        }
    }

    pub fn dispatch<F>(&self, task: F) -> Result<(), Error>
    where
        F: FnOnce() + Send + 'static,
    {
        self.dispatcher().dispatch(task)
    }
}

impl Drop for Window {
    fn drop(&mut self) {
        self.state.destroy();
    }
}

#[cfg(test)]
mod tests {
    use super::{
        terminal_style, Button, Error, Insets, Label, List, ListItem, ScrollView, Stack,
        StackDirection, TerminalCell, TerminalColor, TerminalCursor, TerminalFrame, TerminalView,
        TextField, Window, WindowOptions,
    };
    use std::sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex, OnceLock,
    };
    use std::thread;

    fn window_test_lock() -> &'static Mutex<()> {
        static LOCK: OnceLock<Mutex<()>> = OnceLock::new();
        LOCK.get_or_init(|| Mutex::new(()))
    }

    #[test]
    fn creates_hidden_window_through_utf8_abi() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions {
            title: "iShellPro 麒麟 🚀".to_owned(),
            ..WindowOptions::default()
        })
        .expect("OneUI window should be created through the UTF-8 ABI");
        window.set_title("兴业银行股份有限公司");
    }

    #[test]
    fn mounts_rust_composed_content_into_a_hidden_window() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let content = Stack::new(StackDirection::Column).expect("stack should be created");
        content.set_padding(Insets {
            top: 24.0,
            right: 24.0,
            bottom: 24.0,
            left: 24.0,
        });
        let label = Label::new("iShell Pro").expect("label should be created");
        label.set_font_size(20.0);
        content.add(label.as_widget());
        window.set_content(content.as_widget());
    }

    #[test]
    fn mounts_safe_inventory_controls_with_structured_utf8_list_items() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let content = Stack::new(StackDirection::Column).expect("stack should be created");
        let search = TextField::new("搜索主机、标签或地址").expect("text field should be created");
        let refresh = Button::new("刷新").expect("button should be created");
        let list = List::new().expect("list should be created");
        list.set_items(&[
            ListItem {
                title: "生产 SSH\t主机".to_string(),
                detail: "10.0.0.1\n兴业银行股份有限公司".to_string(),
            },
            ListItem {
                title: "Kylin V10".to_string(),
                detail: "堡垒机直连".to_string(),
            },
        ]);
        list.set_selected_index(1);
        assert_eq!(list.selected_index(), 1);

        let scroll = ScrollView::new().expect("scroll view should be created");
        scroll.set_wheel_step(40.0);
        scroll.set_content(list.as_widget());

        content.add(search.as_widget());
        content.add(refresh.as_widget());
        content.add(scroll.as_widget());
        window.set_content(content.as_widget());
    }

    #[test]
    fn mounts_terminal_grid_with_wide_cells_and_cursor() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let terminal = TerminalView::new().expect("terminal should be created");
        terminal.set_font_size(13.0);
        terminal.set_palette(
            TerminalColor::rgb(20, 24, 36),
            TerminalColor::rgb(220, 226, 240),
            TerminalColor::rgb(170, 190, 255),
        );
        terminal.set_grid(
            2,
            3,
            &[
                TerminalCell {
                    text: "A".to_owned(),
                    ..TerminalCell::default()
                },
                TerminalCell {
                    text: "宽".to_owned(),
                    style: terminal_style::WIDE,
                    ..TerminalCell::default()
                },
                TerminalCell {
                    style: terminal_style::WIDE_CONTINUATION,
                    ..TerminalCell::default()
                },
                TerminalCell::default(),
                TerminalCell::default(),
                TerminalCell::default(),
            ],
        );
        terminal.set_cursor(TerminalCursor {
            row: 0,
            column: 2,
            visible: true,
        });
        window.set_content(terminal.as_widget());
    }

    #[test]
    fn terminal_handle_submits_a_worker_frame_on_the_window_thread() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let terminal = TerminalView::new().expect("terminal should be created");
        window.set_content(terminal.as_widget());
        let handle = window.terminal_view_handle(&terminal);
        let worker = thread::spawn(move || {
            handle
                .submit_frame(TerminalFrame {
                    rows: 1,
                    columns: 1,
                    cells: vec![TerminalCell {
                        text: "X".to_owned(),
                        ..TerminalCell::default()
                    }],
                    cursor: TerminalCursor {
                        row: 0,
                        column: 0,
                        visible: true,
                    },
                })
                .expect("worker should submit a terminal frame");
        });
        worker.join().expect("worker should finish");

        let close_dispatcher = window.dispatcher();
        window
            .dispatch(move || close_dispatcher.request_close())
            .expect("window should accept close request");
        assert_eq!(window.run(), 0);
    }

    #[test]
    fn terminal_handle_rejects_updates_after_the_view_is_destroyed() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let handle = {
            let terminal = TerminalView::new().expect("terminal should be created");
            window.terminal_view_handle(&terminal)
        };

        assert!(matches!(
            handle.submit_frame(TerminalFrame {
                rows: 1,
                columns: 1,
                cells: vec![TerminalCell::default()],
                cursor: TerminalCursor {
                    row: 0,
                    column: 0,
                    visible: true,
                },
            }),
            Err(Error::WidgetDestroyed)
        ));
    }

    #[test]
    fn dispatcher_runs_work_on_the_window_thread() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let dispatcher = window.dispatcher();
        let callback_thread = Arc::new(Mutex::new(None));
        let callback_thread_from_worker = Arc::clone(&callback_thread);
        let ui_thread = thread::current().id();
        let worker = thread::spawn(move || {
            let close_dispatcher = dispatcher.clone();
            dispatcher
                .dispatch(move || {
                    *callback_thread_from_worker.lock().expect("callback lock") =
                        Some(thread::current().id());
                    close_dispatcher.request_close();
                })
                .expect("window should accept dispatched work");
        });

        assert_eq!(window.run(), 0);
        worker.join().expect("worker should finish");
        assert_eq!(
            *callback_thread.lock().expect("callback lock"),
            Some(ui_thread)
        );
    }

    #[test]
    fn dispatcher_cancels_queued_work_when_window_closes() {
        let _guard = window_test_lock().lock().expect("window test lock");
        struct DropFlag(Arc<AtomicBool>);

        impl Drop for DropFlag {
            fn drop(&mut self) {
                self.0.store(true, Ordering::Release);
            }
        }

        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let dispatcher = window.dispatcher();
        let dropped = Arc::new(AtomicBool::new(false));
        let flag = DropFlag(Arc::clone(&dropped));
        dispatcher
            .dispatch(move || drop(flag))
            .expect("window should accept queued work");
        window.close();

        assert!(dropped.load(Ordering::Acquire));
        assert!(matches!(
            dispatcher.dispatch(|| {}),
            Err(Error::WindowClosed)
        ));
    }
}
