//! Safe foundation bindings for OneUI.
//!
//! This crate owns the safe Rust boundary for OneUI's UTF-8 C ABI. It begins
//! with window lifetime and grows reusable controls without exposing raw FFI to
//! product applications.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::{CStr, CString};
use std::marker::PhantomData;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::path::{Path, PathBuf};
use std::ptr::NonNull;
use std::rc::Rc;
use std::sync::{
    atomic::{AtomicBool, AtomicPtr, Ordering},
    Arc, Mutex, OnceLock,
};

pub use oneui_sys as sys;

pub mod controls;
pub mod handles;
pub mod layout;
mod types;

pub use types::{IconSymbol, ListItem, SelectionMode, VirtualListItem, VirtualListRichMetrics};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    AbiVersionMismatch { expected: u32, actual: u32 },
    WindowCreationFailed,
    WidgetCreationFailed,
    WidgetDestroyed,
    WindowClosed,
    WrongThread,
    UiThreadBlockingOperation,
    FileDialogFailed,
    LayoutSnapshotFailed,
    InvalidVideoFrame { reason: &'static str },
    InvalidRemoteCursor { reason: &'static str },
}

/// Describes a panic caught at the Rust-to-OneUI callback boundary.
///
/// Panics must never unwind through the C ABI. OneUI catches them at the
/// boundary and reports this context to the application instead.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CallbackPanic {
    pub context: &'static str,
    pub message: String,
}

type CallbackPanicHandler = Arc<dyn Fn(CallbackPanic) + Send + Sync + 'static>;

/// Identifies a product interaction callback at the point where it was bound.
///
/// OneUI does not assign product-specific identifiers to controls. Capturing
/// the Rust call site gives end-to-end window tests a stable way to prove which
/// callback was exercised without coupling those tests to widget addresses or
/// implementation details.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct InteractionTrace {
    pub control: &'static str,
    pub interaction: &'static str,
    pub source_file: &'static str,
    pub source_line: u32,
    pub source_column: u32,
}

impl InteractionTrace {
    pub fn at(
        control: &'static str,
        interaction: &'static str,
        location: &'static std::panic::Location<'static>,
    ) -> Self {
        Self {
            control,
            interaction,
            source_file: location.file(),
            source_line: location.line(),
            source_column: location.column(),
        }
    }
}

type InteractionTraceHandler = Arc<dyn Fn(InteractionTrace) + Send + Sync + 'static>;

fn interaction_trace_handler() -> &'static Mutex<Option<InteractionTraceHandler>> {
    static HANDLER: OnceLock<Mutex<Option<InteractionTraceHandler>>> = OnceLock::new();
    HANDLER.get_or_init(|| Mutex::new(None))
}

/// Installs a process-wide observer for native control callbacks.
///
/// This hook is intended for test and diagnostic instrumentation. It is inert
/// until installed, and replacing or clearing it does not alter control
/// behavior.
pub fn set_interaction_trace_handler<F>(handler: F)
where
    F: Fn(InteractionTrace) + Send + Sync + 'static,
{
    *interaction_trace_handler()
        .lock()
        .unwrap_or_else(|poisoned| poisoned.into_inner()) = Some(Arc::new(handler));
}

/// Removes the process-wide interaction observer.
pub fn clear_interaction_trace_handler() {
    *interaction_trace_handler()
        .lock()
        .unwrap_or_else(|poisoned| poisoned.into_inner()) = None;
}

/// Emits an application-defined interaction through the process-wide observer.
///
/// Native applications can use this for interaction boundaries that live
/// outside OneUI controls, such as single-instance or operating-system launch
/// callbacks. The observer remains optional and panics are isolated from the
/// application callback path, matching built-in control tracing behavior.
pub fn emit_interaction_trace(trace: InteractionTrace) {
    let handler = interaction_trace_handler()
        .lock()
        .unwrap_or_else(|poisoned| poisoned.into_inner())
        .clone();
    if let Some(handler) = handler {
        let _ = catch_unwind(AssertUnwindSafe(|| handler(trace)));
    }
}

fn traced_callback<F>(trace: InteractionTrace, mut callback: F) -> impl FnMut() + 'static
where
    F: FnMut() + 'static,
{
    move || {
        emit_interaction_trace(trace);
        callback();
    }
}

fn traced_value_callback<T, F>(trace: InteractionTrace, mut callback: F) -> impl FnMut(T) + 'static
where
    F: FnMut(T) + 'static,
{
    move |value| {
        emit_interaction_trace(trace);
        callback(value);
    }
}

fn traced_values_callback<A, B, F>(
    trace: InteractionTrace,
    mut callback: F,
) -> impl FnMut(A, B) + 'static
where
    F: FnMut(A, B) + 'static,
{
    move |first, second| {
        emit_interaction_trace(trace);
        callback(first, second);
    }
}

fn traced_value_result_callback<T, R, F>(
    trace: InteractionTrace,
    mut callback: F,
) -> impl FnMut(T) -> R + 'static
where
    F: FnMut(T) -> R + 'static,
{
    move |value| {
        emit_interaction_trace(trace);
        callback(value)
    }
}

fn callback_panic_handler() -> &'static Mutex<Option<CallbackPanicHandler>> {
    static HANDLER: OnceLock<Mutex<Option<CallbackPanicHandler>>> = OnceLock::new();
    HANDLER.get_or_init(|| Mutex::new(None))
}

/// Installs the process-wide observer for panics caught by OneUI callbacks.
///
/// Applications should install this during startup, before creating windows.
/// Setting a new handler replaces the previous one.
pub fn set_callback_panic_handler<F>(handler: F)
where
    F: Fn(CallbackPanic) + Send + Sync + 'static,
{
    *callback_panic_handler()
        .lock()
        .unwrap_or_else(|poisoned| poisoned.into_inner()) = Some(Arc::new(handler));
}

fn panic_message(payload: &(dyn std::any::Any + Send)) -> String {
    if let Some(message) = payload.downcast_ref::<&str>() {
        (*message).to_owned()
    } else if let Some(message) = payload.downcast_ref::<String>() {
        message.clone()
    } else {
        "non-string panic payload".to_owned()
    }
}

fn run_callback_guarded<R>(context: &'static str, callback: impl FnOnce() -> R) -> Option<R> {
    match catch_unwind(AssertUnwindSafe(callback)) {
        Ok(value) => Some(value),
        Err(payload) => {
            let report = CallbackPanic {
                context,
                message: panic_message(payload.as_ref()),
            };
            let handler = callback_panic_handler()
                .lock()
                .unwrap_or_else(|poisoned| poisoned.into_inner())
                .clone();
            if let Some(handler) = handler {
                // A diagnostic observer is also application code. Keep a
                // failing observer from ever unwinding across the C ABI.
                let _ = catch_unwind(AssertUnwindSafe(|| handler(report)));
            }
            None
        }
    }
}

/// Writes UTF-8 text to the operating system clipboard.
pub fn set_clipboard_text(text: &str) -> bool {
    let text = sys::OneUiUtf8String::from_str(text);
    unsafe { sys::oneui_clipboard_set_text_utf8(text) != 0 }
}

/// Reads UTF-8 text from the operating system clipboard.
pub fn clipboard_text() -> Option<String> {
    let required = unsafe { sys::oneui_clipboard_get_text_utf8(std::ptr::null_mut(), 0) };
    if required == 0 {
        return None;
    }

    let mut buffer = vec![0u8; required];
    let written =
        unsafe { sys::oneui_clipboard_get_text_utf8(buffer.as_mut_ptr().cast(), buffer.len()) };
    if written == 0 {
        return None;
    }
    if buffer.last() == Some(&0) {
        buffer.pop();
    }
    String::from_utf8(buffer).ok()
}

/// Errors returned while building a native OneUI style sheet from CSS.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum StyleSheetError {
    CreationFailed,
    InteriorNul,
    ParseFailed(String),
    ReadFailed(String),
}

/// A reusable CSS theme for OneUI windows and widgets.
///
/// The style sheet is owned by Rust while OneUI retains a shared native copy
/// after it is installed on a window. Applications should assign semantic
/// classes to widgets instead of reproducing visual tokens in every page.
pub struct StyleSheet {
    raw: NonNull<sys::OneUiStyleSheet>,
}

impl StyleSheet {
    pub fn new() -> Result<Self, StyleSheetError> {
        let raw = NonNull::new(unsafe { sys::oneui_style_sheet_create() })
            .ok_or(StyleSheetError::CreationFailed)?;
        Ok(Self { raw })
    }

    pub fn from_css(css: &str) -> Result<Self, StyleSheetError> {
        let mut style_sheet = Self::new()?;
        style_sheet.add_css(css)?;
        Ok(style_sheet)
    }

    pub fn add_css(&mut self, css: &str) -> Result<(), StyleSheetError> {
        let css = CString::new(css).map_err(|_| StyleSheetError::InteriorNul)?;
        let mut error = [0i8; 1024];
        let applied = unsafe {
            sys::oneui_style_sheet_add_css(
                self.raw.as_ptr(),
                css.as_ptr(),
                error.as_mut_ptr(),
                error.len() as i32,
            )
        };
        if applied != 0 {
            return Ok(());
        }

        let message = unsafe { CStr::from_ptr(error.as_ptr()) }
            .to_string_lossy()
            .into_owned();
        Err(StyleSheetError::ParseFailed(message))
    }

    pub fn load_file(&mut self, path: impl AsRef<Path>) -> Result<(), StyleSheetError> {
        let css = std::fs::read_to_string(path.as_ref())
            .map_err(|error| StyleSheetError::ReadFailed(error.to_string()))?;
        self.add_css(&css)
    }

    pub fn set_custom_property(&mut self, name: &str, value: &str) -> Result<(), StyleSheetError> {
        let name = CString::new(name).map_err(|_| StyleSheetError::InteriorNul)?;
        let value = CString::new(value).map_err(|_| StyleSheetError::InteriorNul)?;
        unsafe {
            sys::oneui_style_sheet_set_custom_property(
                self.raw.as_ptr(),
                name.as_ptr(),
                value.as_ptr(),
            )
        };
        Ok(())
    }

    fn as_raw(&self) -> *mut sys::OneUiStyleSheet {
        self.raw.as_ptr()
    }
}

impl Drop for StyleSheet {
    fn drop(&mut self) {
        unsafe { sys::oneui_style_sheet_destroy(self.raw.as_ptr()) };
    }
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

/// Round-trippable native window state for application persistence.
///
/// Bounds describe the restored outer frame in platform screen coordinates.
/// They are deliberately not OneUI layout units; persist them unchanged and
/// let OneUI clamp them to a visible work area during restoration.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct WindowPlacement {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
    pub maximized: bool,
}

pub struct Window {
    state: Arc<WindowState>,
    raw_key_callback: Option<Box<WindowRawKeyCallback>>,
    client_size_changed_callback: Option<Box<WindowClientSizeChangedCallback>>,
    _ui_thread: PhantomData<Rc<()>>,
}

struct WindowRawKeyCallback {
    handler: Box<dyn FnMut(RawKeyEvent) -> bool + 'static>,
}

struct WindowClientSizeChangedCallback {
    handler: Box<dyn FnMut(f32, f32) + 'static>,
}

struct WindowState {
    raw: Mutex<Option<NonNull<sys::OneUiWindow>>>,
    ui_thread: std::thread::ThreadId,
}

// WindowState synchronizes every raw-handle access and native destruction is
// constrained to the recorded UI thread. The dispatcher only posts work.
unsafe impl Send for WindowState {}
unsafe impl Sync for WindowState {}

#[derive(Clone)]
pub struct UiDispatcher {
    state: Arc<WindowState>,
}

/// Presentation options for [`UiDispatcher::prompt_blocking`].
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct PromptOptions<'a> {
    pub initial_value: &'a str,
    pub placeholder: &'a str,
    pub password: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FileDialogMode {
    OpenFile,
    SaveFile,
    SelectFolder,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct FileDialogFilter<'a> {
    pub name: &'a str,
    pub pattern: &'a str,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct FileDialogOptions<'a> {
    pub mode: FileDialogMode,
    pub title: &'a str,
    pub initial_directory: &'a Path,
    pub default_name: &'a str,
    pub default_extension: &'a str,
    pub filters: &'a [FileDialogFilter<'a>],
    pub confirm_overwrite: bool,
}

impl<'a> FileDialogOptions<'a> {
    pub fn open(title: &'a str) -> Self {
        Self {
            mode: FileDialogMode::OpenFile,
            title,
            initial_directory: Path::new(""),
            default_name: "",
            default_extension: "",
            filters: &[],
            confirm_overwrite: true,
        }
    }

    pub fn save(title: &'a str) -> Self {
        Self {
            mode: FileDialogMode::SaveFile,
            ..Self::open(title)
        }
    }

    pub fn select_folder(title: &'a str) -> Self {
        Self {
            mode: FileDialogMode::SelectFolder,
            ..Self::open(title)
        }
    }
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

fn trace_ui_task(message: &str) {
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
    fn current_raw(&self) -> Option<NonNull<sys::OneUiWindow>> {
        *self.raw.lock().expect("OneUI window state lock poisoned")
    }

    #[track_caller]
    fn with_raw<R>(&self, action: impl FnOnce(*mut sys::OneUiWindow) -> R) -> Option<R> {
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
    /// Serializes the mounted widget tree after layout for visual QA and
    /// diagnostics. The snapshot contains geometry and resolved style data,
    /// but deliberately never includes text-field contents.
    pub fn layout_snapshot_json(&self) -> Result<String, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }

        let mut required = 0_usize;
        let query = self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_layout_snapshot_utf8(raw, std::ptr::null_mut(), 0, &mut required)
            })
            .ok_or(Error::WindowClosed)?;
        if query != -2 || required <= 1 {
            return Err(Error::LayoutSnapshotFailed);
        }

        let mut bytes = vec![0_u8; required];
        let result = self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_layout_snapshot_utf8(
                    raw,
                    bytes.as_mut_ptr().cast(),
                    bytes.len(),
                    &mut required,
                )
            })
            .ok_or(Error::WindowClosed)?;
        if result != 1 || required == 0 || required > bytes.len() {
            return Err(Error::LayoutSnapshotFailed);
        }
        bytes.truncate(required - 1);
        String::from_utf8(bytes).map_err(|_| Error::LayoutSnapshotFailed)
    }

    /// Re-resolves and reapplies the window style sheet after runtime CSS
    /// custom-property changes.
    pub fn refresh_style_sheet(&self) -> Result<(), Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_refresh_style_sheet(raw);
        });
        Ok(())
    }

    /// Changes the window-wide default UI font on the owning thread.
    pub fn set_default_font_family(&self, family: &str) -> Result<(), Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let family = sys::OneUiUtf8String::from_str(family.trim());
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_default_font_family_utf8(raw, family);
        });
        Ok(())
    }

    /// Requests focus for a widget already mounted in this window.
    ///
    /// Focus changes are synchronous and must originate on the window thread.
    pub fn request_focus(&self, widget: &Widget, focus_visible: bool) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        Ok(self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_request_focus(raw, widget.as_raw(), i32::from(focus_visible)) != 0
            })
            .unwrap_or(false))
    }

    /// Shows a platform-native file dialog owned by this window.
    ///
    /// Native dialogs run a nested platform message loop and therefore must be
    /// opened directly from the owning window thread. Product code should call
    /// this from a command callback; background services must first dispatch a
    /// short UI task and continue their I/O after the selected path is returned.
    pub fn file_dialog(&self, options: FileDialogOptions<'_>) -> Result<Option<PathBuf>, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        if options.filters.len() > 64 {
            return Err(Error::FileDialogFailed);
        }

        let initial_directory = options.initial_directory.to_string_lossy();
        let filters = options
            .filters
            .iter()
            .map(|filter| sys::OneUiFileDialogFilterUtf8 {
                name: sys::OneUiUtf8String::from_str(filter.name),
                pattern: sys::OneUiUtf8String::from_str(filter.pattern),
            })
            .collect::<Vec<_>>();
        let native = sys::OneUiFileDialogOptionsUtf8 {
            mode: match options.mode {
                FileDialogMode::OpenFile => sys::FILE_DIALOG_OPEN_FILE,
                FileDialogMode::SaveFile => sys::FILE_DIALOG_SAVE_FILE,
                FileDialogMode::SelectFolder => sys::FILE_DIALOG_SELECT_FOLDER,
            },
            title: sys::OneUiUtf8String::from_str(options.title),
            initial_directory: sys::OneUiUtf8String::from_str(&initial_directory),
            default_name: sys::OneUiUtf8String::from_str(options.default_name),
            default_extension: sys::OneUiUtf8String::from_str(options.default_extension),
            filters: filters.as_ptr(),
            filter_count: filters.len(),
            confirm_overwrite: i32::from(options.confirm_overwrite),
        };

        // A Windows file-system path is capped at 32,767 UTF-16 code units.
        // Four UTF-8 bytes per code unit plus NUL covers the ABI result without
        // reopening a modal dialog just to resize the caller's buffer.
        const MAX_FILE_DIALOG_UTF8_BYTES: usize = 32_767 * 4 + 1;
        let mut output = vec![0_u8; MAX_FILE_DIALOG_UTF8_BYTES];
        let mut required = 0_usize;
        let result = self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_file_dialog_utf8(
                    raw,
                    &native,
                    output.as_mut_ptr().cast(),
                    output.len(),
                    &mut required,
                )
            })
            .ok_or(Error::WindowClosed)?;
        match result {
            0 => Ok(None),
            1 if required > 0 && required <= output.len() => {
                output.truncate(required.saturating_sub(1));
                String::from_utf8(output)
                    .map(PathBuf::from)
                    .map(Some)
                    .map_err(|_| Error::FileDialogFailed)
            }
            _ => Err(Error::FileDialogFailed),
        }
    }

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

    /// Displays a platform-native confirmation prompt on the window thread
    /// and waits for the user's decision on the calling worker thread.
    ///
    /// This method is intended for background operations that must pause at a
    /// security or destructive-action boundary. It must not be called from the
    /// window thread because the caller waits until the dispatched UI work has
    /// completed. Closing the window while the request is queued returns
    /// [`Error::WindowClosed`].
    pub fn confirm_blocking(&self, title: &str, message: &str) -> Result<bool, Error> {
        if std::thread::current().id() == self.state.ui_thread {
            return Err(Error::UiThreadBlockingOperation);
        }
        let title = wide_null_terminated(title);
        let message = wide_null_terminated(message);
        let dispatcher = self.clone();
        let (sender, receiver) = std::sync::mpsc::sync_channel(1);
        self.dispatch(move || {
            let accepted = dispatcher.confirm_on_window_thread(&title, &message);
            let _ = sender.send(accepted);
        })?;
        receiver.recv().map_err(|_| Error::WindowClosed)
    }

    /// Displays a platform-native confirmation prompt owned by this window.
    ///
    /// This synchronous form is intended for command callbacks already running
    /// on the window thread. The platform dialog owns its nested message loop;
    /// long-running work following confirmation must still run on a worker.
    pub fn confirm(&self, title: &str, message: &str) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        Ok(self
            .confirm_on_window_thread(&wide_null_terminated(title), &wide_null_terminated(message)))
    }

    fn confirm_on_window_thread(&self, title: &[u16], message: &[u16]) -> bool {
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_confirm(raw, title.as_ptr(), message.as_ptr()) != 0
            })
            .unwrap_or(false)
    }

    /// Displays a platform-native text or password prompt and waits on the
    /// calling worker thread. The entered value is never retained by OneUI.
    ///
    /// Like [`Self::confirm_blocking`], this method must not be called from the
    /// window thread. Closing the prompt returns `Ok(None)`; closing the owner
    /// before queued work runs returns [`Error::WindowClosed`].
    pub fn prompt_blocking(
        &self,
        title: &str,
        message: &str,
        options: PromptOptions<'_>,
    ) -> Result<Option<String>, Error> {
        if std::thread::current().id() == self.state.ui_thread {
            return Err(Error::UiThreadBlockingOperation);
        }
        let title = wide_null_terminated(title);
        let message = wide_null_terminated(message);
        let initial_value = SecretWide::new(options.initial_value);
        let placeholder = wide_null_terminated(options.placeholder);
        let password = options.password;
        let dispatcher = self.clone();
        let (sender, receiver) = std::sync::mpsc::sync_channel(1);
        self.dispatch(move || {
            let result = dispatcher.prompt_on_window_thread(
                &title,
                &message,
                initial_value.as_slice(),
                &placeholder,
                password,
            );
            let _ = sender.send(result);
        })?;
        receiver.recv().map_err(|_| Error::WindowClosed)
    }

    /// Displays a platform-native text or password prompt owned by this window.
    ///
    /// Call this form from a window-thread command callback. The returned value
    /// is not retained by OneUI and the temporary UTF-16 buffer is cleared.
    pub fn prompt(
        &self,
        title: &str,
        message: &str,
        options: PromptOptions<'_>,
    ) -> Result<Option<String>, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let initial_value = SecretWide::new(options.initial_value);
        Ok(self.prompt_on_window_thread(
            &wide_null_terminated(title),
            &wide_null_terminated(message),
            initial_value.as_slice(),
            &wide_null_terminated(options.placeholder),
            options.password,
        ))
    }

    fn prompt_on_window_thread(
        &self,
        title: &[u16],
        message: &[u16],
        initial_value: &[u16],
        placeholder: &[u16],
        password: bool,
    ) -> Option<String> {
        const MAX_PROMPT_CODE_UNITS: usize = 4096;
        let mut output = vec![0_u16; MAX_PROMPT_CODE_UNITS];
        let accepted = self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_prompt_text(
                    raw,
                    title.as_ptr(),
                    message.as_ptr(),
                    initial_value.as_ptr(),
                    placeholder.as_ptr(),
                    i32::from(password),
                    output.as_mut_ptr(),
                    output.len() as i32,
                ) != 0
            })
            .unwrap_or(false);
        let value = accepted.then(|| {
            let length = output
                .iter()
                .position(|code_unit| *code_unit == 0)
                .unwrap_or(output.len());
            String::from_utf16_lossy(&output[..length])
        });
        output.fill(0);
        value
    }

    pub fn request_close(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_request_close(raw);
        });
    }

    /// Creates a thread-safe adaptive-layout producer for a mounted widget.
    pub fn widget_handle(&self, widget: &Widget) -> WidgetHandle {
        WidgetHandle {
            state: Arc::clone(&widget.state),
            dispatcher: self.clone(),
        }
    }

    /// Creates a thread-safe text producer for a label mounted in this window.
    pub fn label_handle(&self, label: &Label) -> LabelHandle {
        LabelHandle {
            state: Arc::clone(&label.state),
            dispatcher: self.clone(),
        }
    }

    /// Creates a thread-safe text producer for a single-line field mounted in this window.
    pub fn text_field_handle(&self, text_field: &TextField) -> TextFieldHandle {
        TextFieldHandle {
            state: Arc::clone(&text_field.state),
            dispatcher: self.clone(),
        }
    }

    /// Creates a thread-safe text producer for a multiline editor mounted in this window.
    pub fn text_area_handle(&self, text_area: &TextArea) -> TextAreaHandle {
        TextAreaHandle {
            state: Arc::clone(&text_area.state),
            dispatcher: self.clone(),
        }
    }

    /// Creates a thread-safe value producer for a progress bar mounted in this window.
    pub fn progress_bar_handle(&self, progress_bar: &ProgressBar) -> ProgressBarHandle {
        ProgressBarHandle {
            state: Arc::clone(&progress_bar.state),
            dispatcher: self.clone(),
        }
    }

    /// Creates a thread-safe sample producer for a sparkline mounted in this window.
    pub fn sparkline_handle(&self, sparkline: &Sparkline) -> SparklineHandle {
        SparklineHandle {
            state: Arc::clone(&sparkline.state),
            dispatcher: self.clone(),
        }
    }

    /// Creates a thread-safe multi-series producer for an operational chart.
    pub fn time_series_chart_handle(&self, chart: &TimeSeriesChart) -> TimeSeriesChartHandle {
        TimeSeriesChartHandle {
            state: Arc::clone(&chart.state),
            dispatcher: self.clone(),
        }
    }

    /// Creates a thread-safe producer for in-place virtual-list row updates.
    pub fn virtual_list_handle(&self, list: &VirtualList) -> VirtualListHandle {
        VirtualListHandle {
            state: Arc::clone(&list.state),
            dispatcher: self.clone(),
        }
    }

    /// Creates a thread-safe producer for table data revisions and row updates.
    pub fn table_handle(&self, table: &Table) -> TableHandle {
        TableHandle {
            state: Arc::clone(&table.state),
            dispatcher: self.clone(),
        }
    }

    /// Creates a thread-safe frame producer for a terminal in this window.
    pub fn terminal_view_handle(&self, terminal: &TerminalView) -> TerminalViewHandle {
        TerminalViewHandle {
            state: Arc::clone(&terminal.state),
            dispatcher: self.clone(),
        }
    }

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

    pub fn request_activate(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_activate(raw);
        });
    }

    pub fn minimize(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_minimize(raw);
        });
    }

    pub fn toggle_maximize(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_toggle_maximize(raw);
        });
    }

    /// Changes immersive presentation at runtime. Worker-thread callers are
    /// marshalled to the owning UI thread.
    pub fn set_fullscreen(&self, fullscreen: bool) -> Result<(), Error> {
        if std::thread::current().id() == self.state.ui_thread {
            return self
                .state
                .with_raw(|raw| unsafe {
                    sys::oneui_window_set_fullscreen(raw, i32::from(fullscreen));
                })
                .ok_or(Error::WindowClosed);
        }
        let dispatcher = self.clone();
        self.dispatch(move || {
            let _ = dispatcher.set_fullscreen(fullscreen);
        })
    }

    /// Reads immersive presentation state on the owning UI thread.
    pub fn is_fullscreen(&self) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_is_fullscreen(raw) != 0 })
            .ok_or(Error::WindowClosed)
    }

    pub fn set_title_bar_interactive_insets(&self, leading_width: f32, trailing_width: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_title_bar_interactive_insets(raw, leading_width, trailing_width);
        });
    }

    pub fn clear_title_bar_interactive_insets(&self) {
        self.set_title_bar_interactive_insets(-1.0, -1.0);
    }
}

#[derive(Clone, Copy, Default)]
struct WidgetLayoutUpdate {
    visible: Option<bool>,
    preferred_size: Option<(f32, f32)>,
}

struct WidgetState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_layout: Mutex<WidgetLayoutUpdate>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for visibility and preferred-size changes on a mounted widget.
///
/// Product shells use this for data-dependent adaptive layouts (for example switching
/// between a detailed low-cardinality view and a virtualized high-cardinality view).
/// Changes are coalesced and always applied on the window's owning UI thread.
#[derive(Clone)]
pub struct WidgetHandle {
    state: Arc<WidgetState>,
    dispatcher: UiDispatcher,
}

impl WidgetHandle {
    pub fn set_visible(&self, visible: bool) -> Result<(), Error> {
        self.update_layout(Some(visible), None)
    }

    pub fn set_preferred_size(&self, width: f32, height: f32) -> Result<(), Error> {
        self.update_layout(None, Some((width, height)))
    }

    pub fn set_visible_and_preferred_size(
        &self,
        visible: bool,
        width: f32,
        height: f32,
    ) -> Result<(), Error> {
        self.update_layout(Some(visible), Some((width, height)))
    }

    fn update_layout(
        &self,
        visible: Option<bool>,
        preferred_size: Option<(f32, f32)>,
    ) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        {
            let mut pending = self
                .state
                .pending_layout
                .lock()
                .expect("widget pending layout lock poisoned");
            if let Some(visible) = visible {
                pending.visible = Some(visible);
            }
            if let Some(preferred_size) = preferred_size {
                pending.preferred_size = Some(preferred_size);
            }
        }
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }

        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_layout(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            *state
                .pending_layout
                .lock()
                .expect("widget pending layout lock poisoned") = WidgetLayoutUpdate::default();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_layout(state: &WidgetState) {
        loop {
            let update = {
                let mut pending = state
                    .pending_layout
                    .lock()
                    .expect("widget pending layout lock poisoned");
                std::mem::take(&mut *pending)
            };
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(visible) = update.visible {
                unsafe { sys::oneui_widget_set_visible(raw, i32::from(visible)) };
            }
            if let Some((width, height)) = update.preferred_size {
                unsafe { sys::oneui_widget_set_preferred_size(raw, width, height) };
            }

            state.update_scheduled.store(false, Ordering::Release);
            let has_more = {
                let pending = state
                    .pending_layout
                    .lock()
                    .expect("widget pending layout lock poisoned");
                pending.visible.is_some() || pending.preferred_size.is_some()
            };
            if !has_more {
                return;
            }
            if !state.update_scheduled.swap(true, Ordering::AcqRel) {
                continue;
            }
            return;
        }
    }
}

pub struct Widget {
    raw: NonNull<sys::OneUiWidget>,
    state: Arc<WidgetState>,
}

impl Widget {
    fn from_raw(raw: *mut sys::OneUiWidget) -> Result<Self, Error> {
        let raw = NonNull::new(raw).ok_or(Error::WidgetCreationFailed)?;
        Ok(Self {
            state: Arc::new(WidgetState {
                raw: AtomicPtr::new(raw.as_ptr()),
                pending_layout: Mutex::new(WidgetLayoutUpdate::default()),
                update_scheduled: AtomicBool::new(false),
            }),
            raw,
        })
    }

    fn as_raw(&self) -> *mut sys::OneUiWidget {
        self.raw.as_ptr()
    }

    /// Sets the layout hint used by parent containers. A zero dimension stays
    /// flexible on that axis, matching OneUI's native layout contract.
    pub fn set_preferred_size(&self, width: f32, height: f32) {
        unsafe { sys::oneui_widget_set_preferred_size(self.as_raw(), width, height) };
    }

    /// Returns the widget's committed logical layout rectangle.
    ///
    /// Use this for geometry-dependent interactions such as anchoring an
    /// overlay to an already mounted trigger. Product layout should continue
    /// to be expressed through containers and CSS rather than manual frames.
    pub fn frame(&self) -> Rect {
        let frame = unsafe { sys::oneui_widget_frame(self.as_raw()) };
        Rect {
            x: frame.x,
            y: frame.y,
            width: frame.width,
            height: frame.height,
        }
    }

    pub fn set_disabled(&self, disabled: bool) {
        unsafe { sys::oneui_widget_set_disabled(self.as_raw(), i32::from(disabled)) };
    }

    /// Controls whether this widget participates in keyboard Tab traversal.
    /// Interactive surfaces such as modeless command palettes may keep pointer
    /// handling enabled while opting out of the focus sequence.
    pub fn set_tab_stop(&self, tab_stop: bool) {
        unsafe { sys::oneui_widget_set_tab_stop(self.as_raw(), i32::from(tab_stop)) };
    }

    pub fn set_visible(&self, visible: bool) {
        unsafe { sys::oneui_widget_set_visible(self.as_raw(), i32::from(visible)) };
    }

    pub fn is_focused(&self) -> bool {
        unsafe { sys::oneui_widget_focused(self.as_raw()) != 0 }
    }

    /// Sets concise hover help for icon-only and unfamiliar controls.
    pub fn set_tooltip(&self, tooltip: &str) {
        let tooltip = wide_null_terminated(tooltip);
        unsafe { sys::oneui_widget_set_tooltip(self.as_raw(), tooltip.as_ptr()) };
    }

    /// Assigns semantic CSS classes. The currently installed window style
    /// sheet is reapplied immediately by the native runtime.
    pub fn set_classes(&self, classes: &str) -> Result<(), StyleSheetError> {
        let classes = CString::new(classes).map_err(|_| StyleSheetError::InteriorNul)?;
        unsafe { sys::oneui_widget_set_classes(self.as_raw(), classes.as_ptr()) };
        Ok(())
    }

    /// Overrides the native element name while preserving a semantic class
    /// list. Use this for layout regions such as `aside`, `main`, and `header`.
    pub fn set_style_node(&self, tag: &str, classes: &str) -> Result<(), StyleSheetError> {
        let tag = CString::new(tag).map_err(|_| StyleSheetError::InteriorNul)?;
        let classes = CString::new(classes).map_err(|_| StyleSheetError::InteriorNul)?;
        unsafe { sys::oneui_widget_set_style_node(self.as_raw(), tag.as_ptr(), classes.as_ptr()) };
        Ok(())
    }

    pub fn apply_style_sheet(&self, style_sheet: &StyleSheet) {
        unsafe { sys::oneui_widget_apply_style_sheet(self.as_raw(), style_sheet.as_raw()) };
    }
}

impl Drop for Widget {
    fn drop(&mut self) {
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        *self
            .state
            .pending_layout
            .lock()
            .expect("widget pending layout lock poisoned") = WidgetLayoutUpdate::default();
        self.state.update_scheduled.store(false, Ordering::Release);
        unsafe { sys::oneui_widget_destroy(self.raw.as_ptr()) };
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StackDirection {
    Column,
    Row,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SplitOrientation {
    Horizontal = 0,
    Vertical = 1,
}

#[derive(Debug, Clone, Copy, Default)]
pub struct Insets {
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub left: f32,
}

/// A rectangle in logical window coordinates.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl Color {
    pub const fn rgb(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b, a: 255 }
    }

    pub const fn rgba(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { r, g, b, a }
    }
}

impl From<Color> for sys::OneUiColor {
    fn from(value: Color) -> Self {
        Self {
            r: value.r,
            g: value.g,
            b: value.b,
            a: value.a,
        }
    }
}

/// The focus treatment used by interactive controls. It is exposed as a
/// first-class value so product shells can remain keyboard accessible while
/// using a custom visual theme.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct FocusRingStyle {
    pub color: Color,
    pub width: f32,
    pub offset: f32,
    pub radius: f32,
    pub visible: bool,
}

impl FocusRingStyle {
    pub const fn hidden() -> Self {
        Self {
            color: Color::rgba(0, 0, 0, 0),
            width: 0.0,
            offset: 0.0,
            radius: 0.0,
            visible: false,
        }
    }
}

impl From<FocusRingStyle> for sys::OneUiFocusRingStyle {
    fn from(value: FocusRingStyle) -> Self {
        Self {
            color: value.color.into(),
            width: value.width,
            offset: value.offset,
            radius: value.radius,
            visible: i32::from(value.visible),
        }
    }
}

/// Colors and geometry for one state in a native button transition.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ButtonStateStyle {
    pub background: Color,
    pub foreground: Color,
    pub border: Color,
    pub border_width: f32,
    pub radius: f32,
    pub focus_ring: FocusRingStyle,
}

impl ButtonStateStyle {
    pub const fn solid(background: Color, foreground: Color, border: Color, radius: f32) -> Self {
        Self {
            background,
            foreground,
            border,
            border_width: 1.0,
            radius,
            focus_ring: FocusRingStyle::hidden(),
        }
    }
}

impl From<ButtonStateStyle> for sys::OneUiButtonStateStyle {
    fn from(value: ButtonStateStyle) -> Self {
        Self {
            background: value.background.into(),
            foreground: value.foreground.into(),
            border: value.border.into(),
            border_width: value.border_width,
            radius: value.radius,
            focus_ring: value.focus_ring.into(),
        }
    }
}

/// A full five-state visual contract for a native [`Button`]. OneUI animates
/// between these values on hover and press; applications only describe the
/// visual states and never hand-roll pointer animations.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ButtonStyle {
    pub normal: ButtonStateStyle,
    pub hovered: ButtonStateStyle,
    pub pressed: ButtonStateStyle,
    pub disabled: ButtonStateStyle,
    pub focus_visible: ButtonStateStyle,
}

impl From<ButtonStyle> for sys::OneUiButtonStyle {
    fn from(value: ButtonStyle) -> Self {
        Self {
            normal: value.normal.into(),
            hovered: value.hovered.into(),
            pressed: value.pressed.into(),
            disabled: value.disabled.into(),
            focus_visible: value.focus_visible.into(),
        }
    }
}

/// The visual state for a content-bearing interactive surface such as a card
/// or list row. It intentionally excludes text colors because the surface may
/// host any arbitrary child composition.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct InteractiveSurfaceStateStyle {
    pub background: Color,
    pub border: Color,
    pub border_width: f32,
    pub radius: f32,
}

impl InteractiveSurfaceStateStyle {
    pub const fn solid(background: Color, border: Color, radius: f32) -> Self {
        Self {
            background,
            border,
            border_width: 1.0,
            radius,
        }
    }
}

impl From<InteractiveSurfaceStateStyle> for sys::OneUiInteractiveSurfaceStateStyle {
    fn from(value: InteractiveSurfaceStateStyle) -> Self {
        Self {
            background: value.background.into(),
            border: value.border.into(),
            border_width: value.border_width,
            radius: value.radius,
        }
    }
}

/// Native hover and press states for an [`InteractiveSurface`]. OneUI owns
/// the animation clock and preserves events for controls nested inside it.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct InteractiveSurfaceStyle {
    pub normal: InteractiveSurfaceStateStyle,
    pub hovered: InteractiveSurfaceStateStyle,
    pub pressed: InteractiveSurfaceStateStyle,
    pub disabled: InteractiveSurfaceStateStyle,
    pub focus_visible: InteractiveSurfaceStateStyle,
}

impl From<InteractiveSurfaceStyle> for sys::OneUiInteractiveSurfaceStyle {
    fn from(value: InteractiveSurfaceStyle) -> Self {
        Self {
            normal: value.normal.into(),
            hovered: value.hovered.into(),
            pressed: value.pressed.into(),
            disabled: value.disabled.into(),
            focus_visible: value.focus_visible.into(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ButtonContentAlign {
    Start = 0,
    Center = 1,
    End = 2,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LabelAlign {
    Start = 0,
    Center = 1,
    End = 2,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StackAlign {
    Start = 0,
    Center = 1,
    End = 2,
    Stretch = 3,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ButtonVariant {
    Primary = 0,
    Secondary = 1,
}

struct VoidCallback {
    handler: Rc<RefCell<Box<dyn FnMut() + 'static>>>,
}

fn run_void_handler(context: &'static str, handler: &Rc<RefCell<Box<dyn FnMut() + 'static>>>) {
    // Native modal UI (for example the Windows prompt dialog) runs a nested
    // message loop. That loop can deliver the same command again before the
    // outer invocation returns. A command callback is edge-triggered, so the
    // nested duplicate must be ignored instead of attempting a second mutable
    // borrow and panicking across the FFI boundary.
    let Ok(mut handler) = handler.try_borrow_mut() else {
        return;
    };
    run_callback_guarded(context, &mut **handler);
}

unsafe extern "C" fn run_void_callback(user_data: *mut std::ffi::c_void) {
    if user_data.is_null() {
        return;
    }
    // Clone the handler before entering product code. Product callbacks may
    // legitimately destroy the widget and its Rust wrapper, which frees the
    // callback token pointed to by `user_data`. The local Rc keeps the actual
    // callable alive until this FFI frame has returned.
    let handler = unsafe { (&*user_data.cast::<VoidCallback>()).handler.clone() };
    run_void_handler("widget.command", &handler);
}

fn wide_null_terminated(value: &str) -> Vec<u16> {
    value.encode_utf16().chain(std::iter::once(0)).collect()
}

struct SecretWide(Vec<u16>);

impl SecretWide {
    fn new(value: &str) -> Self {
        Self(wide_null_terminated(value))
    }

    fn as_slice(&self) -> &[u16] {
        &self.0
    }
}

impl Drop for SecretWide {
    fn drop(&mut self) {
        self.0.fill(0);
    }
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

    pub fn set_align(&self, align: StackAlign) {
        unsafe { sys::oneui_stack_set_align(self.widget.as_raw(), align as i32) };
    }

    pub fn content_width(&self) -> f32 {
        unsafe { sys::oneui_stack_content_width(self.widget.as_raw()) }
    }

    pub fn content_height(&self) -> f32 {
        unsafe { sys::oneui_stack_content_height(self.widget.as_raw()) }
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

struct FloatChangedCallback {
    handler: Box<dyn FnMut(f32) + 'static>,
}

unsafe extern "C" fn run_float_changed_callback(value: f32, user_data: *mut std::ffi::c_void) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<FloatChangedCallback>() };
    run_callback_guarded("value.float_changed", || (callback.handler)(value));
}

/// Native resizable two-pane layout.
///
/// OneUI owns divider hit testing and drag capture. Product code supplies pane
/// content, minimum extents, and optionally persists ratio changes.
pub struct SplitView {
    widget: Widget,
    ratio_changed_callback: Option<Box<FloatChangedCallback>>,
    ratio_committed_callback: Option<Box<FloatChangedCallback>>,
}

impl SplitView {
    pub fn new(orientation: SplitOrientation) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_split_view_create(orientation as i32) })?;
        Ok(Self {
            widget,
            ratio_changed_callback: None,
            ratio_committed_callback: None,
        })
    }

    pub fn set_first(&self, child: &Widget) {
        unsafe { sys::oneui_split_view_set_first(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_second(&self, child: &Widget) {
        unsafe { sys::oneui_split_view_set_second(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_orientation(&self, orientation: SplitOrientation) {
        unsafe { sys::oneui_split_view_set_orientation(self.widget.as_raw(), orientation as i32) };
    }

    pub fn set_ratio(&self, ratio: f32) {
        unsafe { sys::oneui_split_view_set_ratio(self.widget.as_raw(), ratio) };
    }

    pub fn ratio(&self) -> f32 {
        unsafe { sys::oneui_split_view_ratio(self.widget.as_raw()) }
    }

    pub fn set_gap(&self, gap: f32) {
        unsafe { sys::oneui_split_view_set_gap(self.widget.as_raw(), gap) };
    }

    pub fn set_padding(&self, padding: Insets) {
        unsafe { sys::oneui_split_view_set_padding(self.widget.as_raw(), padding.into()) };
    }

    pub fn set_resizable(&self, resizable: bool) {
        unsafe { sys::oneui_split_view_set_resizable(self.widget.as_raw(), i32::from(resizable)) };
    }

    pub fn set_minimum_pane_extent(&self, first: f32, second: f32) {
        unsafe {
            sys::oneui_split_view_set_minimum_pane_extent(self.widget.as_raw(), first, second)
        };
    }

    #[track_caller]
    pub fn set_on_ratio_changed<F>(&mut self, callback: F)
    where
        F: FnMut(f32) + 'static,
    {
        let trace =
            InteractionTrace::at("SplitView", "ratio_changed", std::panic::Location::caller());
        self.clear_on_ratio_changed();
        self.ratio_changed_callback = Some(Box::new(FloatChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .ratio_changed_callback
            .as_deref_mut()
            .expect("split ratio callback was just installed")
            as *mut FloatChangedCallback)
            .cast();
        unsafe {
            sys::oneui_split_view_set_on_ratio_changed(
                self.widget.as_raw(),
                Some(run_float_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_ratio_changed(&mut self) {
        unsafe {
            sys::oneui_split_view_set_on_ratio_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.ratio_changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_ratio_committed<F>(&mut self, callback: F)
    where
        F: FnMut(f32) + 'static,
    {
        let trace = InteractionTrace::at(
            "SplitView",
            "ratio_committed",
            std::panic::Location::caller(),
        );
        self.clear_on_ratio_committed();
        self.ratio_committed_callback = Some(Box::new(FloatChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .ratio_committed_callback
            .as_deref_mut()
            .expect("split ratio committed callback was just installed")
            as *mut FloatChangedCallback)
            .cast();
        unsafe {
            sys::oneui_split_view_set_on_ratio_committed(
                self.widget.as_raw(),
                Some(run_float_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_ratio_committed(&mut self) {
        unsafe {
            sys::oneui_split_view_set_on_ratio_committed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.ratio_committed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for SplitView {
    fn drop(&mut self) {
        self.clear_on_ratio_changed();
        self.clear_on_ratio_committed();
    }
}

/// Alignment for overlays positioned inside an [`OverlayHost`].
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OverlayAlignment {
    Start = 0,
    Center = 1,
    End = 2,
}

/// A native overlay composition root.
///
/// Use an overlay host whenever a product needs transient UI such as dialogs,
/// menus, or teaching callouts. Modal overlays trap focus and prevent pointer
/// input from leaking into the page below.
pub struct OverlayHost {
    widget: Widget,
}

impl OverlayHost {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_overlay_host_create() })?;
        Ok(Self { widget })
    }

    pub fn set_content(&self, child: &Widget) {
        unsafe { sys::oneui_overlay_host_set_content(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn add_overlay(&self, child: &Widget, layer: i32) {
        unsafe { sys::oneui_overlay_host_add_overlay(self.widget.as_raw(), child.as_raw(), layer) };
    }

    #[allow(clippy::too_many_arguments)]
    pub fn add_anchored_overlay(
        &self,
        child: &Widget,
        layer: i32,
        width: f32,
        height: f32,
        margin: Insets,
        horizontal_alignment: OverlayAlignment,
        vertical_alignment: OverlayAlignment,
    ) {
        unsafe {
            sys::oneui_overlay_host_add_anchored_overlay(
                self.widget.as_raw(),
                child.as_raw(),
                layer,
                width,
                height,
                margin.into(),
                horizontal_alignment as i32,
                vertical_alignment as i32,
            )
        };
    }

    #[allow(clippy::too_many_arguments)]
    pub fn add_modal_anchored_overlay(
        &self,
        child: &Widget,
        layer: i32,
        width: f32,
        height: f32,
        margin: Insets,
        horizontal_alignment: OverlayAlignment,
        vertical_alignment: OverlayAlignment,
    ) {
        unsafe {
            sys::oneui_overlay_host_add_modal_anchored_overlay(
                self.widget.as_raw(),
                child.as_raw(),
                layer,
                width,
                height,
                margin.into(),
                horizontal_alignment as i32,
                vertical_alignment as i32,
            )
        };
    }

    pub fn remove_overlay(&self, child: &Widget) -> bool {
        unsafe { sys::oneui_overlay_host_remove_overlay(self.widget.as_raw(), child.as_raw()) != 0 }
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PopupPreferredPlacement {
    BottomStart = 0,
    BottomEnd = 1,
    TopStart = 2,
    TopEnd = 3,
    LeftStart = 4,
    RightStart = 5,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PopupInteractionMode {
    Modeless = 0,
    LightDismiss = 1,
    Modal = 2,
}

/// A native popup surface with viewport-aware placement and light-dismiss behavior.
pub struct Popup {
    widget: Widget,
}

impl Popup {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_popup_create() })?;
        Ok(Self { widget })
    }

    pub fn set_anchor(&self, anchor: &Widget) {
        unsafe { sys::oneui_popup_set_anchor(self.widget.as_raw(), anchor.as_raw()) };
    }

    pub fn set_content(&self, content: &Widget) {
        unsafe { sys::oneui_popup_set_content(self.widget.as_raw(), content.as_raw()) };
    }

    pub fn set_open(&self, open: bool) {
        unsafe { sys::oneui_popup_set_open(self.widget.as_raw(), i32::from(open)) };
    }

    pub fn is_open(&self) -> bool {
        unsafe { sys::oneui_popup_is_open(self.widget.as_raw()) != 0 }
    }

    pub fn set_anchor_rect(&self, x: f32, y: f32, width: f32, height: f32) {
        unsafe { sys::oneui_popup_set_anchor_rect(self.widget.as_raw(), x, y, width, height) };
    }

    pub fn clear_anchor_rect(&self) {
        unsafe { sys::oneui_popup_clear_anchor_rect(self.widget.as_raw()) };
    }

    pub fn set_preferred_placement(&self, placement: PopupPreferredPlacement) {
        unsafe { sys::oneui_popup_set_preferred_placement(self.widget.as_raw(), placement as i32) };
    }

    pub fn set_interaction_mode(&self, mode: PopupInteractionMode) {
        unsafe { sys::oneui_popup_set_interaction_mode(self.widget.as_raw(), mode as i32) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

struct MenuActivatedCallback {
    handler: Box<dyn FnMut(i32) + 'static>,
}

unsafe extern "C" fn run_menu_activated_callback(index: i32, user_data: *mut std::ffi::c_void) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<MenuActivatedCallback>() };
    run_callback_guarded("menu.activated", || (callback.handler)(index));
}

/// A standard native command menu suitable for popups and context menus.
pub struct Menu {
    widget: Widget,
    activated_callback: Option<Box<MenuActivatedCallback>>,
}

impl Menu {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_menu_create() })?;
        Ok(Self {
            widget,
            activated_callback: None,
        })
    }

    pub fn add_header(&self, title: &str, subtitle: &str) {
        let title = wide_null_terminated(title);
        let subtitle = wide_null_terminated(subtitle);
        unsafe {
            sys::oneui_menu_add_header(self.widget.as_raw(), title.as_ptr(), subtitle.as_ptr())
        };
    }

    pub fn add_item(&self, text: &str, icon: Option<IconSymbol>, danger: bool) -> i32 {
        let text = wide_null_terminated(text);
        unsafe {
            sys::oneui_menu_add_item(
                self.widget.as_raw(),
                text.as_ptr(),
                icon.map_or(-1, |symbol| symbol as i32),
                i32::from(danger),
            )
        }
    }

    pub fn add_separator(&self) {
        unsafe { sys::oneui_menu_add_separator(self.widget.as_raw()) };
    }

    pub fn clear_items(&self) {
        unsafe { sys::oneui_menu_clear_items(self.widget.as_raw()) };
    }

    pub fn set_item_disabled(&self, index: i32, disabled: bool) {
        unsafe {
            sys::oneui_menu_set_item_disabled(self.widget.as_raw(), index, i32::from(disabled))
        };
    }

    pub fn preferred_height(&self) -> f32 {
        unsafe { sys::oneui_menu_preferred_height(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_activated<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Menu", "activated", std::panic::Location::caller());
        self.clear_on_activated();
        self.activated_callback = Some(Box::new(MenuActivatedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .activated_callback
            .as_deref_mut()
            .expect("menu callback was just installed")
            as *mut MenuActivatedCallback)
            .cast();
        unsafe {
            sys::oneui_menu_set_on_activated(
                self.widget.as_raw(),
                Some(run_menu_activated_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_activated(&mut self) {
        unsafe {
            sys::oneui_menu_set_on_activated(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.activated_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Menu {
    fn drop(&mut self) {
        self.clear_on_activated();
    }
}

/// Standard native dialog chrome for content supplied by the application.
///
/// Place the dialog in an [`OverlayHost`] with a modal overlay to receive
/// focus trapping and outside-pointer blocking.
pub struct Dialog {
    widget: Widget,
    close_callback: Option<Box<VoidCallback>>,
}

impl Dialog {
    pub fn new(title: &str, subtitle: &str) -> Result<Self, Error> {
        let title = wide_null_terminated(title);
        let subtitle = wide_null_terminated(subtitle);
        let widget = Widget::from_raw(unsafe {
            sys::oneui_dialog_create(title.as_ptr(), subtitle.as_ptr())
        })?;
        Ok(Self {
            widget,
            close_callback: None,
        })
    }

    pub fn set_title(&self, title: &str) {
        let title = wide_null_terminated(title);
        unsafe { sys::oneui_dialog_set_title(self.widget.as_raw(), title.as_ptr()) };
    }

    pub fn set_subtitle(&self, subtitle: &str) {
        let subtitle = wide_null_terminated(subtitle);
        unsafe { sys::oneui_dialog_set_subtitle(self.widget.as_raw(), subtitle.as_ptr()) };
    }

    pub fn set_icon(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_dialog_set_icon(self.widget.as_raw(), symbol as i32) };
    }

    pub fn set_close_visible(&self, visible: bool) {
        unsafe { sys::oneui_dialog_set_close_visible(self.widget.as_raw(), i32::from(visible)) };
    }

    pub fn set_content(&self, child: &Widget) {
        unsafe { sys::oneui_dialog_set_content(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_actions(&self, child: &Widget) {
        unsafe { sys::oneui_dialog_set_actions(self.widget.as_raw(), child.as_raw()) };
    }

    #[track_caller]
    pub fn set_on_close<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace = InteractionTrace::at("Dialog", "close", std::panic::Location::caller());
        self.clear_on_close();
        self.close_callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .close_callback
            .as_deref_mut()
            .expect("dialog close callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_dialog_set_on_close(self.widget.as_raw(), Some(run_void_callback), user_data)
        };
    }

    pub fn clear_on_close(&mut self) {
        unsafe { sys::oneui_dialog_set_on_close(self.widget.as_raw(), None, std::ptr::null_mut()) };
        self.close_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Dialog {
    fn drop(&mut self) {
        self.clear_on_close();
    }
}

/// A centered, semantic status surface for empty, loading, no-result, and error states.
pub struct StateView {
    widget: Widget,
    action_callback: Option<Box<VoidCallback>>,
}

impl StateView {
    pub fn new(title: &str, message: &str) -> Result<Self, Error> {
        let title = wide_null_terminated(title);
        let message = wide_null_terminated(message);
        let widget = Widget::from_raw(unsafe {
            sys::oneui_state_view_create(title.as_ptr(), message.as_ptr())
        })?;
        Ok(Self {
            widget,
            action_callback: None,
        })
    }

    pub fn set_title(&self, title: &str) {
        let title = wide_null_terminated(title);
        unsafe { sys::oneui_state_view_set_title(self.widget.as_raw(), title.as_ptr()) };
    }

    pub fn set_message(&self, message: &str) {
        let message = wide_null_terminated(message);
        unsafe { sys::oneui_state_view_set_message(self.widget.as_raw(), message.as_ptr()) };
    }

    pub fn set_icon(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_state_view_set_icon(self.widget.as_raw(), symbol as i32) };
    }

    pub fn set_action(&self, text: &str) {
        let text = wide_null_terminated(text);
        unsafe { sys::oneui_state_view_set_action(self.widget.as_raw(), text.as_ptr()) };
    }

    #[track_caller]
    pub fn set_on_action<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace = InteractionTrace::at("StateView", "action", std::panic::Location::caller());
        self.clear_on_action();
        self.action_callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .action_callback
            .as_deref_mut()
            .expect("state view action callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_state_view_set_on_action(
                self.widget.as_raw(),
                Some(run_void_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_action(&mut self) {
        unsafe {
            sys::oneui_state_view_set_on_action(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.action_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for StateView {
    fn drop(&mut self) {
        self.clear_on_action();
    }
}

/// A simple native surface used to compose application regions and cards.
pub struct Panel {
    widget: Widget,
}

impl Panel {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_panel_create() })?;
        Ok(Self { widget })
    }

    pub fn set_content(&self, child: &Widget) {
        unsafe { sys::oneui_panel_set_content(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_background(&self, color: Color) {
        unsafe {
            sys::oneui_panel_set_background(
                self.widget.as_raw(),
                color.r,
                color.g,
                color.b,
                color.a,
            )
        };
    }

    pub fn set_border(&self, color: Color, width: f32) {
        unsafe {
            sys::oneui_panel_set_border(
                self.widget.as_raw(),
                color.r,
                color.g,
                color.b,
                color.a,
                width,
            )
        };
    }

    pub fn set_radius(&self, radius: f32) {
        unsafe { sys::oneui_panel_set_radius(self.widget.as_raw(), radius) };
    }

    pub fn set_padding(&self, padding: Insets) {
        unsafe { sys::oneui_panel_set_padding(self.widget.as_raw(), padding.into()) };
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

    pub fn set_chrome_visible(&self, visible: bool) {
        unsafe {
            sys::oneui_scroll_view_set_chrome_visible(self.widget.as_raw(), i32::from(visible))
        };
    }

    pub fn set_scrollbar_style(&self, color: Color, thickness: f32) {
        unsafe {
            sys::oneui_scroll_view_set_scrollbar_style(
                self.widget.as_raw(),
                color.r,
                color.g,
                color.b,
                color.a,
                thickness,
            )
        };
    }

    pub fn scroll_to_bottom(&self) {
        unsafe { sys::oneui_scroll_view_scroll_to_bottom(self.widget.as_raw()) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

struct LabelState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_text: Mutex<Option<String>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for label text owned by a window.
///
/// Worker threads submit only the latest value. OneUI coalesces bursts and
/// applies text on the owning UI thread, matching the terminal frame handle's
/// lifetime and thread-affinity guarantees.
#[derive(Clone)]
pub struct LabelHandle {
    state: Arc<LabelState>,
    dispatcher: UiDispatcher,
}

impl LabelHandle {
    pub fn set_text(&self, text: impl Into<String>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }

        *self
            .state
            .pending_text
            .lock()
            .expect("label pending text lock poisoned") = Some(text.into());
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
                .expect("label pending text lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_text(state: &LabelState) {
        loop {
            let text = state
                .pending_text
                .lock()
                .expect("label pending text lock poisoned")
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(text) = text {
                let text = sys::OneUiUtf8String::from_str(&text);
                trace_ui_task("label update started");
                unsafe { sys::oneui_label_set_text_utf8(raw, text) };
                trace_ui_task("label update completed");
            }

            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_text
                .lock()
                .expect("label pending text lock poisoned")
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

pub struct Label {
    widget: Widget,
    state: Arc<LabelState>,
}

impl Label {
    pub fn new(text: &str) -> Result<Self, Error> {
        let text = sys::OneUiUtf8String::from_str(text);
        let widget = Widget::from_raw(unsafe { sys::oneui_label_create_utf8(text) })?;
        Ok(Self {
            state: Arc::new(LabelState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_text: Mutex::new(None),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
        })
    }

    pub fn set_text(&self, text: &str) {
        let text = sys::OneUiUtf8String::from_str(text);
        unsafe { sys::oneui_label_set_text_utf8(self.widget.as_raw(), text) };
    }

    pub fn set_font_size(&self, font_size: f32) {
        unsafe { sys::oneui_label_set_font_size(self.widget.as_raw(), font_size) };
    }

    pub fn set_color(&self, color: Color) {
        unsafe {
            sys::oneui_label_set_color(self.widget.as_raw(), color.r, color.g, color.b, color.a)
        };
    }

    pub fn set_font_weight(&self, weight: i32) {
        unsafe { sys::oneui_label_set_font_weight(self.widget.as_raw(), weight) };
    }

    pub fn set_align(&self, align: LabelAlign) {
        unsafe { sys::oneui_label_set_align(self.widget.as_raw(), align as i32) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Label {
    fn drop(&mut self) {
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_text
            .lock()
            .expect("label pending text lock poisoned")
            .take();
        self.state.update_scheduled.store(false, Ordering::Release);
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

    pub fn value(&self) -> f64 {
        unsafe { sys::oneui_progress_bar_value(self.widget.as_raw()) }
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

/// One named series in an operational chart. `None` values are rendered as
/// real gaps rather than being joined to adjacent samples.
#[derive(Clone, Debug, PartialEq)]
pub struct TimeSeries {
    pub name: String,
    pub color: Color,
    pub values: Vec<Option<f64>>,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TimeSeriesThreshold {
    pub value: f64,
    pub color: Color,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct TimeSeriesInspection {
    pub index: Option<usize>,
    pub pinned: bool,
}

struct TimeSeriesInspectionCallback {
    handler: Box<dyn FnMut(TimeSeriesInspection) + 'static>,
}

unsafe extern "C" fn run_time_series_inspection_callback(
    index: std::ffi::c_int,
    pinned: std::ffi::c_int,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<TimeSeriesInspectionCallback>() };
    let event = TimeSeriesInspection {
        index: usize::try_from(index).ok(),
        pinned: pinned != 0,
    };
    run_callback_guarded("time_series.inspection", || (callback.handler)(event));
}

fn set_native_time_series(raw: *mut sys::OneUiWidget, series: &[TimeSeries]) {
    let values = series
        .iter()
        .map(|item| {
            item.values
                .iter()
                .map(|value| value.unwrap_or(f64::NAN))
                .collect::<Vec<_>>()
        })
        .collect::<Vec<_>>();
    let native = series
        .iter()
        .zip(values.iter())
        .map(|(item, values)| sys::OneUiTimeSeriesUtf8 {
            name: sys::OneUiUtf8String::from_str(&item.name),
            color: item.color.into(),
            values: values.as_ptr(),
            value_count: values.len(),
        })
        .collect::<Vec<_>>();
    unsafe { sys::oneui_time_series_chart_set_series(raw, native.as_ptr(), native.len()) };
}

struct TimeSeriesChartState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_series: Mutex<Option<Vec<TimeSeries>>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for a mounted operational chart. Bursts collapse to
/// the newest complete data revision before being applied on the UI thread.
#[derive(Clone)]
pub struct TimeSeriesChartHandle {
    state: Arc<TimeSeriesChartState>,
    dispatcher: UiDispatcher,
}

impl TimeSeriesChartHandle {
    pub fn set_series(&self, series: Vec<TimeSeries>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        *self
            .state
            .pending_series
            .lock()
            .expect("time-series pending data lock poisoned") = Some(series);
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }
        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_series(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_series
                .lock()
                .expect("time-series pending data lock poisoned")
                .take();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_series(state: &TimeSeriesChartState) {
        loop {
            let series = state
                .pending_series
                .lock()
                .expect("time-series pending data lock poisoned")
                .take();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            if let Some(series) = series {
                set_native_time_series(raw, &series);
            }
            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_series
                .lock()
                .expect("time-series pending data lock poisoned")
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

/// Native multi-series chart with gaps, thresholds and pointer/keyboard
/// inspection for operational dashboards.
pub struct TimeSeriesChart {
    widget: Widget,
    state: Arc<TimeSeriesChartState>,
    inspection_callback: Option<Box<TimeSeriesInspectionCallback>>,
}

impl TimeSeriesChart {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_time_series_chart_create() })?;
        Ok(Self {
            state: Arc::new(TimeSeriesChartState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_series: Mutex::new(None),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
            inspection_callback: None,
        })
    }

    pub fn set_series(&self, series: &[TimeSeries]) {
        self.state
            .pending_series
            .lock()
            .expect("time-series pending data lock poisoned")
            .take();
        set_native_time_series(self.widget.as_raw(), series);
    }

    pub fn set_range(&self, minimum: f64, maximum: f64) {
        unsafe { sys::oneui_time_series_chart_set_range(self.widget.as_raw(), minimum, maximum) };
    }

    pub fn set_grid_lines(&self, count: i32) {
        unsafe { sys::oneui_time_series_chart_set_grid_lines(self.widget.as_raw(), count) };
    }

    pub fn set_visual_style(
        &self,
        smooth_curves: bool,
        area_fill: bool,
        dashed_grid: bool,
        axes_visible: bool,
        line_width: f32,
        fill_alpha: u8,
    ) {
        unsafe {
            sys::oneui_time_series_chart_set_visual_style(
                self.widget.as_raw(),
                i32::from(smooth_curves),
                i32::from(area_fill),
                i32::from(dashed_grid),
                i32::from(axes_visible),
                line_width,
                fill_alpha,
            )
        };
    }

    pub fn set_plot_insets(&self, insets: Insets) {
        unsafe {
            sys::oneui_time_series_chart_set_plot_insets(
                self.widget.as_raw(),
                sys::OneUiInsets {
                    top: insets.top,
                    right: insets.right,
                    bottom: insets.bottom,
                    left: insets.left,
                },
            )
        };
    }

    pub fn set_thresholds(&self, thresholds: &[TimeSeriesThreshold]) {
        let native = thresholds
            .iter()
            .map(|threshold| sys::OneUiTimeSeriesThreshold {
                value: threshold.value,
                color: threshold.color.into(),
            })
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_time_series_chart_set_thresholds(
                self.widget.as_raw(),
                native.as_ptr(),
                native.len(),
            )
        };
    }

    pub fn set_inspection(&self, inspection: TimeSeriesInspection) {
        let index = inspection
            .index
            .and_then(|index| i32::try_from(index).ok())
            .unwrap_or(-1);
        unsafe {
            sys::oneui_time_series_chart_set_inspection(
                self.widget.as_raw(),
                index,
                i32::from(inspection.pinned),
            )
        };
    }

    pub fn inspection(&self) -> TimeSeriesInspection {
        let index = unsafe { sys::oneui_time_series_chart_inspection_index(self.widget.as_raw()) };
        TimeSeriesInspection {
            index: usize::try_from(index).ok(),
            pinned: unsafe {
                sys::oneui_time_series_chart_inspection_pinned(self.widget.as_raw()) != 0
            },
        }
    }

    pub fn set_on_inspection_changed<F>(&mut self, callback: F)
    where
        F: FnMut(TimeSeriesInspection) + 'static,
    {
        self.clear_on_inspection_changed();
        self.inspection_callback = Some(Box::new(TimeSeriesInspectionCallback {
            handler: Box::new(callback),
        }));
        let user_data = (self
            .inspection_callback
            .as_deref_mut()
            .expect("time-series callback was just installed")
            as *mut TimeSeriesInspectionCallback)
            .cast();
        unsafe {
            sys::oneui_time_series_chart_set_on_inspection_changed(
                self.widget.as_raw(),
                Some(run_time_series_inspection_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_inspection_changed(&mut self) {
        unsafe {
            sys::oneui_time_series_chart_set_on_inspection_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.inspection_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for TimeSeriesChart {
    fn drop(&mut self) {
        self.clear_on_inspection_changed();
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_series
            .lock()
            .expect("time-series pending data lock poisoned")
            .take();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}

/// A standard command button with callback lifetime bound to the Rust wrapper.
pub struct Button {
    widget: Widget,
    callback: Option<Box<VoidCallback>>,
}

impl Button {
    pub fn new(text: &str) -> Result<Self, Error> {
        let text = sys::OneUiUtf8String::from_str(text);
        let widget = Widget::from_raw(unsafe { sys::oneui_button_create_utf8(text) })?;
        Ok(Self {
            widget,
            callback: None,
        })
    }

    pub fn set_text(&self, text: &str) {
        let text = sys::OneUiUtf8String::from_str(text);
        unsafe { sys::oneui_button_set_text_utf8(self.widget.as_raw(), text) };
    }

    pub fn set_icon(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_button_set_icon(self.widget.as_raw(), symbol as i32) };
    }

    pub fn set_content_align(&self, align: ButtonContentAlign) {
        unsafe { sys::oneui_button_set_content_align(self.widget.as_raw(), align as i32) };
    }

    pub fn set_trailing_text(&self, text: &str) {
        let text = sys::OneUiUtf8String::from_str(text);
        unsafe { sys::oneui_button_set_trailing_text_utf8(self.widget.as_raw(), text) };
    }

    pub fn set_variant(&self, variant: ButtonVariant) {
        unsafe { sys::oneui_button_set_variant(self.widget.as_raw(), variant as i32) };
    }

    /// Applies all visual states while preserving OneUI's native hover and
    /// press transitions.
    pub fn set_style(&self, style: ButtonStyle) {
        let style: sys::OneUiButtonStyle = style.into();
        unsafe { sys::oneui_button_set_style(self.widget.as_raw(), &style) };
    }

    pub fn clear_style(&self) {
        unsafe { sys::oneui_button_clear_style(self.widget.as_raw()) };
    }

    #[track_caller]
    pub fn set_on_click<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace = InteractionTrace::at("Button", "click", std::panic::Location::caller());
        self.clear_on_click();
        self.callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .callback
            .as_deref_mut()
            .expect("button callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_button_set_on_click(self.widget.as_raw(), Some(run_void_callback), user_data)
        };
    }

    pub fn clear_on_click(&mut self) {
        unsafe { sys::oneui_button_set_on_click(self.widget.as_raw(), None, std::ptr::null_mut()) };
        self.callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Button {
    fn drop(&mut self) {
        self.clear_on_click();
    }
}

/// A compact native option selector with keyboard navigation and light dismiss.
pub struct Select {
    widget: Widget,
    changed_callback: Option<Box<ListChangedCallback>>,
}

impl Select {
    pub fn new(items: &[String]) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_select_create() })?;
        let select = Self {
            widget,
            changed_callback: None,
        };
        select.set_items(items);
        Ok(select)
    }

    pub fn set_items(&self, items: &[String]) {
        let native_items: Vec<sys::OneUiUtf8String> = items
            .iter()
            .map(|item| sys::OneUiUtf8String::from_str(item))
            .collect();
        unsafe {
            sys::oneui_select_set_items_utf8(
                self.widget.as_raw(),
                native_items.as_ptr(),
                native_items.len(),
            )
        };
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_select_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_select_selected_index(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Select", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("select callback was just installed")
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_select_set_on_changed(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_select_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.changed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Select {
    fn drop(&mut self) {
        self.clear_on_changed();
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PointerButton {
    None,
    Left,
    Right,
    Middle,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct PointerEvent {
    pub x: f32,
    pub y: f32,
    pub button: PointerButton,
    pub click_count: i32,
    pub shift: bool,
    pub control: bool,
    pub alt: bool,
}

impl From<sys::OneUiPointerEvent> for PointerEvent {
    fn from(value: sys::OneUiPointerEvent) -> Self {
        let button = match value.button {
            1 => PointerButton::Left,
            2 => PointerButton::Right,
            3 => PointerButton::Middle,
            _ => PointerButton::None,
        };
        Self {
            x: value.x,
            y: value.y,
            button,
            click_count: value.click_count,
            shift: value.shift != 0,
            control: value.control != 0,
            alt: value.alt != 0,
        }
    }
}

struct PointerCallback {
    handler: Box<dyn FnMut(PointerEvent) + 'static>,
}

unsafe extern "C" fn run_pointer_callback(
    event: *const sys::OneUiPointerEvent,
    user_data: *mut std::ffi::c_void,
) {
    if event.is_null() || user_data.is_null() {
        return;
    }
    let event = PointerEvent::from(unsafe { *event });
    let callback = unsafe { &mut *user_data.cast::<PointerCallback>() };
    run_callback_guarded("pointer.activated", || (callback.handler)(event));
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct GridReorderRequest {
    pub source_id: String,
    pub target_index: usize,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ItemDragPhase {
    Started,
    Updated,
    Dropped,
    Cancelled,
}

impl ItemDragPhase {
    fn from_raw(value: std::ffi::c_int) -> Self {
        match value {
            0 => Self::Started,
            1 => Self::Updated,
            2 => Self::Dropped,
            _ => Self::Cancelled,
        }
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct ItemDragEvent {
    pub source_id: String,
    pub phase: ItemDragPhase,
    pub x: f32,
    pub y: f32,
}

struct GridReorderRequestedCallback {
    handler: Box<dyn FnMut(GridReorderRequest) + 'static>,
}

struct ItemDragCallback {
    handler: Box<dyn FnMut(ItemDragEvent) + 'static>,
}

unsafe extern "C" fn run_grid_reorder_requested_callback(
    source_id: *const std::ffi::c_char,
    source_length: usize,
    target_index: std::ffi::c_int,
    user_data: *mut std::ffi::c_void,
) {
    if source_id.is_null() || user_data.is_null() || target_index < 0 {
        return;
    }
    let source = unsafe { std::slice::from_raw_parts(source_id.cast::<u8>(), source_length) };
    let request = GridReorderRequest {
        source_id: String::from_utf8_lossy(source).into_owned(),
        target_index: target_index as usize,
    };
    let callback = unsafe { &mut *user_data.cast::<GridReorderRequestedCallback>() };
    run_callback_guarded("reorderable_grid.reorder_requested", || {
        (callback.handler)(request)
    });
}

unsafe extern "C" fn run_item_drag_callback(
    source_id: *const std::ffi::c_char,
    source_length: usize,
    phase: std::ffi::c_int,
    x: f32,
    y: f32,
    user_data: *mut std::ffi::c_void,
) {
    if source_id.is_null() || user_data.is_null() {
        return;
    }
    let source = unsafe { std::slice::from_raw_parts(source_id.cast::<u8>(), source_length) };
    let event = ItemDragEvent {
        source_id: String::from_utf8_lossy(source).into_owned(),
        phase: ItemDragPhase::from_raw(phase),
        x,
        y,
    };
    let callback = unsafe { &mut *user_data.cast::<ItemDragCallback>() };
    run_callback_guarded("reorderable_grid.item_drag", || (callback.handler)(event));
}

/// A native responsive grid that lays out arbitrary widgets and emits
/// reorder requests without mutating product-owned domain data.
pub struct ReorderableGrid {
    widget: Widget,
    reorder_callback: Option<Box<GridReorderRequestedCallback>>,
    item_drag_callback: Option<Box<ItemDragCallback>>,
}

impl ReorderableGrid {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_reorderable_grid_create() })?;
        Ok(Self {
            widget,
            reorder_callback: None,
            item_drag_callback: None,
        })
    }

    pub fn clear_items(&self) {
        unsafe { sys::oneui_reorderable_grid_clear_items(self.widget.as_raw()) };
    }

    pub fn add_item(&self, id: &str, child: &Widget) {
        unsafe {
            sys::oneui_reorderable_grid_add_item_utf8(
                self.widget.as_raw(),
                sys::OneUiUtf8String::from_str(id),
                child.as_raw(),
            )
        };
    }

    pub fn move_item(&self, source_id: &str, target_index: usize) -> bool {
        unsafe {
            sys::oneui_reorderable_grid_move_item_utf8(
                self.widget.as_raw(),
                sys::OneUiUtf8String::from_str(source_id),
                target_index.min(std::ffi::c_int::MAX as usize) as std::ffi::c_int,
            ) != 0
        }
    }

    pub fn set_column_count(&self, columns: usize) {
        unsafe {
            sys::oneui_reorderable_grid_set_column_count(
                self.widget.as_raw(),
                columns.min(std::ffi::c_int::MAX as usize) as std::ffi::c_int,
            )
        };
    }

    pub fn set_gaps(&self, column_gap: f32, row_gap: f32) {
        unsafe { sys::oneui_reorderable_grid_set_gaps(self.widget.as_raw(), column_gap, row_gap) };
    }

    pub fn set_item_height(&self, height: f32) {
        unsafe { sys::oneui_reorderable_grid_set_item_height(self.widget.as_raw(), height) };
    }

    pub fn content_height(&self) -> f32 {
        unsafe { sys::oneui_reorderable_grid_content_height(self.widget.as_raw()) }
    }

    pub fn set_reorder_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_reorderable_grid_set_reorder_enabled(
                self.widget.as_raw(),
                i32::from(enabled),
            )
        };
    }

    pub fn reorder_enabled(&self) -> bool {
        unsafe { sys::oneui_reorderable_grid_reorder_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_reorder_requested<F>(&mut self, callback: F)
    where
        F: FnMut(GridReorderRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "ReorderableGrid",
            "reorder_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_reorder_requested();
        self.reorder_callback = Some(Box::new(GridReorderRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .reorder_callback
            .as_deref_mut()
            .expect("grid reorder callback was just installed")
            as *mut GridReorderRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_reorderable_grid_set_on_reorder_requested_utf8(
                self.widget.as_raw(),
                Some(run_grid_reorder_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_reorder_requested(&mut self) {
        unsafe {
            sys::oneui_reorderable_grid_set_on_reorder_requested_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.reorder_callback = None;
    }

    pub fn set_item_drag_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_reorderable_grid_set_item_drag_enabled(
                self.widget.as_raw(),
                i32::from(enabled),
            )
        };
    }

    pub fn item_drag_enabled(&self) -> bool {
        unsafe { sys::oneui_reorderable_grid_item_drag_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_item_drag<F>(&mut self, callback: F)
    where
        F: FnMut(ItemDragEvent) + 'static,
    {
        let trace = InteractionTrace::at(
            "ReorderableGrid",
            "item_drag",
            std::panic::Location::caller(),
        );
        self.clear_on_item_drag();
        self.item_drag_callback = Some(Box::new(ItemDragCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .item_drag_callback
            .as_deref_mut()
            .expect("item drag callback was just installed")
            as *mut ItemDragCallback)
            .cast();
        unsafe {
            sys::oneui_reorderable_grid_set_on_item_drag_utf8(
                self.widget.as_raw(),
                Some(run_item_drag_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_item_drag(&mut self) {
        unsafe {
            sys::oneui_reorderable_grid_set_on_item_drag_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.item_drag_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for ReorderableGrid {
    fn drop(&mut self) {
        self.clear_on_item_drag();
        self.clear_on_reorder_requested();
    }
}

/// A reusable native card/list-row surface with hover and press transitions.
pub struct InteractiveSurface {
    widget: Widget,
    click_callback: Option<Box<VoidCallback>>,
    pointer_callback: Option<Box<PointerCallback>>,
    pointer_moved_callback: Option<Box<PointerCallback>>,
    hover_changed_callback: Option<Box<BoolChangedCallback>>,
    context_menu_callback: Option<Box<PointerCallback>>,
}

impl InteractiveSurface {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_interactive_surface_create() })?;
        Ok(Self {
            widget,
            click_callback: None,
            pointer_callback: None,
            pointer_moved_callback: None,
            hover_changed_callback: None,
            context_menu_callback: None,
        })
    }

    pub fn set_content(&self, child: &Widget) {
        unsafe { sys::oneui_interactive_surface_set_content(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_padding(&self, padding: Insets) {
        unsafe { sys::oneui_interactive_surface_set_padding(self.widget.as_raw(), padding.into()) };
    }

    pub fn set_style(&self, style: InteractiveSurfaceStyle) {
        let style: sys::OneUiInteractiveSurfaceStyle = style.into();
        unsafe { sys::oneui_interactive_surface_set_style(self.widget.as_raw(), &style) };
    }

    #[track_caller]
    pub fn set_on_click<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "click",
            std::panic::Location::caller(),
        );
        self.clear_on_click();
        self.click_callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .click_callback
            .as_deref_mut()
            .expect("interactive surface click callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_click(
                self.widget.as_raw(),
                Some(run_void_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_click(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_click(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.click_callback = None;
    }

    #[track_caller]
    pub fn set_on_pointer_activated<F>(&mut self, callback: F)
    where
        F: FnMut(PointerEvent) + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "pointer_activated",
            std::panic::Location::caller(),
        );
        self.clear_on_pointer_activated();
        self.pointer_callback = Some(Box::new(PointerCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .pointer_callback
            .as_deref_mut()
            .expect("interactive surface pointer callback was just installed")
            as *mut PointerCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_pointer_activated(
                self.widget.as_raw(),
                Some(run_pointer_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_pointer_activated(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_pointer_activated(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.pointer_callback = None;
    }

    #[track_caller]
    pub fn set_on_pointer_moved<F>(&mut self, callback: F)
    where
        F: FnMut(PointerEvent) + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "pointer_moved",
            std::panic::Location::caller(),
        );
        self.clear_on_pointer_moved();
        self.pointer_moved_callback = Some(Box::new(PointerCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .pointer_moved_callback
            .as_deref_mut()
            .expect("interactive surface pointer-moved callback was just installed")
            as *mut PointerCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_pointer_moved(
                self.widget.as_raw(),
                Some(run_pointer_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_pointer_moved(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_pointer_moved(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.pointer_moved_callback = None;
    }

    #[track_caller]
    pub fn set_on_hover_changed<F>(&mut self, callback: F)
    where
        F: FnMut(bool) + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "hover_changed",
            std::panic::Location::caller(),
        );
        self.clear_on_hover_changed();
        self.hover_changed_callback = Some(Box::new(BoolChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .hover_changed_callback
            .as_deref_mut()
            .expect("interactive surface hover callback was just installed")
            as *mut BoolChangedCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_hover_changed(
                self.widget.as_raw(),
                Some(run_bool_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_hover_changed(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_hover_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.hover_changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_context_menu_requested<F>(&mut self, callback: F)
    where
        F: FnMut(PointerEvent) + 'static,
    {
        let trace = InteractionTrace::at(
            "InteractiveSurface",
            "context_menu_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_context_menu_requested();
        self.context_menu_callback = Some(Box::new(PointerCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .context_menu_callback
            .as_deref_mut()
            .expect("interactive surface context menu callback was just installed")
            as *mut PointerCallback)
            .cast();
        unsafe {
            sys::oneui_interactive_surface_set_on_context_menu_requested(
                self.widget.as_raw(),
                Some(run_pointer_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_context_menu_requested(&mut self) {
        unsafe {
            sys::oneui_interactive_surface_set_on_context_menu_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.context_menu_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for InteractiveSurface {
    fn drop(&mut self) {
        self.clear_on_click();
        self.clear_on_pointer_activated();
        self.clear_on_pointer_moved();
        self.clear_on_hover_changed();
        self.clear_on_context_menu_requested();
    }
}

pub struct Icon {
    widget: Widget,
}

impl Icon {
    pub fn new(symbol: IconSymbol) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_icon_create(symbol as i32) })?;
        Ok(Self { widget })
    }

    pub fn set_symbol(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_icon_set_symbol(self.widget.as_raw(), symbol as i32) };
    }

    pub fn set_color(&self, color: Color) {
        unsafe {
            sys::oneui_icon_set_color(self.widget.as_raw(), color.r, color.g, color.b, color.a)
        };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

pub struct IconButton {
    widget: Widget,
    callback: Option<Box<VoidCallback>>,
}

impl IconButton {
    pub fn new(symbol: IconSymbol) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_icon_button_create(symbol as i32) })?;
        Ok(Self {
            widget,
            callback: None,
        })
    }

    pub fn set_symbol(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_icon_button_set_symbol(self.widget.as_raw(), symbol as i32) };
    }

    #[track_caller]
    pub fn set_on_click<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace = InteractionTrace::at("IconButton", "click", std::panic::Location::caller());
        self.clear_on_click();
        self.callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .callback
            .as_deref_mut()
            .expect("icon button callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_icon_button_set_on_click(
                self.widget.as_raw(),
                Some(run_void_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_click(&mut self) {
        unsafe {
            sys::oneui_icon_button_set_on_click(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for IconButton {
    fn drop(&mut self) {
        self.clear_on_click();
    }
}

/// Native title bar for borderless desktop windows.
pub struct WindowTitleBar {
    widget: Widget,
    minimize_callback: Option<Box<VoidCallback>>,
    maximize_callback: Option<Box<VoidCallback>>,
    close_callback: Option<Box<VoidCallback>>,
}

impl WindowTitleBar {
    pub fn new(title: &str) -> Result<Self, Error> {
        let title = wide_null_terminated(title);
        let widget = Widget::from_raw(unsafe { sys::oneui_title_bar_create(title.as_ptr()) })?;
        Ok(Self {
            widget,
            minimize_callback: None,
            maximize_callback: None,
            close_callback: None,
        })
    }

    pub fn set_title(&self, title: &str) {
        let title = wide_null_terminated(title);
        unsafe { sys::oneui_title_bar_set_title(self.widget.as_raw(), title.as_ptr()) };
    }

    pub fn set_icon(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_title_bar_set_icon_symbol(self.widget.as_raw(), symbol as i32) };
    }

    /// Places application controls before the title and center accessory.
    pub fn set_leading(&self, leading: &Widget) {
        unsafe { sys::oneui_title_bar_set_leading(self.widget.as_raw(), leading.as_raw()) };
    }

    /// Selects a named built-in chrome variant, such as `dark`.
    pub fn set_variant(&self, variant: &str) {
        let variant = std::ffi::CString::new(variant)
            .expect("OneUI title bar variants cannot contain a NUL byte");
        unsafe { sys::oneui_title_bar_set_variant(self.widget.as_raw(), variant.as_ptr()) };
    }

    /// Places application controls in the center of the native title bar.
    pub fn set_accessory(&self, accessory: &Widget) {
        unsafe { sys::oneui_title_bar_set_accessory(self.widget.as_raw(), accessory.as_raw()) };
    }

    #[track_caller]
    pub fn set_on_minimize<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace =
            InteractionTrace::at("WindowTitleBar", "minimize", std::panic::Location::caller());
        self.clear_on_minimize();
        self.minimize_callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .minimize_callback
            .as_deref_mut()
            .expect("title bar minimize callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_title_bar_set_on_minimize(
                self.widget.as_raw(),
                Some(run_void_callback),
                user_data,
            )
        };
    }

    #[track_caller]
    pub fn set_on_maximize<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace =
            InteractionTrace::at("WindowTitleBar", "maximize", std::panic::Location::caller());
        self.clear_on_maximize();
        self.maximize_callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .maximize_callback
            .as_deref_mut()
            .expect("title bar maximize callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_title_bar_set_on_maximize(
                self.widget.as_raw(),
                Some(run_void_callback),
                user_data,
            )
        };
    }

    #[track_caller]
    pub fn set_on_close<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace = InteractionTrace::at("WindowTitleBar", "close", std::panic::Location::caller());
        self.clear_on_close();
        self.close_callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .close_callback
            .as_deref_mut()
            .expect("title bar close callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_title_bar_set_on_close(
                self.widget.as_raw(),
                Some(run_void_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_minimize(&mut self) {
        unsafe {
            sys::oneui_title_bar_set_on_minimize(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.minimize_callback = None;
    }

    pub fn clear_on_maximize(&mut self) {
        unsafe {
            sys::oneui_title_bar_set_on_maximize(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.maximize_callback = None;
    }

    pub fn clear_on_close(&mut self) {
        unsafe {
            sys::oneui_title_bar_set_on_close(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.close_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for WindowTitleBar {
    fn drop(&mut self) {
        self.clear_on_minimize();
        self.clear_on_maximize();
        self.clear_on_close();
    }
}

/// Animated desktop-sidebar navigation item.
pub struct NavItem {
    widget: Widget,
    callback: Option<Box<VoidCallback>>,
}

impl NavItem {
    pub fn new(text: &str, symbol: IconSymbol, selected: bool) -> Result<Self, Error> {
        let text = wide_null_terminated(text);
        let widget = Widget::from_raw(unsafe {
            sys::oneui_nav_item_create(text.as_ptr(), symbol as i32, i32::from(selected))
        })?;
        Ok(Self {
            widget,
            callback: None,
        })
    }

    pub fn set_selected(&self, selected: bool) {
        unsafe { sys::oneui_nav_item_set_selected(self.widget.as_raw(), i32::from(selected)) };
    }

    #[track_caller]
    pub fn set_on_click<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        let trace = InteractionTrace::at("NavItem", "click", std::panic::Location::caller());
        self.clear_on_click();
        self.callback = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(traced_callback(trace, callback)))),
        }));
        let user_data = (self
            .callback
            .as_deref_mut()
            .expect("nav item callback was just installed")
            as *mut VoidCallback)
            .cast();
        unsafe {
            sys::oneui_nav_item_set_on_click(
                self.widget.as_raw(),
                Some(run_void_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_click(&mut self) {
        unsafe {
            sys::oneui_nav_item_set_on_click(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for NavItem {
    fn drop(&mut self) {
        self.clear_on_click();
    }
}

struct TextFieldChangedCallback {
    handler: Box<dyn FnMut(String) + 'static>,
}

unsafe extern "C" fn run_text_field_changed_callback(
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

unsafe extern "C" fn run_text_field_submitted_callback(
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
    widget: Widget,
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
    widget: Widget,
    state: Arc<TextAreaState>,
    changed_callback: Option<Box<TextFieldChangedCallback>>,
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
    }
}

struct BoolChangedCallback {
    handler: Box<dyn FnMut(bool) + 'static>,
}

unsafe extern "C" fn run_bool_changed_callback(value: i32, user_data: *mut std::ffi::c_void) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<BoolChangedCallback>() };
    run_callback_guarded("value.bool_changed", || (callback.handler)(value != 0));
}

/// A native binary setting control. The callback is retained by the wrapper,
/// so the C++ event handler can never outlive Rust-owned state.
pub struct Switch {
    widget: Widget,
    changed_callback: Option<Box<BoolChangedCallback>>,
}

impl Switch {
    pub fn new(text: &str) -> Result<Self, Error> {
        let text = wide_null_terminated(text);
        let widget = Widget::from_raw(unsafe { sys::oneui_switch_create(text.as_ptr()) })?;
        Ok(Self {
            widget,
            changed_callback: None,
        })
    }

    pub fn set_text(&self, text: &str) {
        let text = wide_null_terminated(text);
        unsafe { sys::oneui_switch_set_text(self.widget.as_raw(), text.as_ptr()) };
    }

    pub fn set_checked(&self, checked: bool) {
        unsafe { sys::oneui_switch_set_checked(self.widget.as_raw(), i32::from(checked)) };
    }

    pub fn checked(&self) -> bool {
        unsafe { sys::oneui_switch_checked(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(bool) + 'static,
    {
        let trace = InteractionTrace::at("Switch", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(BoolChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("switch callback was just installed")
            as *mut BoolChangedCallback)
            .cast();
        unsafe {
            sys::oneui_switch_set_on_changed(
                self.widget.as_raw(),
                Some(run_bool_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_switch_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.changed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Switch {
    fn drop(&mut self) {
        self.clear_on_changed();
    }
}

/// A native check box for independent option selection. It shares OneUI's
/// focus, accessibility, keyboard, style-sheet and callback lifetime rules.
pub struct Checkbox {
    widget: Widget,
    changed_callback: Option<Box<BoolChangedCallback>>,
}

impl Checkbox {
    pub fn new(text: &str) -> Result<Self, Error> {
        let text = wide_null_terminated(text);
        let widget = Widget::from_raw(unsafe { sys::oneui_checkbox_create(text.as_ptr()) })?;
        Ok(Self {
            widget,
            changed_callback: None,
        })
    }

    pub fn set_text(&self, text: &str) {
        let text = wide_null_terminated(text);
        unsafe { sys::oneui_checkbox_set_text(self.widget.as_raw(), text.as_ptr()) };
    }

    pub fn set_checked(&self, checked: bool) {
        unsafe { sys::oneui_checkbox_set_checked(self.widget.as_raw(), i32::from(checked)) };
    }

    pub fn checked(&self) -> bool {
        unsafe { sys::oneui_checkbox_checked(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(bool) + 'static,
    {
        let trace = InteractionTrace::at("Checkbox", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(BoolChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("checkbox callback was just installed")
            as *mut BoolChangedCallback)
            .cast();
        unsafe {
            sys::oneui_checkbox_set_on_changed(
                self.widget.as_raw(),
                Some(run_bool_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_checkbox_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.changed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Checkbox {
    fn drop(&mut self) {
        self.clear_on_changed();
    }
}

struct IndexChangedCallback {
    handler: Box<dyn FnMut(i32) + 'static>,
}

unsafe extern "C" fn run_index_changed_callback(value: i32, user_data: *mut std::ffi::c_void) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<IndexChangedCallback>() };
    run_callback_guarded("value.index_changed", || (callback.handler)(value));
}

/// Keyboard-accessible segmented choice control backed by OneUI's `Tabs`.
/// Item labels are serialized only at the FFI boundary; product code works
/// exclusively with structured UTF-8 strings.
pub struct SegmentedControl {
    widget: Widget,
    changed_callback: Option<Box<IndexChangedCallback>>,
}

impl SegmentedControl {
    pub fn new(items: &[String]) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_segmented_control_create() })?;
        let control = Self {
            widget,
            changed_callback: None,
        };
        control.set_items(items);
        Ok(control)
    }

    pub fn set_items(&self, items: &[String]) {
        let items = wide_null_terminated(&items.join("|"));
        unsafe { sys::oneui_segmented_control_set_items(self.widget.as_raw(), items.as_ptr()) };
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_segmented_control_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_segmented_control_selected_index(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at(
            "SegmentedControl",
            "changed",
            std::panic::Location::caller(),
        );
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(IndexChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("segmented control callback was just installed")
            as *mut IndexChangedCallback)
            .cast();
        unsafe {
            sys::oneui_segmented_control_set_on_changed(
                self.widget.as_raw(),
                Some(run_index_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_segmented_control_set_on_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.changed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for SegmentedControl {
    fn drop(&mut self) {
        self.clear_on_changed();
    }
}

/// Keyboard-accessible tab strip for switching between peer workspace views.
pub struct Tabs {
    widget: Widget,
    changed_callback: Option<Box<IndexChangedCallback>>,
    close_requested_callback: Option<Box<IndexChangedCallback>>,
    context_menu_requested_callback: Option<Box<ContextMenuRequestedCallback>>,
    reorder_requested_callback: Option<Box<ReorderRequestedCallback>>,
}

impl Tabs {
    pub fn new(items: &[String]) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_tabs_create() })?;
        let control = Self {
            widget,
            changed_callback: None,
            close_requested_callback: None,
            context_menu_requested_callback: None,
            reorder_requested_callback: None,
        };
        control.set_items(items);
        Ok(control)
    }

    pub fn set_items(&self, items: &[String]) {
        let views: Vec<sys::OneUiUtf8String> = items
            .iter()
            .map(|item| sys::OneUiUtf8String::from_str(item))
            .collect();
        unsafe {
            sys::oneui_tabs_set_items_utf8(self.widget.as_raw(), views.as_ptr(), views.len())
        };
    }

    /// Sets an optional leading icon for each tab. Missing entries and `None`
    /// keep the corresponding tab text-only.
    pub fn set_item_icons(&self, icons: &[Option<IconSymbol>]) {
        let symbols = icons
            .iter()
            .map(|icon| icon.map_or(-1, |symbol| symbol as i32))
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_tabs_set_item_icons(self.widget.as_raw(), symbols.as_ptr(), symbols.len())
        };
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_tabs_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_tabs_selected_index(self.widget.as_raw()) }
    }

    /// Uses content-oriented tab widths with bounded horizontal overflow.
    pub fn set_compact(&self, compact: bool) {
        unsafe { sys::oneui_tabs_set_compact(self.widget.as_raw(), i32::from(compact)) };
    }

    pub fn set_item_width_range(&self, minimum: f32, maximum: f32) {
        unsafe { sys::oneui_tabs_set_item_width_range(self.widget.as_raw(), minimum, maximum) };
    }

    pub fn set_closable(&self, closable: bool) {
        unsafe { sys::oneui_tabs_set_closable(self.widget.as_raw(), i32::from(closable)) };
    }

    pub fn set_reorder_enabled(&self, enabled: bool) {
        unsafe { sys::oneui_tabs_set_reorder_enabled(self.widget.as_raw(), i32::from(enabled)) };
    }

    pub fn reorder_enabled(&self) -> bool {
        unsafe { sys::oneui_tabs_reorder_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Tabs", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(IndexChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("tabs callback was just installed")
            as *mut IndexChangedCallback)
            .cast();
        unsafe {
            sys::oneui_tabs_set_on_changed(
                self.widget.as_raw(),
                Some(run_index_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe { sys::oneui_tabs_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut()) };
        self.changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_close_requested<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Tabs", "close_requested", std::panic::Location::caller());
        self.clear_on_close_requested();
        self.close_requested_callback = Some(Box::new(IndexChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .close_requested_callback
            .as_deref_mut()
            .expect("tabs close callback was just installed")
            as *mut IndexChangedCallback)
            .cast();
        unsafe {
            sys::oneui_tabs_set_on_close_requested(
                self.widget.as_raw(),
                Some(run_index_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_close_requested(&mut self) {
        unsafe {
            sys::oneui_tabs_set_on_close_requested(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.close_requested_callback = None;
    }

    #[track_caller]
    pub fn set_on_context_menu_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ContextMenuRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "Tabs",
            "context_menu_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_context_menu_requested();
        self.context_menu_requested_callback = Some(Box::new(ContextMenuRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .context_menu_requested_callback
            .as_deref_mut()
            .expect("tabs context menu callback was just installed")
            as *mut ContextMenuRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_tabs_set_on_context_menu_requested(
                self.widget.as_raw(),
                Some(run_context_menu_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_context_menu_requested(&mut self) {
        unsafe {
            sys::oneui_tabs_set_on_context_menu_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.context_menu_requested_callback = None;
    }

    #[track_caller]
    pub fn set_on_reorder_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ReorderRequest) + 'static,
    {
        let trace =
            InteractionTrace::at("Tabs", "reorder_requested", std::panic::Location::caller());
        self.clear_on_reorder_requested();
        self.reorder_requested_callback = Some(Box::new(ReorderRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .reorder_requested_callback
            .as_deref_mut()
            .expect("tabs reorder callback was just installed")
            as *mut ReorderRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_tabs_set_on_reorder_requested(
                self.widget.as_raw(),
                Some(run_reorder_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_reorder_requested(&mut self) {
        unsafe {
            sys::oneui_tabs_set_on_reorder_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.reorder_requested_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Tabs {
    fn drop(&mut self) {
        self.clear_on_changed();
        self.clear_on_close_requested();
        self.clear_on_context_menu_requested();
        self.clear_on_reorder_requested();
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
    pub const BLINK_SLOW: u32 = 1 << 7;
    pub const BLINK_RAPID: u32 = 1 << 8;
    pub const CONCEAL: u32 = 1 << 9;
    pub const STRIKETHROUGH: u32 = 1 << 10;
    pub const OVERLINE: u32 = 1 << 11;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(u32)]
pub enum TerminalUnderlineStyle {
    #[default]
    None = 0,
    Single = 1,
    Double = 2,
    Curly = 3,
    Dotted = 4,
    Dashed = 5,
}

/// One terminal cell. It deliberately holds text and colors separately so a
/// caller never has to encode values into a string boundary.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TerminalCell {
    pub text: String,
    pub foreground: TerminalColor,
    pub background: TerminalColor,
    pub style: u32,
    /// Stable emulator-owned OSC 8 hyperlink identifier. Zero means no link.
    pub hyperlink_id: u32,
    pub underline_style: TerminalUnderlineStyle,
    /// `None` follows the effective foreground, including inverse video.
    pub underline_color: Option<TerminalColor>,
}

impl Default for TerminalCell {
    fn default() -> Self {
        Self {
            text: String::new(),
            foreground: TerminalColor::default(),
            background: TerminalColor::rgb(20, 24, 36),
            style: 0,
            hyperlink_id: 0,
            underline_style: TerminalUnderlineStyle::None,
            underline_color: None,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalCursor {
    pub row: u16,
    pub column: u16,
    pub visible: bool,
}

/// A half-open terminal cell range. The end column is exclusive.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalSelection {
    pub start_row: u16,
    pub start_column: u16,
    pub end_row: u16,
    pub end_column: u16,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum TerminalCursorStyle {
    Block = 0,
    Bar = 1,
    Underline = 2,
}

/// The whole-cell terminal viewport reported by the native renderer.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalViewport {
    pub rows: u16,
    pub columns: u16,
}

/// An owned terminal snapshot that can be submitted from a session worker.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TerminalFrame {
    pub rows: u16,
    pub columns: u16,
    pub cells: Vec<TerminalCell>,
    pub cursor: TerminalCursor,
    pub cursor_style: TerminalCursorStyle,
    pub cursor_blinking: bool,
    /// Applies frame cursor style and blinking only when the terminal
    /// application explicitly requested them. Otherwise the view keeps its
    /// user-configured defaults from `TerminalViewOptions`.
    pub cursor_style_from_application: bool,
    /// Whether terminal applications currently own pointer input. Shift still
    /// bypasses reporting so users can select text locally.
    pub mouse_reporting: bool,
    /// One-based physical row number for the first visible row. Zero hides
    /// numbers for this frame (for example while using an alternate screen).
    pub first_visible_line_number: u64,
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

/// Pixel layout accepted by [`RealtimeFrameView`].
///
/// NV12 is reserved by the C ABI but is not exposed here until OneUI has a
/// native conversion path; remote decoders should currently submit BGRA or
/// RGBA frames.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum PixelFormat {
    Bgra8888 = 0,
    Rgba8888 = 1,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum VideoScaleMode {
    ActualSize = 0,
    Fit = 1,
    Fill = 2,
    Stretch = 3,
}

/// Immutable decoded video frame that may be produced on any worker thread.
///
/// The pixel allocation is reference-counted and transferred to OneUI without
/// another full-frame copy. `stride == 0` selects the tightly packed stride.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RemoteFrame {
    pub pixels: Arc<[u8]>,
    pub width: i32,
    pub height: i32,
    pub stride: i32,
    pub format: PixelFormat,
    pub frame_id: u64,
    pub timestamp_us: u64,
}

/// Copied dirty rectangle for a [`RemoteFrameDamage`] update.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RemoteFramePatch {
    pub pixels: Arc<[u8]>,
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
    pub stride: i32,
}

impl RemoteFramePatch {
    pub fn new(
        pixels: impl Into<Arc<[u8]>>,
        x: i32,
        y: i32,
        width: i32,
        height: i32,
        stride: i32,
    ) -> Self {
        Self {
            pixels: pixels.into(),
            x,
            y,
            width,
            height,
            stride,
        }
    }
}

/// A batch of dirty rectangles applied to the current compatible remote frame.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RemoteFrameDamage {
    pub width: i32,
    pub height: i32,
    pub format: PixelFormat,
    pub patches: Vec<RemoteFramePatch>,
    pub frame_id: u64,
    pub timestamp_us: u64,
}

impl RemoteFrameDamage {
    pub fn new(
        width: i32,
        height: i32,
        format: PixelFormat,
        patches: Vec<RemoteFramePatch>,
        frame_id: u64,
        timestamp_us: u64,
    ) -> Result<Self, Error> {
        let mut damage = Self {
            width,
            height,
            format,
            patches,
            frame_id,
            timestamp_us,
        };
        damage.normalize_and_validate()?;
        Ok(damage)
    }

    fn normalize_and_validate(&mut self) -> Result<(), Error> {
        const MAX_PATCHES: usize = 64;
        if self.width <= 0 || self.height <= 0 {
            return Err(Error::InvalidVideoFrame {
                reason: "damage frame dimensions must be positive",
            });
        }
        if self.patches.is_empty() || self.patches.len() > MAX_PATCHES {
            return Err(Error::InvalidVideoFrame {
                reason: "damage must contain between 1 and 64 patches",
            });
        }
        for patch in &mut self.patches {
            if patch.width <= 0
                || patch.height <= 0
                || patch.x < 0
                || patch.y < 0
                || patch.x > self.width - patch.width
                || patch.y > self.height - patch.height
            {
                return Err(Error::InvalidVideoFrame {
                    reason: "damage patch is outside the remote frame",
                });
            }
            let row_bytes = patch.width.checked_mul(4).ok_or(Error::InvalidVideoFrame {
                reason: "damage row byte count overflowed",
            })?;
            if patch.stride == 0 {
                patch.stride = row_bytes;
            }
            if patch.stride < row_bytes {
                return Err(Error::InvalidVideoFrame {
                    reason: "damage stride is smaller than a pixel row",
                });
            }
            let required_bytes = usize::try_from(patch.height - 1)
                .ok()
                .and_then(|height| height.checked_mul(patch.stride as usize))
                .and_then(|prefix| prefix.checked_add(row_bytes as usize))
                .ok_or(Error::InvalidVideoFrame {
                    reason: "damage byte count overflowed",
                })?;
            if patch.pixels.len() < required_bytes {
                return Err(Error::InvalidVideoFrame {
                    reason: "damage pixel buffer is shorter than its metadata requires",
                });
            }
        }
        Ok(())
    }
}

impl RemoteFrame {
    pub fn new(
        pixels: impl Into<Arc<[u8]>>,
        width: i32,
        height: i32,
        stride: i32,
        format: PixelFormat,
        frame_id: u64,
        timestamp_us: u64,
    ) -> Result<Self, Error> {
        let mut frame = Self {
            pixels: pixels.into(),
            width,
            height,
            stride,
            format,
            frame_id,
            timestamp_us,
        };
        frame.normalize_and_validate()?;
        Ok(frame)
    }

    fn normalize_and_validate(&mut self) -> Result<(), Error> {
        if self.width <= 0 || self.height <= 0 {
            return Err(Error::InvalidVideoFrame {
                reason: "width and height must be positive",
            });
        }
        let width = usize::try_from(self.width).map_err(|_| Error::InvalidVideoFrame {
            reason: "width is outside the supported range",
        })?;
        let height = usize::try_from(self.height).map_err(|_| Error::InvalidVideoFrame {
            reason: "height is outside the supported range",
        })?;
        let row_bytes = width.checked_mul(4).ok_or(Error::InvalidVideoFrame {
            reason: "row byte count overflowed",
        })?;
        if row_bytes > i32::MAX as usize {
            return Err(Error::InvalidVideoFrame {
                reason: "row byte count exceeds the native stride range",
            });
        }
        if self.stride == 0 {
            self.stride = row_bytes as i32;
        }
        if self.stride < 0 || (self.stride as usize) < row_bytes {
            return Err(Error::InvalidVideoFrame {
                reason: "stride is smaller than a pixel row",
            });
        }
        let required_bytes = (height - 1)
            .checked_mul(self.stride as usize)
            .and_then(|prefix| prefix.checked_add(row_bytes))
            .ok_or(Error::InvalidVideoFrame {
                reason: "frame byte count overflowed",
            })?;
        if self.pixels.len() < required_bytes {
            return Err(Error::InvalidVideoFrame {
                reason: "pixel buffer is shorter than the frame metadata requires",
            });
        }
        Ok(())
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum RemoteCursorMode {
    Default = 0,
    Hidden = 1,
    Bitmap = 2,
}

/// Premultiplied RGBA bitmap used for a server-rendered remote cursor.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RemoteCursorImage {
    pub pixels: Arc<[u8]>,
    pub width: i32,
    pub height: i32,
    pub stride: i32,
    pub hotspot_x: i32,
    pub hotspot_y: i32,
}

impl RemoteCursorImage {
    pub fn new(
        pixels: impl Into<Arc<[u8]>>,
        width: i32,
        height: i32,
        stride: i32,
        hotspot_x: i32,
        hotspot_y: i32,
    ) -> Result<Self, Error> {
        let mut image = Self {
            pixels: pixels.into(),
            width,
            height,
            stride,
            hotspot_x,
            hotspot_y,
        };
        image.normalize_and_validate()?;
        Ok(image)
    }

    fn normalize_and_validate(&mut self) -> Result<(), Error> {
        const MAX_CURSOR_DIMENSION: i32 = 512;
        if self.width <= 0
            || self.height <= 0
            || self.width > MAX_CURSOR_DIMENSION
            || self.height > MAX_CURSOR_DIMENSION
        {
            return Err(Error::InvalidRemoteCursor {
                reason: "cursor dimensions must be between 1 and 512 pixels",
            });
        }
        let row_bytes = self
            .width
            .checked_mul(4)
            .ok_or(Error::InvalidRemoteCursor {
                reason: "cursor row byte count overflowed",
            })?;
        if self.stride == 0 {
            self.stride = row_bytes;
        }
        if self.stride < row_bytes {
            return Err(Error::InvalidRemoteCursor {
                reason: "cursor stride is smaller than a pixel row",
            });
        }
        if self.hotspot_x < 0
            || self.hotspot_x >= self.width
            || self.hotspot_y < 0
            || self.hotspot_y >= self.height
        {
            return Err(Error::InvalidRemoteCursor {
                reason: "cursor hotspot is outside the bitmap",
            });
        }
        let required_bytes = usize::try_from(self.height - 1)
            .ok()
            .and_then(|height| height.checked_mul(self.stride as usize))
            .and_then(|prefix| prefix.checked_add(row_bytes as usize))
            .ok_or(Error::InvalidRemoteCursor {
                reason: "cursor byte count overflowed",
            })?;
        if self.pixels.len() < required_bytes {
            return Err(Error::InvalidRemoteCursor {
                reason: "cursor pixel buffer is shorter than its metadata requires",
            });
        }
        Ok(())
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RemotePointerButton {
    None,
    Left,
    Right,
    Middle,
    X1,
    X2,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct RemotePointerEvent {
    pub window_x: f32,
    pub window_y: f32,
    pub content_x: f32,
    pub content_y: f32,
    pub normalized_x: f32,
    pub normalized_y: f32,
    pub remote_x: f32,
    pub remote_y: f32,
    pub button: RemotePointerButton,
    pub pressed: bool,
    pub wheel_delta_x: i32,
    pub wheel_delta_y: i32,
}

impl From<sys::OneUiRemotePointerEvent> for RemotePointerEvent {
    fn from(value: sys::OneUiRemotePointerEvent) -> Self {
        let button = match value.button {
            1 => RemotePointerButton::Left,
            2 => RemotePointerButton::Right,
            3 => RemotePointerButton::Middle,
            4 => RemotePointerButton::X1,
            5 => RemotePointerButton::X2,
            _ => RemotePointerButton::None,
        };
        Self {
            window_x: value.window_x,
            window_y: value.window_y,
            content_x: value.content_x,
            content_y: value.content_y,
            normalized_x: value.normalized_x,
            normalized_y: value.normalized_y,
            remote_x: value.remote_x,
            remote_y: value.remote_y,
            button,
            pressed: value.pressed != 0,
            wheel_delta_x: value.wheel_delta_x,
            wheel_delta_y: value.wheel_delta_y,
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

struct RemotePointerCallback {
    handler: Box<dyn FnMut(RemotePointerEvent) + 'static>,
}

struct RemoteRawKeyCallback {
    handler: Box<dyn FnMut(RawKeyEvent) + 'static>,
}

struct RemoteTextInputCallback {
    handler: Box<dyn FnMut(String) + 'static>,
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

unsafe extern "C" fn run_remote_text_input_callback(
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

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TerminalPointerAction {
    Press,
    Release,
    Move,
    Wheel,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TerminalPointerButton {
    None,
    Left,
    Right,
    Middle,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum TerminalAuxiliaryButtonAction {
    Ignore = 0,
    Copy = 1,
    Paste = 2,
    Callback = 3,
}

/// Complete visual and pointer configuration for a terminal view.
///
/// Applying this value through a `TerminalViewHandle` uses one UI-thread
/// dispatch, so a settings change cannot enqueue a burst of independent
/// terminal mutations.
#[derive(Debug, Clone, PartialEq)]
pub struct TerminalViewOptions {
    pub font_family: String,
    pub font_size: f32,
    pub line_height: f32,
    pub letter_spacing: f32,
    pub cursor_style: TerminalCursorStyle,
    pub cursor_blinking: bool,
    /// Whether an application-originated DECSCUSR sequence may override the
    /// user's configured cursor shape. Keeping this explicit prevents frame
    /// updates from silently undoing a preference applied through settings.
    pub honor_application_cursor_style: bool,
    pub copy_on_select: bool,
    pub right_button_action: TerminalAuxiliaryButtonAction,
    pub middle_button_action: TerminalAuxiliaryButtonAction,
    pub scroll_rows_per_wheel: f32,
    pub line_numbers_visible: bool,
    pub background: TerminalColor,
    pub foreground: TerminalColor,
    pub cursor: TerminalColor,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct TerminalPointerEvent {
    pub action: TerminalPointerAction,
    pub button: TerminalPointerButton,
    pub x: f32,
    pub y: f32,
    pub row: u16,
    pub column: u16,
    pub wheel_delta: i32,
    pub shift: bool,
    pub control: bool,
    pub alt: bool,
}

impl From<sys::OneUiTerminalPointerEvent> for TerminalPointerEvent {
    fn from(value: sys::OneUiTerminalPointerEvent) -> Self {
        let action = match value.action {
            0 => TerminalPointerAction::Press,
            1 => TerminalPointerAction::Release,
            3 => TerminalPointerAction::Wheel,
            _ => TerminalPointerAction::Move,
        };
        let button = match value.button {
            1 => TerminalPointerButton::Left,
            2 => TerminalPointerButton::Right,
            3 => TerminalPointerButton::Middle,
            _ => TerminalPointerButton::None,
        };
        Self {
            action,
            button,
            x: value.x,
            y: value.y,
            row: value.row,
            column: value.column,
            wheel_delta: value.wheel_delta,
            shift: value.shift != 0,
            control: value.control != 0,
            alt: value.alt != 0,
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

unsafe extern "C" fn run_window_raw_key_callback(
    event: *const sys::OneUiRawKeyEvent,
    user_data: *mut std::ffi::c_void,
) -> std::ffi::c_int {
    if event.is_null() || user_data.is_null() {
        return 0;
    }
    let event = RawKeyEvent::from(unsafe { *event });
    let callback = unsafe { &mut *user_data.cast::<WindowRawKeyCallback>() };
    i32::from(run_callback_guarded("window.raw_key", || (callback.handler)(event)).unwrap_or(false))
}

unsafe extern "C" fn run_window_client_size_changed_callback(
    width: f32,
    height: f32,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<WindowClientSizeChangedCallback>() };
    run_callback_guarded("window.client_size_changed", || {
        (callback.handler)(width, height)
    });
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
        self.clear_focus_callback();
    }
}

fn apply_terminal_frame(
    raw: *mut sys::OneUiWidget,
    frame: &TerminalFrame,
    previous: Option<&TerminalFrame>,
    honor_application_cursor_style: bool,
) {
    unsafe {
        sys::oneui_terminal_view_set_first_visible_line_number(raw, frame.first_visible_line_number)
    };
    let can_update_in_place = previous.is_some_and(|previous| {
        previous.rows == frame.rows
            && previous.columns == frame.columns
            && previous.cells.len() == frame.cells.len()
    });
    if can_update_in_place {
        for range in terminal_dirty_ranges(previous.expect("previous frame checked"), frame) {
            apply_terminal_cell_update(raw, range.start, &frame.cells[range]);
        }
    } else {
        apply_terminal_grid(raw, frame.rows, frame.columns, &frame.cells);
    }
    unsafe {
        sys::oneui_terminal_view_set_mouse_reporting(raw, i32::from(frame.mouse_reporting));
        if should_apply_application_cursor_style(
            honor_application_cursor_style,
            frame.cursor_style_from_application,
        ) {
            sys::oneui_terminal_view_set_cursor_style(raw, frame.cursor_style as i32);
            sys::oneui_terminal_view_set_cursor_blinking(raw, i32::from(frame.cursor_blinking));
        }
        sys::oneui_terminal_view_set_cursor(
            raw,
            frame.cursor.row,
            frame.cursor.column,
            i32::from(frame.cursor.visible),
        )
    };
}

fn should_apply_application_cursor_style(
    honor_application_cursor_style: bool,
    cursor_style_from_application: bool,
) -> bool {
    honor_application_cursor_style && cursor_style_from_application
}

fn terminal_dirty_ranges(
    previous: &TerminalFrame,
    frame: &TerminalFrame,
) -> Vec<std::ops::Range<usize>> {
    const MAX_SPARSE_RANGES: usize = 32;

    let mut ranges = Vec::with_capacity(4);
    let mut current_start = None;
    let mut first_changed = None;
    let mut last_changed = 0;
    let mut collapsed = false;

    for (index, (previous, current)) in previous.cells.iter().zip(&frame.cells).enumerate() {
        if previous != current {
            first_changed.get_or_insert(index);
            last_changed = index;
            current_start.get_or_insert(index);
        } else if let Some(start) = current_start.take() {
            if !collapsed {
                ranges.push(start..index);
                if ranges.len() > MAX_SPARSE_RANGES {
                    ranges.clear();
                    collapsed = true;
                }
            }
        }
    }
    if let Some(start) = current_start {
        if !collapsed {
            ranges.push(start..last_changed + 1);
            if ranges.len() > MAX_SPARSE_RANGES {
                collapsed = true;
            }
        }
    }

    if collapsed {
        std::iter::once(
            first_changed.expect("collapsed ranges require a changed cell")..last_changed + 1,
        )
        .collect()
    } else {
        ranges
    }
}

fn apply_terminal_view_options(raw: *mut sys::OneUiWidget, options: &TerminalViewOptions) {
    unsafe {
        sys::oneui_terminal_view_set_font_family_utf8(
            raw,
            sys::OneUiUtf8String::from_str(&options.font_family),
        );
        sys::oneui_terminal_view_set_font_size(raw, options.font_size);
        sys::oneui_terminal_view_set_line_height(raw, options.line_height);
        sys::oneui_terminal_view_set_letter_spacing(raw, options.letter_spacing);
        sys::oneui_terminal_view_set_cursor_style(raw, options.cursor_style as i32);
        sys::oneui_terminal_view_set_cursor_blinking(raw, i32::from(options.cursor_blinking));
        sys::oneui_terminal_view_set_copy_on_select(raw, i32::from(options.copy_on_select));
        sys::oneui_terminal_view_set_right_button_action(raw, options.right_button_action as i32);
        sys::oneui_terminal_view_set_middle_button_action(raw, options.middle_button_action as i32);
        sys::oneui_terminal_view_set_scroll_rows_per_wheel(raw, options.scroll_rows_per_wheel);
        sys::oneui_terminal_view_set_line_numbers_visible(
            raw,
            i32::from(options.line_numbers_visible),
        );
        sys::oneui_terminal_view_set_palette(
            raw,
            options.background.into(),
            options.foreground.into(),
            options.cursor.into(),
        );
    }
}

fn apply_terminal_grid(
    raw: *mut sys::OneUiWidget,
    rows: u16,
    columns: u16,
    cells: &[TerminalCell],
) {
    let native_cells = native_terminal_cells(cells);
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

fn apply_terminal_cell_update(
    raw: *mut sys::OneUiWidget,
    first_cell: usize,
    cells: &[TerminalCell],
) {
    let native_cells = native_terminal_cells(cells);
    unsafe {
        sys::oneui_terminal_view_update_cells_utf8(
            raw,
            first_cell,
            native_cells.as_ptr(),
            native_cells.len(),
        )
    };
}

fn native_terminal_cells(cells: &[TerminalCell]) -> Vec<sys::OneUiTerminalCellUtf8> {
    cells
        .iter()
        .map(|cell| sys::OneUiTerminalCellUtf8 {
            text: sys::OneUiUtf8String::from_str(&cell.text),
            foreground: cell.foreground.into(),
            background: cell.background.into(),
            style: cell.style,
            hyperlink_id: cell.hyperlink_id,
            underline_style: cell.underline_style as u32,
            underline_color: cell
                .underline_color
                .unwrap_or(TerminalColor::rgb(0, 0, 0))
                .into(),
            underline_color_set: i32::from(cell.underline_color.is_some()),
        })
        .collect()
}

/// A single immutable line for the selectable native log view.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LogLine {
    pub text: String,
    pub color: Color,
}

/// A native, selectable, multi-line log surface.
///
/// Use this for command output and diagnostics. It deliberately keeps one
/// structured line per append, rather than asking a label control to emulate
/// terminal layout with embedded newlines.
pub struct LogView {
    widget: Widget,
}

impl LogView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_log_view_create() })?;
        Ok(Self { widget })
    }

    pub fn append_line(&self, line: &LogLine) {
        let text = wide_null_terminated(&line.text);
        unsafe {
            sys::oneui_log_view_append_line(
                self.widget.as_raw(),
                text.as_ptr(),
                line.color.r,
                line.color.g,
                line.color.b,
                line.color.a,
            )
        };
    }

    pub fn set_lines(&self, lines: &[LogLine]) {
        self.clear();
        for line in lines {
            self.append_line(line);
        }
    }

    pub fn clear(&self) {
        unsafe { sys::oneui_log_view_clear(self.widget.as_raw()) };
    }

    pub fn content_height(&self) -> f32 {
        unsafe { sys::oneui_log_view_content_height(self.widget.as_raw()) }
    }

    pub fn set_font_size(&self, size: f32) {
        unsafe { sys::oneui_log_view_set_font_size(self.widget.as_raw(), size) };
    }

    pub fn set_line_height(&self, height: f32) {
        unsafe { sys::oneui_log_view_set_line_height(self.widget.as_raw(), height) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

fn native_virtual_list_item(item: &VirtualListItem) -> sys::OneUiRichListItemUtf8 {
    sys::OneUiRichListItemUtf8 {
        title: sys::OneUiUtf8String::from_str(&item.title),
        detail: sys::OneUiUtf8String::from_str(&item.detail),
        badge: sys::OneUiUtf8String::from_str(&item.badge),
        trailing: sys::OneUiUtf8String::from_str(&item.trailing),
        indicator_color: item
            .indicator_color
            .unwrap_or(Color::rgba(0, 0, 0, 0))
            .into(),
        trailing_color: item
            .trailing_color
            .unwrap_or(Color::rgba(0, 0, 0, 0))
            .into(),
        indicator_visible: i32::from(item.indicator_color.is_some()),
    }
}

struct ListChangedCallback {
    handler: Box<dyn FnMut(i32) + 'static>,
}

unsafe extern "C" fn run_list_changed_callback(value: i32, user_data: *mut std::ffi::c_void) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<ListChangedCallback>() };
    run_callback_guarded("list.index_changed", || (callback.handler)(value));
}

struct ListSelectionChangedCallback {
    handler: Box<dyn FnMut(Vec<i32>) + 'static>,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ContextMenuRequest {
    pub index: i32,
    pub x: f32,
    pub y: f32,
}

/// A proposed list reorder. The control never mutates application data;
/// callers validate and apply this request before assigning updated items.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ReorderRequest {
    pub source_index: i32,
    pub target_index: i32,
}

struct ContextMenuRequestedCallback {
    handler: Box<dyn FnMut(ContextMenuRequest) + 'static>,
}

struct ReorderRequestedCallback {
    handler: Box<dyn FnMut(ReorderRequest) + 'static>,
}

unsafe extern "C" fn run_list_selection_changed_callback(
    values: *const i32,
    count: usize,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() || (values.is_null() && count > 0) {
        return;
    }
    let selected = if count == 0 {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(values, count) }.to_vec()
    };
    let callback = unsafe { &mut *user_data.cast::<ListSelectionChangedCallback>() };
    run_callback_guarded("list.selection_changed", || (callback.handler)(selected));
}

unsafe extern "C" fn run_context_menu_requested_callback(
    index: i32,
    x: f32,
    y: f32,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<ContextMenuRequestedCallback>() };
    let request = ContextMenuRequest { index, x, y };
    run_callback_guarded("list.context_menu", || (callback.handler)(request));
}

unsafe extern "C" fn run_reorder_requested_callback(
    source_index: i32,
    target_index: i32,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<ReorderRequestedCallback>() };
    let request = ReorderRequest {
        source_index,
        target_index,
    };
    run_callback_guarded("list.reorder_requested", || (callback.handler)(request));
}

/// A selectable native list.
pub struct List {
    widget: Widget,
    changed_callback: Option<Box<ListChangedCallback>>,
}

impl List {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_list_create() })?;
        Ok(Self {
            widget,
            changed_callback: None,
        })
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

    /// Controls whether the list must always keep one item selected.
    ///
    /// The default is `true` for backward compatibility. Set this to `false`
    /// before assigning index `-1` when the surrounding editor has an explicit
    /// new/empty state.
    pub fn set_selection_required(&self, required: bool) {
        unsafe {
            sys::oneui_list_set_selection_required(self.widget.as_raw(), i32::from(required))
        };
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_list_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_list_selected_index(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("List", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("list callback was just installed")
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_list_set_on_changed(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe { sys::oneui_list_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut()) };
        self.changed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for List {
    fn drop(&mut self) {
        self.clear_on_changed();
    }
}

/// A fixed-height viewport-virtualized list for large structured data sets.
///
/// Unlike [`List`], this control retains all data but only paints rows visible
/// in its viewport. Keep row content to a title and optional detail line.
enum PendingVirtualListItem {
    Basic(ListItem),
    Rich(VirtualListItem),
}

struct VirtualListState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_items: Mutex<BTreeMap<usize, PendingVirtualListItem>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for in-place row updates on a mounted virtual list.
///
/// Updates are coalesced per row and applied on the owning UI thread. Unlike a
/// full item reset, row updates preserve selection, scroll offset, and active
/// scroll motion.
#[derive(Clone)]
pub struct VirtualListHandle {
    state: Arc<VirtualListState>,
    dispatcher: UiDispatcher,
}

impl VirtualListHandle {
    /// Replaces the complete data revision on the owning window thread.
    ///
    /// Use this for background loads, filters, and directory scans. Incremental
    /// status changes should continue to use [`Self::update_item`] so active
    /// scrolling and selection are preserved.
    pub fn set_items(&self, items: Vec<ListItem>) -> Result<(), Error> {
        self.replace_items(items, false)
    }

    /// Replaces the complete data revision with structured operational rows.
    pub fn set_rich_items(&self, items: Vec<VirtualListItem>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            state
                .pending_items
                .lock()
                .expect("virtual list pending items lock poisoned")
                .clear();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            let native_items = items
                .iter()
                .map(native_virtual_list_item)
                .collect::<Vec<_>>();
            unsafe {
                sys::oneui_virtual_list_set_rich_items_utf8(
                    raw,
                    native_items.as_ptr(),
                    native_items.len(),
                );
            }
        })
    }

    /// Replaces the complete data revision and clears the selection in the
    /// same UI-thread task.
    ///
    /// Use this when row indices do not identify the same logical objects
    /// across revisions, such as directory listings after rename or delete.
    /// Clearing atomically prevents a retained index from targeting a
    /// different row between the revision and a later selection reset.
    pub fn set_items_and_clear_selection(&self, items: Vec<ListItem>) -> Result<(), Error> {
        self.replace_items(items, true)
    }

    fn replace_items(&self, items: Vec<ListItem>, clear_selection: bool) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            trace_ui_task("virtual-list revision started");
            state
                .pending_items
                .lock()
                .expect("virtual list pending items lock poisoned")
                .clear();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            let native_items = items
                .iter()
                .map(|item| sys::OneUiListItemUtf8 {
                    title: sys::OneUiUtf8String::from_str(&item.title),
                    detail: sys::OneUiUtf8String::from_str(&item.detail),
                })
                .collect::<Vec<_>>();
            unsafe {
                sys::oneui_virtual_list_set_items_utf8(
                    raw,
                    native_items.as_ptr(),
                    native_items.len(),
                );
                if clear_selection {
                    sys::oneui_virtual_list_set_selected_index(raw, -1);
                }
            };
            trace_ui_task("virtual-list revision completed");
        })
    }

    pub fn update_item(&self, index: usize, item: ListItem) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }

        self.state
            .pending_items
            .lock()
            .expect("virtual list pending items lock poisoned")
            .insert(index, PendingVirtualListItem::Basic(item));
        self.schedule_pending_items()
    }

    /// Coalesces one operational-row update onto the owning window thread.
    pub fn update_rich_item(&self, index: usize, item: VirtualListItem) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        self.state
            .pending_items
            .lock()
            .expect("virtual list pending items lock poisoned")
            .insert(index, PendingVirtualListItem::Rich(item));
        self.schedule_pending_items()
    }

    fn schedule_pending_items(&self) -> Result<(), Error> {
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }

        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_items(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_items
                .lock()
                .expect("virtual list pending items lock poisoned")
                .clear();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_items(state: &VirtualListState) {
        loop {
            let items = std::mem::take(
                &mut *state
                    .pending_items
                    .lock()
                    .expect("virtual list pending items lock poisoned"),
            );
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }

            for (index, item) in items {
                match item {
                    PendingVirtualListItem::Basic(item) => {
                        let native_item = sys::OneUiListItemUtf8 {
                            title: sys::OneUiUtf8String::from_str(&item.title),
                            detail: sys::OneUiUtf8String::from_str(&item.detail),
                        };
                        unsafe {
                            sys::oneui_virtual_list_update_item_utf8(raw, index, &native_item);
                        }
                    }
                    PendingVirtualListItem::Rich(item) => {
                        let native_item = native_virtual_list_item(&item);
                        unsafe {
                            sys::oneui_virtual_list_update_rich_item_utf8(raw, index, &native_item);
                        }
                    }
                }
            }

            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_items
                .lock()
                .expect("virtual list pending items lock poisoned")
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

pub struct VirtualList {
    widget: Widget,
    state: Arc<VirtualListState>,
    changed_callback: Option<Box<ListChangedCallback>>,
    selection_changed_callback: Option<Box<ListSelectionChangedCallback>>,
    activated_callback: Option<Box<ListChangedCallback>>,
    edit_requested_callback: Option<Box<ListChangedCallback>>,
    delete_requested_callback: Option<Box<ListSelectionChangedCallback>>,
    context_menu_requested_callback: Option<Box<ContextMenuRequestedCallback>>,
    reorder_requested_callback: Option<Box<ReorderRequestedCallback>>,
    item_drag_callback: Option<Box<ItemDragCallback>>,
}

impl VirtualList {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_virtual_list_create() })?;
        Ok(Self {
            state: Arc::new(VirtualListState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_items: Mutex::new(BTreeMap::new()),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
            changed_callback: None,
            selection_changed_callback: None,
            activated_callback: None,
            edit_requested_callback: None,
            delete_requested_callback: None,
            context_menu_requested_callback: None,
            reorder_requested_callback: None,
            item_drag_callback: None,
        })
    }

    pub fn set_items(&self, items: &[ListItem]) {
        // A full data revision invalidates queued row patches from the previous
        // revision. Keep the scheduled drain alive so updates submitted after
        // this reset still use the already queued UI-thread task.
        self.state
            .pending_items
            .lock()
            .expect("virtual list pending items lock poisoned")
            .clear();
        let native_items: Vec<sys::OneUiListItemUtf8> = items
            .iter()
            .map(|item| sys::OneUiListItemUtf8 {
                title: sys::OneUiUtf8String::from_str(&item.title),
                detail: sys::OneUiUtf8String::from_str(&item.detail),
            })
            .collect();
        unsafe {
            sys::oneui_virtual_list_set_items_utf8(
                self.widget.as_raw(),
                native_items.as_ptr(),
                native_items.len(),
            )
        };
    }

    pub fn set_rich_items(&self, items: &[VirtualListItem]) {
        self.state
            .pending_items
            .lock()
            .expect("virtual list pending items lock poisoned")
            .clear();
        let native_items = items
            .iter()
            .map(native_virtual_list_item)
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_virtual_list_set_rich_items_utf8(
                self.widget.as_raw(),
                native_items.as_ptr(),
                native_items.len(),
            )
        };
    }

    /// Replaces one row without resetting selection, scrolling, or motion.
    pub fn update_item(&self, index: usize, item: &ListItem) -> bool {
        let native_item = sys::OneUiListItemUtf8 {
            title: sys::OneUiUtf8String::from_str(&item.title),
            detail: sys::OneUiUtf8String::from_str(&item.detail),
        };
        unsafe {
            sys::oneui_virtual_list_update_item_utf8(self.widget.as_raw(), index, &native_item) != 0
        }
    }

    /// Replaces one operational row without resetting selection or scrolling.
    pub fn update_rich_item(&self, index: usize, item: &VirtualListItem) -> bool {
        let native_item = native_virtual_list_item(item);
        unsafe {
            sys::oneui_virtual_list_update_rich_item_utf8(self.widget.as_raw(), index, &native_item)
                != 0
        }
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_virtual_list_set_selected_index(self.widget.as_raw(), index) };
    }

    /// Clears the active row while preserving the list contents and scroll position.
    pub fn clear_selection(&self) {
        self.set_selected_index(-1);
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_virtual_list_selected_index(self.widget.as_raw()) }
    }

    pub fn set_selection_mode(&self, mode: SelectionMode) {
        let native_mode = match mode {
            SelectionMode::Single => 0,
            SelectionMode::Multiple => 1,
        };
        unsafe { sys::oneui_virtual_list_set_selection_mode(self.widget.as_raw(), native_mode) };
    }

    pub fn set_selected_indices(&self, indices: &[i32]) {
        unsafe {
            sys::oneui_virtual_list_set_selected_indices(
                self.widget.as_raw(),
                indices.as_ptr(),
                indices.len(),
            )
        };
    }

    pub fn selected_indices(&self) -> Vec<i32> {
        let count = unsafe {
            sys::oneui_virtual_list_selected_indices(self.widget.as_raw(), std::ptr::null_mut(), 0)
        };
        let mut values = vec![0; count];
        if count > 0 {
            unsafe {
                sys::oneui_virtual_list_selected_indices(
                    self.widget.as_raw(),
                    values.as_mut_ptr(),
                    values.len(),
                )
            };
        }
        values
    }

    pub fn set_row_height(&self, height: f32) {
        unsafe { sys::oneui_virtual_list_set_row_height(self.widget.as_raw(), height) };
    }

    pub fn set_rich_metrics(&self, metrics: VirtualListRichMetrics) {
        unsafe {
            sys::oneui_virtual_list_set_rich_metrics(
                self.widget.as_raw(),
                metrics.indicator_space,
                metrics.indicator_diameter,
                metrics.badge_height,
                metrics.badge_radius,
                metrics.badge_horizontal_padding,
                metrics.title_badge_gap,
                metrics.trailing_width,
                metrics.trailing_gap,
            )
        };
    }

    pub fn rich_metrics(&self) -> VirtualListRichMetrics {
        let mut metrics = sys::OneUiVirtualListRichMetrics::default();
        let read =
            unsafe { sys::oneui_virtual_list_rich_metrics(self.widget.as_raw(), &mut metrics) };
        if read == 0 {
            return VirtualListRichMetrics::default();
        }
        VirtualListRichMetrics {
            indicator_space: metrics.indicator_space,
            indicator_diameter: metrics.indicator_diameter,
            badge_height: metrics.badge_height,
            badge_radius: metrics.badge_radius,
            badge_horizontal_padding: metrics.badge_horizontal_padding,
            title_badge_gap: metrics.title_badge_gap,
            trailing_width: metrics.trailing_width,
            trailing_gap: metrics.trailing_gap,
        }
    }

    pub fn set_scroll_offset(&self, offset: f32) {
        unsafe { sys::oneui_virtual_list_set_scroll_offset(self.widget.as_raw(), offset) };
    }

    pub fn scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_virtual_list_scroll_offset(self.widget.as_raw()) }
    }

    pub fn max_scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_virtual_list_max_scroll_offset(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("VirtualList", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("virtual list callback was just installed")
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_changed(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_selection_changed<F>(&mut self, callback: F)
    where
        F: FnMut(Vec<i32>) + 'static,
    {
        let trace = InteractionTrace::at(
            "VirtualList",
            "selection_changed",
            std::panic::Location::caller(),
        );
        self.clear_on_selection_changed();
        self.selection_changed_callback = Some(Box::new(ListSelectionChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .selection_changed_callback
            .as_deref_mut()
            .expect("virtual list selection callback was just installed")
            as *mut ListSelectionChangedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_selection_changed(
                self.widget.as_raw(),
                Some(run_list_selection_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_selection_changed(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_selection_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.selection_changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_activated<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace =
            InteractionTrace::at("VirtualList", "activated", std::panic::Location::caller());
        self.clear_on_activated();
        self.activated_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .activated_callback
            .as_deref_mut()
            .expect("virtual list activation callback was just installed")
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_activated(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_activated(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_activated(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.activated_callback = None;
    }

    #[track_caller]
    pub fn set_on_edit_requested<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at(
            "VirtualList",
            "edit_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_edit_requested();
        self.edit_requested_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .edit_requested_callback
            .as_deref_mut()
            .expect("virtual list edit callback was just installed")
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_edit_requested(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_edit_requested(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_edit_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.edit_requested_callback = None;
    }

    /// Runs the standard Delete command with the complete current selection.
    #[track_caller]
    pub fn set_on_delete_requested<F>(&mut self, callback: F)
    where
        F: FnMut(Vec<i32>) + 'static,
    {
        let trace = InteractionTrace::at(
            "VirtualList",
            "delete_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_delete_requested();
        self.delete_requested_callback = Some(Box::new(ListSelectionChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .delete_requested_callback
            .as_deref_mut()
            .expect("virtual list delete callback was just installed")
            as *mut ListSelectionChangedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_delete_requested(
                self.widget.as_raw(),
                Some(run_list_selection_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_delete_requested(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_delete_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.delete_requested_callback = None;
    }

    #[track_caller]
    pub fn set_on_context_menu_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ContextMenuRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "VirtualList",
            "context_menu_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_context_menu_requested();
        self.context_menu_requested_callback = Some(Box::new(ContextMenuRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .context_menu_requested_callback
            .as_deref_mut()
            .expect("virtual list context menu callback was just installed")
            as *mut ContextMenuRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_context_menu_requested(
                self.widget.as_raw(),
                Some(run_context_menu_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_context_menu_requested(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_context_menu_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.context_menu_requested_callback = None;
    }

    /// Enables pointer and `Alt+Up`/`Alt+Down` reorder requests.
    pub fn set_reorder_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_virtual_list_set_reorder_enabled(self.widget.as_raw(), i32::from(enabled))
        };
    }

    pub fn reorder_enabled(&self) -> bool {
        unsafe { sys::oneui_virtual_list_reorder_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_reorder_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ReorderRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "VirtualList",
            "reorder_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_reorder_requested();
        self.reorder_requested_callback = Some(Box::new(ReorderRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .reorder_requested_callback
            .as_deref_mut()
            .expect("virtual list reorder callback was just installed")
            as *mut ReorderRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_reorder_requested(
                self.widget.as_raw(),
                Some(run_reorder_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_reorder_requested(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_reorder_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.reorder_requested_callback = None;
    }

    /// Assigns stable domain identifiers to the current rows for external drag operations.
    ///
    /// The identifiers must be non-empty, unique, and exactly match the current row count.
    /// A full [`Self::set_items`] reset clears them so stale identities cannot be emitted.
    pub fn set_item_drag_ids(&self, ids: &[String]) -> bool {
        let native_ids: Vec<sys::OneUiUtf8String> = ids
            .iter()
            .map(|id| sys::OneUiUtf8String::from_str(id))
            .collect();
        unsafe {
            sys::oneui_virtual_list_set_item_drag_ids_utf8(
                self.widget.as_raw(),
                native_ids.as_ptr(),
                native_ids.len(),
            ) != 0
        }
    }

    pub fn set_item_drag_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_virtual_list_set_item_drag_enabled(self.widget.as_raw(), i32::from(enabled))
        };
    }

    pub fn item_drag_enabled(&self) -> bool {
        unsafe { sys::oneui_virtual_list_item_drag_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_item_drag<F>(&mut self, callback: F)
    where
        F: FnMut(ItemDragEvent) + 'static,
    {
        let trace =
            InteractionTrace::at("VirtualList", "item_drag", std::panic::Location::caller());
        self.clear_on_item_drag();
        self.item_drag_callback = Some(Box::new(ItemDragCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .item_drag_callback
            .as_deref_mut()
            .expect("virtual list item drag callback was just installed")
            as *mut ItemDragCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_item_drag_utf8(
                self.widget.as_raw(),
                Some(run_item_drag_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_item_drag(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_item_drag_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.item_drag_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for VirtualList {
    fn drop(&mut self) {
        self.clear_on_item_drag();
        self.clear_on_reorder_requested();
        self.clear_on_context_menu_requested();
        self.clear_on_delete_requested();
        self.clear_on_edit_requested();
        self.clear_on_activated();
        self.clear_on_selection_changed();
        self.clear_on_changed();
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_items
            .lock()
            .expect("virtual list pending items lock poisoned")
            .clear();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}

/// A column in a native table. A width of `0` consumes the remaining space.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct TableColumn {
    pub header: String,
    pub width: f32,
}

/// Structured UTF-8 data for one native table row.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct TableRow {
    pub cells: Vec<String>,
}

struct TableState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_rows: Mutex<BTreeMap<usize, TableRow>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for table data owned by a window thread.
#[derive(Clone)]
pub struct TableHandle {
    state: Arc<TableState>,
    dispatcher: UiDispatcher,
}

impl TableHandle {
    pub fn set_rows(&self, rows: Vec<TableRow>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
                .clear();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            set_table_rows_raw(raw, &rows);
        })
    }

    /// Replaces the current revision and clears index-based selection in the
    /// same window-thread transaction. Use this when row identity changed so
    /// commands cannot accidentally target a different row that inherited the
    /// previous index.
    pub fn set_rows_and_clear_selection(&self, rows: Vec<TableRow>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
                .clear();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            set_table_rows_raw(raw, &rows);
            unsafe {
                sys::oneui_table_set_selected_indices(raw, std::ptr::null(), 0);
            }
        })
    }

    pub fn update_row(&self, index: usize, row: TableRow) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        self.state
            .pending_rows
            .lock()
            .expect("table pending rows lock poisoned")
            .insert(index, row);
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }
        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_rows(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
                .clear();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_rows(state: &TableState) {
        loop {
            let rows = std::mem::take(
                &mut *state
                    .pending_rows
                    .lock()
                    .expect("table pending rows lock poisoned"),
            );
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            for (index, row) in rows {
                update_table_row_raw(raw, index, &row);
            }
            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
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

fn set_table_rows_raw(raw: *mut sys::OneUiWidget, rows: &[TableRow]) {
    let native_cells = rows
        .iter()
        .map(|row| {
            row.cells
                .iter()
                .map(|cell| sys::OneUiUtf8String::from_str(cell))
                .collect::<Vec<_>>()
        })
        .collect::<Vec<_>>();
    let native_rows = native_cells
        .iter()
        .map(|cells| sys::OneUiTableRowUtf8 {
            cells: cells.as_ptr(),
            cell_count: cells.len(),
        })
        .collect::<Vec<_>>();
    unsafe { sys::oneui_table_set_rows_utf8(raw, native_rows.as_ptr(), native_rows.len()) };
}

fn update_table_row_raw(raw: *mut sys::OneUiWidget, index: usize, row: &TableRow) -> bool {
    let cells = row
        .cells
        .iter()
        .map(|cell| sys::OneUiUtf8String::from_str(cell))
        .collect::<Vec<_>>();
    let native_row = sys::OneUiTableRowUtf8 {
        cells: cells.as_ptr(),
        cell_count: cells.len(),
    };
    unsafe { sys::oneui_table_update_row_utf8(raw, index, &native_row) != 0 }
}

/// A fixed-row-height, viewport-virtualized native data table.
pub struct Table {
    widget: Widget,
    state: Arc<TableState>,
    changed_callback: Option<Box<ListChangedCallback>>,
    selection_changed_callback: Option<Box<ListSelectionChangedCallback>>,
    activated_callback: Option<Box<ListChangedCallback>>,
    edit_requested_callback: Option<Box<ListChangedCallback>>,
    delete_requested_callback: Option<Box<ListSelectionChangedCallback>>,
    context_menu_requested_callback: Option<Box<ContextMenuRequestedCallback>>,
    reorder_requested_callback: Option<Box<ReorderRequestedCallback>>,
    item_drag_callback: Option<Box<ItemDragCallback>>,
}

impl Table {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_table_create() })?;
        Ok(Self {
            state: Arc::new(TableState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_rows: Mutex::new(BTreeMap::new()),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
            changed_callback: None,
            selection_changed_callback: None,
            activated_callback: None,
            edit_requested_callback: None,
            delete_requested_callback: None,
            context_menu_requested_callback: None,
            reorder_requested_callback: None,
            item_drag_callback: None,
        })
    }

    pub fn set_columns(&self, columns: &[TableColumn]) {
        let native_columns = columns
            .iter()
            .map(|column| sys::OneUiTableColumnUtf8 {
                header: sys::OneUiUtf8String::from_str(&column.header),
                width: column.width,
            })
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_table_set_columns_utf8(
                self.widget.as_raw(),
                native_columns.as_ptr(),
                native_columns.len(),
            )
        };
    }

    pub fn set_rows(&self, rows: &[TableRow]) {
        self.state
            .pending_rows
            .lock()
            .expect("table pending rows lock poisoned")
            .clear();
        set_table_rows_raw(self.widget.as_raw(), rows);
    }

    pub fn update_row(&self, index: usize, row: &TableRow) -> bool {
        update_table_row_raw(self.widget.as_raw(), index, row)
    }

    pub fn set_selection_mode(&self, mode: SelectionMode) {
        let mode = match mode {
            SelectionMode::Single => 0,
            SelectionMode::Multiple => 1,
        };
        unsafe { sys::oneui_table_set_selection_mode(self.widget.as_raw(), mode) };
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_table_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_table_selected_index(self.widget.as_raw()) }
    }

    pub fn set_selected_indices(&self, indices: &[i32]) {
        unsafe {
            sys::oneui_table_set_selected_indices(
                self.widget.as_raw(),
                indices.as_ptr(),
                indices.len(),
            )
        };
    }

    pub fn selected_indices(&self) -> Vec<i32> {
        let count = unsafe {
            sys::oneui_table_selected_indices(self.widget.as_raw(), std::ptr::null_mut(), 0)
        };
        let mut values = vec![0; count];
        if count > 0 {
            unsafe {
                sys::oneui_table_selected_indices(
                    self.widget.as_raw(),
                    values.as_mut_ptr(),
                    values.len(),
                )
            };
        }
        values
    }

    pub fn set_row_height(&self, height: f32) {
        unsafe { sys::oneui_table_set_row_height(self.widget.as_raw(), height) };
    }

    pub fn set_scroll_offset(&self, offset: f32) {
        unsafe { sys::oneui_table_set_scroll_offset(self.widget.as_raw(), offset) };
    }

    pub fn scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_table_scroll_offset(self.widget.as_raw()) }
    }

    pub fn max_scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_table_max_scroll_offset(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Table", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data =
            (self.changed_callback.as_deref_mut().unwrap() as *mut ListChangedCallback).cast();
        unsafe {
            sys::oneui_table_set_on_changed(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_table_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_selection_changed<F>(&mut self, callback: F)
    where
        F: FnMut(Vec<i32>) + 'static,
    {
        let trace =
            InteractionTrace::at("Table", "selection_changed", std::panic::Location::caller());
        self.clear_on_selection_changed();
        self.selection_changed_callback = Some(Box::new(ListSelectionChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self.selection_changed_callback.as_deref_mut().unwrap()
            as *mut ListSelectionChangedCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_selection_changed(
                self.widget.as_raw(),
                Some(run_list_selection_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_selection_changed(&mut self) {
        unsafe {
            sys::oneui_table_set_on_selection_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.selection_changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_activated<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Table", "activated", std::panic::Location::caller());
        self.clear_on_activated();
        self.activated_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data =
            (self.activated_callback.as_deref_mut().unwrap() as *mut ListChangedCallback).cast();
        unsafe {
            sys::oneui_table_set_on_activated(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_activated(&mut self) {
        unsafe {
            sys::oneui_table_set_on_activated(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.activated_callback = None;
    }

    #[track_caller]
    pub fn set_on_edit_requested<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Table", "edit_requested", std::panic::Location::caller());
        self.clear_on_edit_requested();
        self.edit_requested_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self.edit_requested_callback.as_deref_mut().unwrap()
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_edit_requested(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_edit_requested(&mut self) {
        unsafe {
            sys::oneui_table_set_on_edit_requested(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.edit_requested_callback = None;
    }

    #[track_caller]
    pub fn set_on_delete_requested<F>(&mut self, callback: F)
    where
        F: FnMut(Vec<i32>) + 'static,
    {
        let trace =
            InteractionTrace::at("Table", "delete_requested", std::panic::Location::caller());
        self.clear_on_delete_requested();
        self.delete_requested_callback = Some(Box::new(ListSelectionChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self.delete_requested_callback.as_deref_mut().unwrap()
            as *mut ListSelectionChangedCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_delete_requested(
                self.widget.as_raw(),
                Some(run_list_selection_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_delete_requested(&mut self) {
        unsafe {
            sys::oneui_table_set_on_delete_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.delete_requested_callback = None;
    }

    #[track_caller]
    pub fn set_on_context_menu_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ContextMenuRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "Table",
            "context_menu_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_context_menu_requested();
        self.context_menu_requested_callback = Some(Box::new(ContextMenuRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self.context_menu_requested_callback.as_deref_mut().unwrap()
            as *mut ContextMenuRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_context_menu_requested(
                self.widget.as_raw(),
                Some(run_context_menu_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_context_menu_requested(&mut self) {
        unsafe {
            sys::oneui_table_set_on_context_menu_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.context_menu_requested_callback = None;
    }

    /// Enables pointer and `Alt+Up`/`Alt+Down` row reorder requests.
    pub fn set_reorder_enabled(&self, enabled: bool) {
        unsafe { sys::oneui_table_set_reorder_enabled(self.widget.as_raw(), i32::from(enabled)) };
    }

    pub fn reorder_enabled(&self) -> bool {
        unsafe { sys::oneui_table_reorder_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_reorder_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ReorderRequest) + 'static,
    {
        let trace =
            InteractionTrace::at("Table", "reorder_requested", std::panic::Location::caller());
        self.clear_on_reorder_requested();
        self.reorder_requested_callback = Some(Box::new(ReorderRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self.reorder_requested_callback.as_deref_mut().unwrap()
            as *mut ReorderRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_reorder_requested(
                self.widget.as_raw(),
                Some(run_reorder_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_reorder_requested(&mut self) {
        unsafe {
            sys::oneui_table_set_on_reorder_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.reorder_requested_callback = None;
    }

    /// Assigns stable domain identifiers to the current rows for external drag operations.
    pub fn set_item_drag_ids(&self, ids: &[String]) -> bool {
        let native_ids = ids
            .iter()
            .map(|id| sys::OneUiUtf8String::from_str(id))
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_table_set_item_drag_ids_utf8(
                self.widget.as_raw(),
                native_ids.as_ptr(),
                native_ids.len(),
            ) != 0
        }
    }

    pub fn set_item_drag_enabled(&self, enabled: bool) {
        unsafe { sys::oneui_table_set_item_drag_enabled(self.widget.as_raw(), i32::from(enabled)) };
    }

    pub fn item_drag_enabled(&self) -> bool {
        unsafe { sys::oneui_table_item_drag_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_item_drag<F>(&mut self, callback: F)
    where
        F: FnMut(ItemDragEvent) + 'static,
    {
        let trace = InteractionTrace::at("Table", "item_drag", std::panic::Location::caller());
        self.clear_on_item_drag();
        self.item_drag_callback = Some(Box::new(ItemDragCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data =
            (self.item_drag_callback.as_deref_mut().unwrap() as *mut ItemDragCallback).cast();
        unsafe {
            sys::oneui_table_set_on_item_drag_utf8(
                self.widget.as_raw(),
                Some(run_item_drag_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_item_drag(&mut self) {
        unsafe {
            sys::oneui_table_set_on_item_drag_utf8(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.item_drag_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Table {
    fn drop(&mut self) {
        self.clear_on_item_drag();
        self.clear_on_reorder_requested();
        self.clear_on_context_menu_requested();
        self.clear_on_delete_requested();
        self.clear_on_edit_requested();
        self.clear_on_activated();
        self.clear_on_selection_changed();
        self.clear_on_changed();
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_rows
            .lock()
            .expect("table pending rows lock poisoned")
            .clear();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}

/// Structured tree data. `id` is a stable opaque identifier; an empty
/// `parent_id` denotes a root and every field remains UTF-8 at the ABI edge.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct TreeItem {
    pub id: String,
    pub parent_id: String,
    pub title: String,
    pub detail: String,
    pub expanded: bool,
}

struct TreeViewSelectionCallback {
    handler: Box<dyn FnMut(String) + 'static>,
}

struct TreeViewExpansionCallback {
    handler: Box<dyn FnMut(String, bool) + 'static>,
}

/// A proposed tree reorder addressed by stable application IDs.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TreeReorderRequest {
    pub source_id: String,
    pub target_id: String,
}

struct TreeViewReorderCallback {
    handler: Box<dyn FnMut(TreeReorderRequest) + 'static>,
}

unsafe extern "C" fn run_tree_view_selection_callback(
    text: *const std::ffi::c_char,
    length: usize,
    user_data: *mut std::ffi::c_void,
) {
    if text.is_null() || user_data.is_null() {
        return;
    }
    let bytes = unsafe { std::slice::from_raw_parts(text.cast::<u8>(), length) };
    let id = String::from_utf8_lossy(bytes).into_owned();
    let callback = unsafe { &mut *user_data.cast::<TreeViewSelectionCallback>() };
    run_callback_guarded("tree.selection_changed", || (callback.handler)(id));
}

unsafe extern "C" fn run_tree_view_expansion_callback(
    id: *const std::ffi::c_char,
    length: usize,
    expanded: i32,
    user_data: *mut std::ffi::c_void,
) {
    if id.is_null() || user_data.is_null() {
        return;
    }
    let bytes = unsafe { std::slice::from_raw_parts(id.cast::<u8>(), length) };
    let id = String::from_utf8_lossy(bytes).into_owned();
    let callback = unsafe { &mut *user_data.cast::<TreeViewExpansionCallback>() };
    run_callback_guarded("tree.expansion_changed", || {
        (callback.handler)(id, expanded != 0)
    });
}

unsafe extern "C" fn run_tree_view_reorder_callback(
    source_id: *const std::ffi::c_char,
    source_length: usize,
    target_id: *const std::ffi::c_char,
    target_length: usize,
    user_data: *mut std::ffi::c_void,
) {
    if source_id.is_null() || target_id.is_null() || user_data.is_null() {
        return;
    }
    let source = unsafe { std::slice::from_raw_parts(source_id.cast::<u8>(), source_length) };
    let target = unsafe { std::slice::from_raw_parts(target_id.cast::<u8>(), target_length) };
    let request = TreeReorderRequest {
        source_id: String::from_utf8_lossy(source).into_owned(),
        target_id: String::from_utf8_lossy(target).into_owned(),
    };
    let callback = unsafe { &mut *user_data.cast::<TreeViewReorderCallback>() };
    run_callback_guarded("tree.reorder_requested", || (callback.handler)(request));
}

/// Native hierarchical navigation with ID-based selection and local expansion.
pub struct TreeView {
    widget: Widget,
    selection_callback: Option<Box<TreeViewSelectionCallback>>,
    expansion_callback: Option<Box<TreeViewExpansionCallback>>,
    reorder_callback: Option<Box<TreeViewReorderCallback>>,
}

impl TreeView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_tree_view_create() })?;
        Ok(Self {
            widget,
            selection_callback: None,
            expansion_callback: None,
            reorder_callback: None,
        })
    }

    pub fn set_items(&self, items: &[TreeItem]) {
        let native_items: Vec<sys::OneUiTreeItemUtf8> = items
            .iter()
            .map(|item| sys::OneUiTreeItemUtf8 {
                id: sys::OneUiUtf8String::from_str(&item.id),
                parent_id: sys::OneUiUtf8String::from_str(&item.parent_id),
                title: sys::OneUiUtf8String::from_str(&item.title),
                detail: sys::OneUiUtf8String::from_str(&item.detail),
                expanded: i32::from(item.expanded),
            })
            .collect();
        unsafe {
            sys::oneui_tree_view_set_items_utf8(
                self.widget.as_raw(),
                native_items.as_ptr(),
                native_items.len(),
            )
        };
    }

    pub fn set_selected_id(&self, id: &str) {
        unsafe {
            sys::oneui_tree_view_set_selected_id_utf8(
                self.widget.as_raw(),
                sys::OneUiUtf8String::from_str(id),
            )
        };
    }

    pub fn content_height(&self) -> f32 {
        unsafe { sys::oneui_tree_view_content_height(self.widget.as_raw()) }
    }

    pub fn selected_id(&self) -> String {
        let required = unsafe {
            sys::oneui_tree_view_selected_id_utf8(self.widget.as_raw(), std::ptr::null_mut(), 0)
        };
        if required <= 1 {
            return String::new();
        }
        let mut bytes = vec![0u8; required];
        unsafe {
            sys::oneui_tree_view_selected_id_utf8(
                self.widget.as_raw(),
                bytes.as_mut_ptr().cast(),
                bytes.len(),
            )
        };
        bytes.pop();
        String::from_utf8_lossy(&bytes).into_owned()
    }

    /// Updates the externally dragged item's drop target using client-space coordinates.
    ///
    /// The returned ID is empty when the pointer is outside a visible row. This API only
    /// owns transient presentation state; product code remains responsible for validating
    /// and applying the domain operation when the drag is dropped.
    pub fn update_external_drop_target(&self, x: f32, y: f32) -> String {
        unsafe { sys::oneui_tree_view_update_external_drop_target(self.widget.as_raw(), x, y) };
        self.external_drop_target_id()
    }

    pub fn clear_external_drop_target(&self) {
        unsafe { sys::oneui_tree_view_clear_external_drop_target(self.widget.as_raw()) };
    }

    pub fn external_drop_target_id(&self) -> String {
        let required = unsafe {
            sys::oneui_tree_view_external_drop_target_id_utf8(
                self.widget.as_raw(),
                std::ptr::null_mut(),
                0,
            )
        };
        if required <= 1 {
            return String::new();
        }
        let mut bytes = vec![0u8; required];
        unsafe {
            sys::oneui_tree_view_external_drop_target_id_utf8(
                self.widget.as_raw(),
                bytes.as_mut_ptr().cast(),
                bytes.len(),
            )
        };
        bytes.pop();
        String::from_utf8_lossy(&bytes).into_owned()
    }

    #[track_caller]
    pub fn set_on_selection_changed<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        let trace = InteractionTrace::at(
            "TreeView",
            "selection_changed",
            std::panic::Location::caller(),
        );
        self.clear_selection_callback();
        self.selection_callback = Some(Box::new(TreeViewSelectionCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .selection_callback
            .as_deref_mut()
            .expect("tree view selection callback was just installed")
            as *mut TreeViewSelectionCallback)
            .cast();
        unsafe {
            sys::oneui_tree_view_set_on_selection_changed_utf8(
                self.widget.as_raw(),
                Some(run_tree_view_selection_callback),
                user_data,
            )
        };
    }

    pub fn clear_selection_callback(&mut self) {
        unsafe {
            sys::oneui_tree_view_set_on_selection_changed_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.selection_callback = None;
    }

    #[track_caller]
    pub fn set_on_expansion_changed<F>(&mut self, callback: F)
    where
        F: FnMut(String, bool) + 'static,
    {
        let trace = InteractionTrace::at(
            "TreeView",
            "expansion_changed",
            std::panic::Location::caller(),
        );
        self.clear_expansion_callback();
        self.expansion_callback = Some(Box::new(TreeViewExpansionCallback {
            handler: Box::new(traced_values_callback(trace, callback)),
        }));
        let user_data = (self
            .expansion_callback
            .as_deref_mut()
            .expect("tree view expansion callback was just installed")
            as *mut TreeViewExpansionCallback)
            .cast();
        unsafe {
            sys::oneui_tree_view_set_on_expansion_changed_utf8(
                self.widget.as_raw(),
                Some(run_tree_view_expansion_callback),
                user_data,
            )
        };
    }

    pub fn clear_expansion_callback(&mut self) {
        unsafe {
            sys::oneui_tree_view_set_on_expansion_changed_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.expansion_callback = None;
    }

    /// Enables pointer and `Alt+Up`/`Alt+Down` reorder requests.
    pub fn set_reorder_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_tree_view_set_reorder_enabled(self.widget.as_raw(), i32::from(enabled))
        };
    }

    pub fn reorder_enabled(&self) -> bool {
        unsafe { sys::oneui_tree_view_reorder_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_reorder_requested<F>(&mut self, callback: F)
    where
        F: FnMut(TreeReorderRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "TreeView",
            "reorder_requested",
            std::panic::Location::caller(),
        );
        self.clear_reorder_callback();
        self.reorder_callback = Some(Box::new(TreeViewReorderCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .reorder_callback
            .as_deref_mut()
            .expect("tree view reorder callback was just installed")
            as *mut TreeViewReorderCallback)
            .cast();
        unsafe {
            sys::oneui_tree_view_set_on_reorder_requested_utf8(
                self.widget.as_raw(),
                Some(run_tree_view_reorder_callback),
                user_data,
            )
        };
    }

    pub fn clear_reorder_callback(&mut self) {
        unsafe {
            sys::oneui_tree_view_set_on_reorder_requested_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.reorder_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for TreeView {
    fn drop(&mut self) {
        self.clear_reorder_callback();
        self.clear_selection_callback();
        self.clear_expansion_callback();
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
                ui_thread: std::thread::current().id(),
            }),
            raw_key_callback: None,
            client_size_changed_callback: None,
            _ui_thread: PhantomData,
        })
    }

    pub fn set_title(&self, title: &str) {
        let title = sys::OneUiUtf8String::from_str(title);
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_title_utf8(raw, title);
        });
    }

    pub fn set_borderless(&self, borderless: bool) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_borderless(raw, i32::from(borderless));
        });
    }

    pub fn set_fullscreen(&self, fullscreen: bool) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_fullscreen(raw, i32::from(fullscreen));
        });
    }

    pub fn is_fullscreen(&self) -> bool {
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_is_fullscreen(raw) != 0 })
            .unwrap_or(false)
    }

    pub fn set_minimum_client_size(&self, width: f32, height: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_minimum_client_size(raw, width.max(0.0), height.max(0.0));
        });
    }

    pub fn placement(&self) -> Option<WindowPlacement> {
        self.state.with_raw(|raw| {
            let mut native = sys::OneUiWindowPlacement::default();
            let available = unsafe { sys::oneui_window_get_placement(raw, &mut native) } != 0;
            available.then_some(WindowPlacement {
                x: native.x,
                y: native.y,
                width: native.width,
                height: native.height,
                maximized: native.maximized != 0,
            })
        })?
    }

    pub fn set_placement(&self, placement: WindowPlacement) -> bool {
        let native = sys::OneUiWindowPlacement {
            x: placement.x,
            y: placement.y,
            width: placement.width,
            height: placement.height,
            maximized: i32::from(placement.maximized),
        };
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_set_placement(raw, &native) != 0 })
            .unwrap_or(false)
    }

    pub fn set_title_bar_drag_metrics(&self, title_bar_height: f32, reserved_button_width: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_title_bar_drag_metrics(
                raw,
                title_bar_height,
                reserved_button_width,
            );
        });
    }

    pub fn set_title_bar_interactive_insets(&self, leading_width: f32, trailing_width: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_title_bar_interactive_insets(raw, leading_width, trailing_width);
        });
    }

    pub fn clear_title_bar_interactive_insets(&self) {
        self.set_title_bar_interactive_insets(-1.0, -1.0);
    }

    pub fn set_corner_radius(&self, radius: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_corner_radius(raw, radius);
        });
    }

    pub fn set_content(&self, content: &Widget) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_content(raw, content.as_raw());
        });
    }

    pub fn request_focus(&self, widget: &Widget, focus_visible: bool) -> bool {
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_request_focus(raw, widget.as_raw(), i32::from(focus_visible)) != 0
            })
            .unwrap_or(false)
    }

    /// Installs a window-level key handler that runs before focused widgets.
    /// Returning `true` consumes the event. Text composition remains routed
    /// through the normal IME/text-input path.
    #[track_caller]
    pub fn set_on_raw_key<F>(&mut self, callback: F)
    where
        F: FnMut(RawKeyEvent) -> bool + 'static,
    {
        let trace = InteractionTrace::at("Window", "raw_key", std::panic::Location::caller());
        self.clear_raw_key_callback();
        self.raw_key_callback = Some(Box::new(WindowRawKeyCallback {
            handler: Box::new(traced_value_result_callback(trace, callback)),
        }));
        let user_data = (self
            .raw_key_callback
            .as_deref_mut()
            .expect("window raw key callback was just installed")
            as *mut WindowRawKeyCallback)
            .cast();
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_raw_key(raw, Some(run_window_raw_key_callback), user_data);
        });
    }

    pub fn clear_raw_key_callback(&mut self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_raw_key(raw, None, std::ptr::null_mut());
        });
        self.raw_key_callback = None;
    }

    /// Installs a coalesced logical client-size callback. The callback runs on
    /// the window UI thread after the backend has committed the latest resize
    /// message for the frame.
    pub fn set_on_client_size_changed<F>(&mut self, callback: F)
    where
        F: FnMut(f32, f32) + 'static,
    {
        self.clear_client_size_changed_callback();
        self.client_size_changed_callback = Some(Box::new(WindowClientSizeChangedCallback {
            handler: Box::new(callback),
        }));
        let user_data = (self
            .client_size_changed_callback
            .as_deref_mut()
            .expect("window client-size callback was just installed")
            as *mut WindowClientSizeChangedCallback)
            .cast();
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_client_size_changed(
                raw,
                Some(run_window_client_size_changed_callback),
                user_data,
            );
        });
    }

    pub fn clear_client_size_changed_callback(&mut self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_client_size_changed(raw, None, std::ptr::null_mut());
        });
        self.client_size_changed_callback = None;
    }

    /// Installs the application theme before composing the window tree.
    /// Widgets created afterwards inherit it, and explicit widget styling can
    /// still use [`Widget::apply_style_sheet`] where needed.
    pub fn set_style_sheet(&self, style_sheet: &StyleSheet) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_style_sheet(raw, style_sheet.as_raw());
        });
    }

    /// Sets the font family used by controls that request the standard UI
    /// font. Passing an empty string restores the platform default. This is a
    /// window-wide setting and applies without rebuilding the widget tree.
    pub fn set_default_font_family(&self, family: &str) {
        let family = sys::OneUiUtf8String::from_str(family.trim());
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_default_font_family_utf8(raw, family);
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

    pub fn layout_snapshot_json(&self) -> Result<String, Error> {
        self.dispatcher().layout_snapshot_json()
    }

    pub fn file_dialog(&self, options: FileDialogOptions<'_>) -> Result<Option<PathBuf>, Error> {
        self.dispatcher().file_dialog(options)
    }

    pub fn confirm(&self, title: &str, message: &str) -> Result<bool, Error> {
        self.dispatcher().confirm(title, message)
    }

    pub fn prompt(
        &self,
        title: &str,
        message: &str,
        options: PromptOptions<'_>,
    ) -> Result<Option<String>, Error> {
        self.dispatcher().prompt(title, message, options)
    }

    /// Returns the only thread-safe update path for a terminal mounted in this
    /// window. The terminal itself remains UI-thread bound.
    pub fn terminal_view_handle(&self, terminal: &TerminalView) -> TerminalViewHandle {
        self.dispatcher().terminal_view_handle(terminal)
    }

    /// Returns a thread-safe coalescing producer for decoded remote frames.
    pub fn realtime_frame_view_handle(
        &self,
        frame_view: &RealtimeFrameView,
    ) -> RealtimeFrameViewHandle {
        self.dispatcher().realtime_frame_view_handle(frame_view)
    }

    /// Returns a thread-safe coalescing producer for remote dimensions and
    /// server cursor presentation. The input region remains UI-thread bound.
    pub fn remote_input_region_handle(
        &self,
        input_region: &RemoteInputRegion,
    ) -> RemoteInputRegionHandle {
        self.dispatcher().remote_input_region_handle(input_region)
    }

    /// Returns a thread-safe adaptive-layout handle for a mounted widget.
    pub fn widget_handle(&self, widget: &Widget) -> WidgetHandle {
        self.dispatcher().widget_handle(widget)
    }

    /// Returns the only thread-safe update path for a label mounted in this
    /// window. Bursts are coalesced to the latest text value.
    pub fn label_handle(&self, label: &Label) -> LabelHandle {
        self.dispatcher().label_handle(label)
    }

    /// Returns the thread-safe update path for a mounted single-line text field.
    pub fn text_field_handle(&self, text_field: &TextField) -> TextFieldHandle {
        self.dispatcher().text_field_handle(text_field)
    }

    /// Returns the thread-safe update path for a mounted multiline editor.
    pub fn text_area_handle(&self, text_area: &TextArea) -> TextAreaHandle {
        self.dispatcher().text_area_handle(text_area)
    }

    /// Returns the only thread-safe update path for a mounted progress bar.
    pub fn progress_bar_handle(&self, progress_bar: &ProgressBar) -> ProgressBarHandle {
        self.dispatcher().progress_bar_handle(progress_bar)
    }

    /// Returns the thread-safe update path for a mounted sparkline.
    pub fn sparkline_handle(&self, sparkline: &Sparkline) -> SparklineHandle {
        self.dispatcher().sparkline_handle(sparkline)
    }

    /// Returns the thread-safe update path for a mounted operational chart.
    pub fn time_series_chart_handle(&self, chart: &TimeSeriesChart) -> TimeSeriesChartHandle {
        self.dispatcher().time_series_chart_handle(chart)
    }

    /// Returns the thread-safe update path for rows in a mounted virtual list.
    pub fn virtual_list_handle(&self, list: &VirtualList) -> VirtualListHandle {
        self.dispatcher().virtual_list_handle(list)
    }

    pub fn table_handle(&self, table: &Table) -> TableHandle {
        self.dispatcher().table_handle(table)
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
        self.clear_client_size_changed_callback();
        self.clear_raw_key_callback();
        self.state.destroy();
    }
}

#[cfg(test)]
mod tests {
    use super::sys;
    use super::{
        callback_panic_handler, clear_interaction_trace_handler, emit_interaction_trace,
        run_callback_guarded, run_remote_text_input_callback, run_time_series_inspection_callback,
        run_void_handler, run_window_client_size_changed_callback, run_window_raw_key_callback,
        set_callback_panic_handler, set_interaction_trace_handler,
        should_apply_application_cursor_style, terminal_style, traced_callback,
        traced_value_callback, Button, Color, Dialog, Error, FileDialogFilter, FileDialogMode,
        FileDialogOptions, IconSymbol, Insets, InteractionTrace, InteractiveSurface,
        InteractiveSurfaceStateStyle, InteractiveSurfaceStyle, Label, List, ListItem, LogLine,
        LogView, Menu, OverlayAlignment, OverlayHost, Panel, PixelFormat, Popup,
        PopupInteractionMode, PopupPreferredPlacement, ProgressBar, PromptOptions, RawKeyEvent,
        RealtimeFrameView, RemoteCursorImage, RemoteFrame, RemoteFrameDamage, RemoteFramePatch,
        RemoteInputRegion, RemoteTextInputCallback, ReorderableGrid, ScrollView, SegmentedControl,
        Select, SelectionMode, SplitOrientation, SplitView, Stack, StackDirection, StyleSheet,
        Switch, Table, TableColumn, TableRow, Tabs, TerminalCell, TerminalColor, TerminalCursor,
        TerminalCursorStyle, TerminalFrame, TerminalSelection, TerminalUnderlineStyle,
        TerminalView, TextArea, TextField, TimeSeries, TimeSeriesChart, TimeSeriesChartHandle,
        TimeSeriesInspection, TimeSeriesInspectionCallback, TimeSeriesThreshold, TreeItem,
        TreeView, VirtualList, VirtualListItem, VirtualListRichMetrics, Window,
        WindowClientSizeChangedCallback, WindowOptions, WindowPlacement, WindowRawKeyCallback,
        WindowState, WindowTitleBar,
    };
    use std::cell::{Cell, RefCell};
    use std::ptr::NonNull;
    use std::rc::Rc;
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
    fn application_cursor_style_requires_both_user_consent_and_an_application_frame() {
        assert!(!should_apply_application_cursor_style(false, false));
        assert!(!should_apply_application_cursor_style(false, true));
        assert!(!should_apply_application_cursor_style(true, false));
        assert!(should_apply_application_cursor_style(true, true));
    }

    #[test]
    fn client_size_callback_preserves_logical_dimensions() {
        let observed = Rc::new(RefCell::new(None));
        let observed_from_callback = Rc::clone(&observed);
        let mut callback = WindowClientSizeChangedCallback {
            handler: Box::new(move |width, height| {
                *observed_from_callback.borrow_mut() = Some((width, height));
            }),
        };

        unsafe {
            run_window_client_size_changed_callback(
                1024.5,
                720.25,
                (&mut callback as *mut WindowClientSizeChangedCallback).cast(),
            );
        }

        assert_eq!(*observed.borrow(), Some((1024.5, 720.25)));
    }

    #[test]
    fn time_series_inspection_callback_preserves_index_and_pin_state() {
        let observed = Rc::new(Cell::new(None));
        let observed_from_callback = Rc::clone(&observed);
        let mut callback = TimeSeriesInspectionCallback {
            handler: Box::new(move |event| observed_from_callback.set(Some(event))),
        };

        unsafe {
            run_time_series_inspection_callback(
                17,
                1,
                (&mut callback as *mut TimeSeriesInspectionCallback).cast(),
            );
        }

        assert_eq!(
            observed.get(),
            Some(TimeSeriesInspection {
                index: Some(17),
                pinned: true,
            })
        );
    }

    #[test]
    fn time_series_chart_safe_binding_keeps_gaps_thresholds_and_handle_lifetime_safe() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let chart = TimeSeriesChart::new().expect("time-series chart should be created");
        chart.set_range(0.0, 100.0);
        chart.set_grid_lines(4);
        chart.set_visual_style(false, true, true, false, 1.0, 18);
        chart.set_plot_insets(Insets {
            top: 4.0,
            right: 4.0,
            bottom: 4.0,
            left: 4.0,
        });
        chart.set_thresholds(&[TimeSeriesThreshold {
            value: 80.0,
            color: Color::rgba(245, 158, 11, 128),
        }]);
        chart.set_series(&[TimeSeries {
            name: "CPU".to_string(),
            color: Color::rgb(77, 163, 255),
            values: vec![Some(20.0), None, Some(42.0)],
        }]);
        chart.set_inspection(TimeSeriesInspection {
            index: Some(2),
            pinned: true,
        });
        assert_eq!(
            chart.inspection(),
            TimeSeriesInspection {
                index: Some(2),
                pinned: true,
            }
        );

        let handle: TimeSeriesChartHandle = window.time_series_chart_handle(&chart);
        handle
            .set_series(vec![TimeSeries {
                name: "内存".to_string(),
                color: Color::rgb(161, 111, 255),
                values: vec![Some(35.0), Some(36.0)],
            }])
            .expect("mounted chart handle should accept a coalesced update");
        window.set_content(chart.as_widget());
        drop(chart);
        assert_eq!(handle.set_series(Vec::new()), Err(Error::WidgetDestroyed));
        window.close();
    }

    #[test]
    fn remote_text_input_callback_preserves_committed_utf8() {
        let observed = Rc::new(RefCell::new(String::new()));
        let observed_from_callback = Rc::clone(&observed);
        let mut callback = RemoteTextInputCallback {
            handler: Box::new(move |value| *observed_from_callback.borrow_mut() = value),
        };
        let value = "中文输入";

        unsafe {
            run_remote_text_input_callback(
                value.as_ptr().cast(),
                value.len(),
                (&mut callback as *mut RemoteTextInputCallback).cast(),
            );
        }

        assert_eq!(observed.borrow().as_str(), value);
    }

    #[test]
    fn window_default_font_can_be_changed_through_window_and_dispatcher() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        window.set_default_font_family("Segoe UI");
        window
            .dispatcher()
            .set_default_font_family("Microsoft YaHei UI")
            .expect("UI-thread font update should succeed");
        window.set_default_font_family("");
        window.close();
    }

    #[test]
    fn reports_panics_caught_at_callback_boundaries() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let reports = Arc::new(Mutex::new(Vec::new()));
        let reports_for_handler = Arc::clone(&reports);
        set_callback_panic_handler(move |report| {
            reports_for_handler
                .lock()
                .expect("callback panic reports lock")
                .push(report);
        });

        let result = run_callback_guarded("test.callback", || panic!("callback failed"));

        assert!(result.is_none());
        let reports = reports.lock().expect("callback panic reports lock");
        assert_eq!(reports.len(), 1);
        assert_eq!(reports[0].context, "test.callback");
        assert_eq!(reports[0].message, "callback failed");
        drop(reports);
        *callback_panic_handler()
            .lock()
            .expect("callback panic handler lock") = None;
    }

    #[test]
    fn interaction_trace_observer_reports_callbacks_without_changing_behavior() {
        let _guard = window_test_lock().lock().expect("window test lock");
        clear_interaction_trace_handler();
        let reports = Arc::new(Mutex::new(Vec::new()));
        let reports_for_handler = Arc::clone(&reports);
        set_interaction_trace_handler(move |trace| {
            reports_for_handler
                .lock()
                .expect("interaction trace reports lock")
                .push(trace);
        });

        let invocations = Rc::new(Cell::new(0));
        let invocations_for_callback = Rc::clone(&invocations);
        let trace = InteractionTrace {
            control: "Button",
            interaction: "click",
            source_file: "tests/interaction.rs",
            source_line: 17,
            source_column: 9,
        };
        let mut callback = traced_callback(trace, move || {
            invocations_for_callback.set(invocations_for_callback.get() + 1);
        });

        callback();
        clear_interaction_trace_handler();
        callback();

        assert_eq!(invocations.get(), 2);
        let reports = reports.lock().expect("interaction trace reports lock");
        assert_eq!(reports.as_slice(), &[trace]);
    }

    #[test]
    fn application_interaction_trace_uses_the_same_isolated_observer() {
        let _guard = window_test_lock().lock().expect("window test lock");
        clear_interaction_trace_handler();
        let reports = Arc::new(Mutex::new(Vec::new()));
        let reports_for_handler = Arc::clone(&reports);
        set_interaction_trace_handler(move |trace| {
            reports_for_handler
                .lock()
                .expect("application interaction trace reports lock")
                .push(trace);
        });
        let trace = InteractionTrace {
            control: "NativeSingleInstance",
            interaction: "arguments",
            source_file: "tests/application_interaction.rs",
            source_line: 41,
            source_column: 5,
        };

        emit_interaction_trace(trace);
        clear_interaction_trace_handler();
        emit_interaction_trace(trace);

        let reports = reports.lock().expect("application trace reports lock");
        assert_eq!(reports.as_slice(), &[trace]);
    }

    #[test]
    fn interaction_trace_observer_panics_are_isolated_from_value_callbacks() {
        let _guard = window_test_lock().lock().expect("window test lock");
        clear_interaction_trace_handler();
        set_interaction_trace_handler(|_| panic!("trace observer failed"));
        let observed = Rc::new(Cell::new(None));
        let observed_for_callback = Rc::clone(&observed);
        let trace = InteractionTrace {
            control: "Tabs",
            interaction: "changed",
            source_file: "tests/interaction.rs",
            source_line: 33,
            source_column: 7,
        };
        let mut callback = traced_value_callback(trace, move |value| {
            observed_for_callback.set(Some(value));
        });

        callback(4_i32);
        clear_interaction_trace_handler();

        assert_eq!(observed.get(), Some(4));
    }

    #[test]
    fn nested_void_command_is_ignored_instead_of_panicking() {
        let invocations = Rc::new(Cell::new(0));
        let callback_slot = Rc::new(RefCell::new(
            None::<Rc<RefCell<Box<dyn FnMut() + 'static>>>>,
        ));
        let slot_for_callback = Rc::clone(&callback_slot);
        let invocations_for_callback = Rc::clone(&invocations);
        let handler: Rc<RefCell<Box<dyn FnMut() + 'static>>> =
            Rc::new(RefCell::new(Box::new(move || {
                invocations_for_callback.set(invocations_for_callback.get() + 1);
                let nested = slot_for_callback
                    .borrow()
                    .as_ref()
                    .expect("handler should be installed")
                    .clone();
                run_void_handler("test.nested_command", &nested);
            })));
        *callback_slot.borrow_mut() = Some(Rc::clone(&handler));

        run_void_handler("test.command", &handler);

        assert_eq!(invocations.get(), 1);
    }

    #[test]
    fn window_raw_key_callback_reports_consumption_and_preserves_event_data() {
        let observed = Rc::new(Cell::new(None));
        let observed_from_callback = Rc::clone(&observed);
        let mut callback = Box::new(WindowRawKeyCallback {
            handler: Box::new(move |event| {
                observed_from_callback.set(Some(event));
                event.pressed && event.ctrl && event.virtual_key == 0x4B
            }),
        });
        let event = sys::OneUiRawKeyEvent {
            virtual_key: 0x4B,
            scan_code: 0x25,
            pressed: 1,
            repeat: 0,
            extended: 0,
            alt: 0,
            ctrl: 1,
            shift: 0,
            win: 0,
        };

        let consumed = unsafe {
            run_window_raw_key_callback(
                &event,
                (&mut *callback as *mut WindowRawKeyCallback).cast(),
            )
        };

        assert_eq!(consumed, 1);
        assert_eq!(
            observed.get(),
            Some(RawKeyEvent {
                virtual_key: 0x4B,
                scan_code: 0x25,
                pressed: true,
                repeat: false,
                extended: false,
                alt: false,
                ctrl: true,
                shift: false,
                win: false,
            })
        );
    }

    #[test]
    fn window_raw_key_callback_does_not_consume_null_or_panicking_handlers() {
        assert_eq!(
            unsafe { run_window_raw_key_callback(std::ptr::null(), std::ptr::null_mut()) },
            0
        );

        let _guard = window_test_lock().lock().expect("window test lock");
        let mut callback = Box::new(WindowRawKeyCallback {
            handler: Box::new(|_| panic!("window shortcut failed")),
        });
        let event = sys::OneUiRawKeyEvent::default();
        let consumed = unsafe {
            run_window_raw_key_callback(
                &event,
                (&mut *callback as *mut WindowRawKeyCallback).cast(),
            )
        };
        assert_eq!(consumed, 0);
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
    fn runtime_fullscreen_and_minimum_client_size_round_trip_through_v21() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        window.set_minimum_client_size(510.0, 700.0);
        assert!(!window.is_fullscreen());
        window.set_fullscreen(true);
        assert!(window.is_fullscreen());
        window.set_fullscreen(false);
        assert!(!window.is_fullscreen());
    }

    #[test]
    fn file_dialog_options_have_safe_platform_defaults() {
        let open = FileDialogOptions::open("Open recording");
        assert_eq!(open.mode, FileDialogMode::OpenFile);
        assert!(open.initial_directory.as_os_str().is_empty());
        assert!(open.default_name.is_empty());
        assert!(open.confirm_overwrite);

        let filters = [FileDialogFilter {
            name: "Terminal recordings",
            pattern: "*.cast",
        }];
        let save = FileDialogOptions {
            filters: &filters,
            default_extension: "cast",
            ..FileDialogOptions::save("Export recording")
        };
        assert_eq!(save.mode, FileDialogMode::SaveFile);
        assert_eq!(save.filters[0].pattern, "*.cast");

        let folder = FileDialogOptions::select_folder("Choose folder");
        assert_eq!(folder.mode, FileDialogMode::SelectFolder);
    }

    #[test]
    fn round_trips_window_placement_through_the_safe_binding() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions {
            borderless: true,
            ..WindowOptions::default()
        })
        .expect("window should be created");
        let requested = WindowPlacement {
            x: 120,
            y: 140,
            width: 720,
            height: 520,
            maximized: true,
        };

        assert!(window.set_placement(requested));
        let actual = window.placement().expect("placement should be available");
        assert_eq!(actual.width, requested.width);
        assert_eq!(actual.height, requested.height);
        assert!(actual.maximized);
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
    fn configures_resizable_split_view_through_safe_binding() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let mut split =
            SplitView::new(SplitOrientation::Horizontal).expect("split view should be created");
        let first = Panel::new().expect("first panel should be created");
        let second = Panel::new().expect("second panel should be created");
        split.set_first(first.as_widget());
        split.set_second(second.as_widget());
        split.set_gap(6.0);
        split.set_padding(Insets {
            top: 1.0,
            right: 2.0,
            bottom: 3.0,
            left: 4.0,
        });
        split.set_minimum_pane_extent(120.0, 160.0);
        split.set_resizable(true);
        split.set_ratio(0.625);
        split.set_orientation(SplitOrientation::Vertical);
        split.set_on_ratio_changed(|_| {});
        split.set_on_ratio_committed(|_| {});

        assert!((split.ratio() - 0.625).abs() < 0.001);
        split.clear_on_ratio_changed();
        split.clear_on_ratio_committed();
    }

    #[test]
    fn applies_css_theme_to_semantic_widget_nodes() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let theme = StyleSheet::from_css(
            ":root { --surface: #1e1e2e; --text: #dcdeec; }\n\
             section.workspace { background: var(--surface); }\n\
             label.page-title { color: var(--text); font-size: 18px; font-weight: 600; }",
        )
        .expect("CSS should parse");
        window.set_style_sheet(&theme);

        let root = Panel::new().expect("panel should be created");
        root.as_widget()
            .set_style_node("section", "workspace")
            .expect("classes should be valid");
        let title = Label::new("Host management").expect("label should be created");
        title
            .as_widget()
            .set_classes("page-title")
            .expect("classes should be valid");
        let content = Stack::new(StackDirection::Column).expect("stack should be created");
        content.add(title.as_widget());
        root.set_content(content.as_widget());
        window.set_content(root.as_widget());
    }

    #[test]
    fn mounts_a_styled_interactive_surface_with_native_hover_states() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let surface = InteractiveSurface::new().expect("surface should be created");
        surface.set_style(InteractiveSurfaceStyle {
            normal: InteractiveSurfaceStateStyle::solid(
                Color::rgb(33, 35, 50),
                Color::rgb(57, 60, 79),
                10.0,
            ),
            hovered: InteractiveSurfaceStateStyle::solid(
                Color::rgb(48, 51, 70),
                Color::rgb(80, 84, 113),
                10.0,
            ),
            pressed: InteractiveSurfaceStateStyle::solid(
                Color::rgb(40, 42, 58),
                Color::rgb(101, 88, 241),
                10.0,
            ),
            disabled: InteractiveSurfaceStateStyle::solid(
                Color::rgb(33, 35, 50),
                Color::rgb(57, 60, 79),
                10.0,
            ),
            focus_visible: InteractiveSurfaceStateStyle::solid(
                Color::rgb(33, 35, 50),
                Color::rgb(101, 88, 241),
                10.0,
            ),
        });
        surface.set_padding(Insets {
            top: 12.0,
            right: 12.0,
            bottom: 12.0,
            left: 12.0,
        });
        let label = Label::new("Native card").expect("label should be created");
        surface.set_content(label.as_widget());
        window.set_content(surface.as_widget());
    }

    #[test]
    fn configures_a_native_reorderable_grid_without_product_geometry() {
        let mut grid = ReorderableGrid::new().expect("grid should be created");
        grid.set_column_count(2);
        grid.set_gaps(12.0, 8.0);
        grid.set_item_height(40.0);
        grid.set_reorder_enabled(true);
        grid.set_item_drag_enabled(true);
        grid.set_on_item_drag(|_| {});

        let first = Panel::new().expect("first panel should be created");
        let second = Panel::new().expect("second panel should be created");
        let third = Panel::new().expect("third panel should be created");
        grid.add_item("alpha", first.as_widget());
        grid.add_item("beta", second.as_widget());
        grid.add_item("gamma", third.as_widget());

        assert!(grid.reorder_enabled());
        assert!(grid.item_drag_enabled());
        assert!((grid.content_height() - 88.0).abs() < 0.001);
        assert!(grid.move_item("alpha", 2));
        assert!(!grid.move_item("missing", 0));
        grid.clear_items();
        assert!(grid.content_height().abs() < 0.001);
    }

    #[test]
    fn reports_stack_content_extent_for_scroll_view_composition() {
        let stack = Stack::new(StackDirection::Column).expect("stack should be created");
        stack.set_gap(7.0);
        stack.set_padding(Insets {
            top: 2.0,
            right: 3.0,
            bottom: 4.0,
            left: 5.0,
        });
        let first = Panel::new().expect("first panel should be created");
        first.as_widget().set_preferred_size(140.0, 19.0);
        let second = Panel::new().expect("second panel should be created");
        second.as_widget().set_preferred_size(80.0, 32.0);
        let third = Panel::new().expect("third panel should be created");
        third.as_widget().set_preferred_size(120.0, 28.0);
        stack.add(first.as_widget());
        stack.add(second.as_widget());
        stack.add(third.as_widget());

        assert!((stack.content_width() - 148.0).abs() < 0.001);
        assert!((stack.content_height() - 99.0).abs() < 0.001);
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
    fn retains_text_field_change_callbacks_through_programmatic_updates() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let observed = Arc::new(Mutex::new(Vec::new()));
        let mut field = TextField::new("搜索主机").expect("text field should be created");
        field.set_password_mode(true);
        field.set_password_mask('●');
        let observed_for_callback = Arc::clone(&observed);
        field.set_on_changed(move |value| {
            observed_for_callback
                .lock()
                .expect("observed values lock")
                .push(value);
        });

        field.set_text("生产堡垒机");
        assert_eq!(
            observed.lock().expect("observed values lock").as_slice(),
            ["生产堡垒机"]
        );

        let long_value =
            "rename 5a9c8e06-0977-4426-8e3f-b8518f6c6500 QA Snippet Group Renamed 中文值";
        field.set_text(long_value);
        assert_eq!(
            observed.lock().expect("observed values lock").as_slice(),
            ["生产堡垒机", long_value]
        );
    }

    #[test]
    fn mounts_stateful_native_setting_controls() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let content = Stack::new(StackDirection::Column).expect("stack should be created");
        let mut segmented =
            SegmentedControl::new(&["卡片视图".to_string(), "列表视图".to_string()])
                .expect("segmented control should be created");
        segmented.set_selected_index(1);
        assert_eq!(segmented.selected_index(), 1);
        segmented.set_on_changed(|_| {});

        let mut switch = Switch::new("启用 Docker 管理").expect("switch should be created");
        switch.set_checked(true);
        assert!(switch.checked());
        switch.set_on_changed(|_| {});

        content.add(segmented.as_widget());
        content.add(switch.as_widget());
        window.set_content(content.as_widget());
    }

    #[test]
    fn mounts_utf8_workspace_tabs_and_reports_selection_changes() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let observed = std::rc::Rc::new(std::cell::RefCell::new(None));
        let mut tabs = Tabs::new(&["生产堡垒机".to_string(), "Kylin V10 🚀".to_string()])
            .expect("tabs should be created");
        let observed_for_callback = std::rc::Rc::clone(&observed);
        tabs.set_on_changed(move |index| *observed_for_callback.borrow_mut() = Some(index));
        tabs.set_selected_index(1);

        assert_eq!(tabs.selected_index(), 1);
        assert_eq!(*observed.borrow(), Some(1));
        window.set_content(tabs.as_widget());
    }

    #[test]
    fn mounts_utf8_select_and_reports_selection_changes() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let observed = std::rc::Rc::new(std::cell::RefCell::new(None));
        let mut select = Select::new(&[
            "手动排序".to_string(),
            "按名称".to_string(),
            "最近使用".to_string(),
        ])
        .expect("select should be created");
        let observed_for_callback = std::rc::Rc::clone(&observed);
        select.set_on_changed(move |index| *observed_for_callback.borrow_mut() = Some(index));
        select.set_selected_index(2);

        assert_eq!(select.selected_index(), 2);
        assert_eq!(*observed.borrow(), Some(2));
        window.set_content(select.as_widget());
    }

    #[test]
    fn reports_native_list_selection_changes_to_rust() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let selected = std::rc::Rc::new(std::cell::RefCell::new(None));
        let mut list = List::new().expect("list should be created");
        list.set_items(&[
            ListItem {
                title: "Production".to_owned(),
                detail: "10.0.0.1".to_owned(),
            },
            ListItem {
                title: "Staging".to_owned(),
                detail: "10.0.0.2".to_owned(),
            },
        ]);
        let observed = std::rc::Rc::clone(&selected);
        list.set_on_changed(move |index| *observed.borrow_mut() = Some(index));
        list.set_selected_index(1);

        assert_eq!(*selected.borrow(), Some(1));
        window.set_content(list.as_widget());
    }

    #[test]
    fn supports_an_explicit_unselected_list_state() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let list = List::new().expect("list should be created");
        list.set_items(&[
            ListItem {
                title: "Production".to_owned(),
                detail: "10.0.0.1".to_owned(),
            },
            ListItem {
                title: "Staging".to_owned(),
                detail: "10.0.0.2".to_owned(),
            },
        ]);
        list.set_selection_required(false);
        list.set_selected_index(-1);

        assert_eq!(list.selected_index(), -1);
        window.set_content(list.as_widget());
    }

    #[test]
    fn mounts_virtual_list_with_large_structured_data_without_widget_per_row() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let mut list = VirtualList::new().expect("virtual list should be created");
        let items: Vec<ListItem> = (0..5_000)
            .map(|index| ListItem {
                title: format!("Host {index}"),
                detail: format!("10.0.{}.{}", index / 255, index % 255),
            })
            .collect();
        list.set_items(&items);
        let drag_ids: Vec<String> = (0..5_000).map(|index| format!("host-{index}")).collect();
        assert!(list.set_item_drag_ids(&drag_ids));
        assert!(!list.set_item_drag_ids(&drag_ids[..4_999]));
        let mut duplicate_drag_ids = drag_ids.clone();
        duplicate_drag_ids[4_999] = duplicate_drag_ids[0].clone();
        assert!(!list.set_item_drag_ids(&duplicate_drag_ids));
        list.set_item_drag_enabled(true);
        assert!(list.item_drag_enabled());
        list.set_on_item_drag(|_| {});
        list.set_row_height(44.0);
        list.set_selected_index(4_999);
        assert_eq!(list.selected_index(), 4_999);
        assert!(list.max_scroll_offset() >= list.scroll_offset());

        let observed = std::rc::Rc::new(std::cell::RefCell::new(None));
        let callback_observed = std::rc::Rc::clone(&observed);
        list.set_on_changed(move |index| *callback_observed.borrow_mut() = Some(index));
        list.set_selected_index(12);
        assert_eq!(*observed.borrow(), Some(12));

        let selected_sets = std::rc::Rc::new(std::cell::RefCell::new(Vec::new()));
        let selected_sets_for_callback = std::rc::Rc::clone(&selected_sets);
        list.set_on_selection_changed(move |indices| {
            *selected_sets_for_callback.borrow_mut() = indices;
        });
        list.set_selection_mode(SelectionMode::Multiple);
        list.set_selected_indices(&[2, 5, 9]);
        assert_eq!(list.selected_indices(), vec![2, 5, 9]);
        assert_eq!(*selected_sets.borrow(), vec![2, 5, 9]);
        list.set_on_activated(|_| {});
        list.set_on_edit_requested(|_| {});
        list.set_on_context_menu_requested(|_| {});
        window.set_content(list.as_widget());
    }

    #[test]
    fn mounts_rich_virtual_list_rows_and_title_bar_leading_content() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let list = VirtualList::new().expect("virtual list should be created");
        let row = VirtualListItem {
            title: "ERP management".to_owned(),
            detail: "erp-demo.wangyunchuan.cn".to_owned(),
            badge: "HTTP".to_owned(),
            trailing: "Running".to_owned(),
            indicator_color: Some(Color::rgb(34, 197, 94)),
            trailing_color: Some(Color::rgb(22, 163, 74)),
        };
        list.set_rich_items(std::slice::from_ref(&row));
        assert!(list.update_rich_item(0, &row));
        let metrics = VirtualListRichMetrics {
            trailing_width: 54.0,
            ..VirtualListRichMetrics::default()
        };
        list.set_rich_metrics(metrics);
        assert!((list.rich_metrics().trailing_width - 54.0).abs() < 0.001);

        let title_bar = WindowTitleBar::new("Workspace").expect("title bar should be created");
        let leading = Panel::new().expect("leading panel should be created");
        title_bar.set_leading(leading.as_widget());
        title_bar.set_icon(IconSymbol::Folder);
        window.set_content(list.as_widget());
    }

    #[test]
    fn mounts_multiline_log_view_with_structured_colored_lines() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let log = LogView::new().expect("log view should be created");
        log.set_font_size(13.0);
        log.set_line_height(21.0);
        log.set_lines(&[
            LogLine {
                text: "ops@node:~$ uptime".to_owned(),
                color: Color::rgb(220, 226, 240),
            },
            LogLine {
                text: "load average: 0.12, 0.08, 0.05".to_owned(),
                color: Color::rgb(116, 218, 156),
            },
        ]);
        assert!(log.content_height() >= 42.0);
        log.clear();
        log.append_line(&LogLine {
            text: "fresh line".to_owned(),
            color: Color::rgb(132, 145, 255),
        });
        window.set_content(log.as_widget());
    }

    #[test]
    fn mounts_and_removes_a_modal_dialog_from_an_overlay_host() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let overlay = OverlayHost::new().expect("overlay host should be created");
        let page = Panel::new().expect("page should be created");
        overlay.set_content(page.as_widget());

        let mut dialog =
            Dialog::new("新建主机", "仅用于原生界面交互演示").expect("dialog should be created");
        dialog.set_icon(IconSymbol::Server);
        let dialog_body = Label::new("表单内容由产品层组合，弹层能力由 OneUI 统一提供。")
            .expect("dialog body should be created");
        dialog.set_content(dialog_body.as_widget());
        let closed = Arc::new(AtomicBool::new(false));
        let closed_for_callback = Arc::clone(&closed);
        dialog.set_on_close(move || closed_for_callback.store(true, Ordering::SeqCst));
        overlay.add_modal_anchored_overlay(
            dialog.as_widget(),
            10,
            440.0,
            240.0,
            Insets::default(),
            OverlayAlignment::Center,
            OverlayAlignment::Center,
        );
        window.set_content(overlay.as_widget());
        assert!(overlay.remove_overlay(dialog.as_widget()));
        assert!(!closed.load(Ordering::SeqCst));
    }

    #[test]
    fn mounts_a_light_dismiss_native_context_menu() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let overlay = OverlayHost::new().expect("overlay host should be created");
        let page = Panel::new().expect("page should be created");
        overlay.set_content(page.as_widget());

        let anchor = Panel::new().expect("popup anchor should be created");
        anchor.as_widget().set_preferred_size(1.0, 1.0);
        let mut menu = Menu::new().expect("menu should be created");
        menu.add_header("Production", "root@10.0.0.1:22");
        assert_eq!(
            menu.add_item("Connect", Some(IconSymbol::Terminal), false),
            0
        );
        menu.add_separator();
        assert_eq!(menu.add_item("Delete", Some(IconSymbol::Trash), true), 1);
        menu.set_on_activated(|_| {});
        menu.as_widget()
            .set_preferred_size(220.0, menu.preferred_height());

        let popup = Popup::new().expect("popup should be created");
        popup.set_anchor(anchor.as_widget());
        popup.set_content(menu.as_widget());
        popup.set_anchor_rect(40.0, 50.0, 1.0, 1.0);
        popup.set_preferred_placement(PopupPreferredPlacement::BottomStart);
        popup.set_interaction_mode(PopupInteractionMode::LightDismiss);
        popup.set_open(true);
        assert!(popup.is_open());

        overlay.add_anchored_overlay(
            popup.as_widget(),
            100,
            -1.0,
            -1.0,
            Insets::default(),
            OverlayAlignment::Start,
            OverlayAlignment::Start,
        );
        window.set_content(overlay.as_widget());
        assert!(overlay.remove_overlay(popup.as_widget()));
    }

    #[test]
    fn mounts_native_tree_with_structured_id_based_items() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let tree = TreeView::new().expect("tree view should be created");
        tree.set_items(&[
            TreeItem {
                id: "platform".to_owned(),
                title: "Platform".to_owned(),
                detail: "12".to_owned(),
                expanded: true,
                ..TreeItem::default()
            },
            TreeItem {
                id: "production".to_owned(),
                parent_id: "platform".to_owned(),
                title: "Production".to_owned(),
                detail: "8".to_owned(),
                expanded: true,
            },
        ]);
        tree.set_selected_id("production");
        assert_eq!(tree.selected_id(), "production");
        assert_eq!(tree.update_external_drop_target(-1.0, -1.0), "");
        assert_eq!(tree.external_drop_target_id(), "");
        tree.clear_external_drop_target();
        assert_eq!(tree.content_height(), 64.0);
        tree.as_widget().set_preferred_size(240.0, 0.0);
        window.set_content(tree.as_widget());
    }

    #[test]
    fn terminal_dirty_ranges_preserve_sparse_updates() {
        let mut previous = TerminalFrame {
            rows: 2,
            columns: 3,
            cells: vec![TerminalCell::default(); 6],
            cursor: TerminalCursor {
                row: 0,
                column: 0,
                visible: true,
            },
            cursor_style: TerminalCursorStyle::Block,
            cursor_blinking: true,
            cursor_style_from_application: false,
            mouse_reporting: false,
            first_visible_line_number: 1,
        };
        let mut current = previous.clone();
        assert_eq!(
            super::terminal_dirty_ranges(&previous, &current),
            Vec::new()
        );

        current.cells[4].text = "X".to_owned();
        assert_eq!(
            super::terminal_dirty_ranges(&previous, &current),
            vec![4..5]
        );

        previous.cells[1].text = "before".to_owned();
        current.cells[1].text = "after".to_owned();
        assert_eq!(
            super::terminal_dirty_ranges(&previous, &current),
            vec![1..2, 4..5]
        );

        current.cells[0].hyperlink_id = 42;
        assert_eq!(
            super::terminal_dirty_ranges(&previous, &current),
            vec![0..2, 4..5]
        );
    }

    #[test]
    fn terminal_cells_preserve_extended_attributes_across_the_c_abi() {
        let native = super::native_terminal_cells(&[TerminalCell {
            text: "docs".to_owned(),
            hyperlink_id: 73,
            underline_style: TerminalUnderlineStyle::Curly,
            underline_color: Some(TerminalColor::rgb(12, 34, 56)),
            ..TerminalCell::default()
        }]);
        assert_eq!(native.len(), 1);
        assert_eq!(native[0].hyperlink_id, 73);
        assert_eq!(
            native[0].underline_style,
            TerminalUnderlineStyle::Curly as u32
        );
        assert_eq!(native[0].underline_color.r, 12);
        assert_eq!(native[0].underline_color.g, 34);
        assert_eq!(native[0].underline_color.b, 56);
        assert_eq!(native[0].underline_color_set, 1);
    }

    #[test]
    fn terminal_dirty_ranges_collapse_pathological_fragmentation() {
        let previous = TerminalFrame {
            rows: 1,
            columns: 80,
            cells: vec![TerminalCell::default(); 80],
            cursor: TerminalCursor {
                row: 0,
                column: 0,
                visible: true,
            },
            cursor_style: TerminalCursorStyle::Block,
            cursor_blinking: true,
            cursor_style_from_application: false,
            mouse_reporting: false,
            first_visible_line_number: 1,
        };
        let mut current = previous.clone();
        for index in (0..80).step_by(2) {
            current.cells[index].text = "X".to_owned();
        }

        assert_eq!(
            super::terminal_dirty_ranges(&previous, &current),
            vec![0..79]
        );
    }

    #[test]
    fn mounts_terminal_grid_with_wide_cells_and_cursor() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let terminal = TerminalView::new().expect("terminal should be created");
        terminal.set_font_size(13.0);
        terminal.set_font_family("Cascadia Mono");
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
        terminal.select_all();
        assert!(terminal.has_selection());
        assert!(terminal.copy_selection());
        assert_eq!(terminal.selected_text(), "A宽\r\n");
        terminal.set_selection(TerminalSelection {
            start_row: 0,
            start_column: 0,
            end_row: 0,
            end_column: 1,
        });
        assert_eq!(terminal.selected_text(), "A");
        terminal.clear_selection();
        assert!(!terminal.has_selection());
        window.set_content(terminal.as_widget());
    }

    #[test]
    fn exposes_terminal_caret_geometry_for_native_overlays() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let terminal = TerminalView::new().unwrap();
        terminal.set_grid(2, 4, &vec![TerminalCell::default(); 8]);
        terminal.set_cursor(TerminalCursor {
            row: 1,
            column: 3,
            visible: true,
        });
        window.set_content(terminal.as_widget());

        let caret = terminal.text_input_caret_rect().expect("caret rectangle");
        assert!(caret.x >= 3.0);
        assert!(caret.y >= 1.0);
        assert!(caret.width >= 1.0);
        assert!(caret.height >= 1.0);
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
                    cursor_style: TerminalCursorStyle::Bar,
                    cursor_blinking: false,
                    cursor_style_from_application: true,
                    mouse_reporting: true,
                    first_visible_line_number: 1,
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
                cursor_style: TerminalCursorStyle::Block,
                cursor_blinking: true,
                cursor_style_from_application: false,
                mouse_reporting: false,
                first_visible_line_number: 1,
            }),
            Err(Error::WidgetDestroyed)
        ));
    }

    #[test]
    fn remote_frame_normalizes_stride_and_rejects_short_buffers() {
        let frame = RemoteFrame::new(vec![0_u8; 16], 2, 2, 0, PixelFormat::Bgra8888, 7, 700)
            .expect("tightly packed remote frame should be valid");
        assert_eq!(frame.stride, 8);

        assert!(matches!(
            RemoteFrame::new(vec![0_u8; 15], 2, 2, 8, PixelFormat::Rgba8888, 8, 800,),
            Err(Error::InvalidVideoFrame { .. })
        ));
        assert!(matches!(
            RemoteFrame::new(vec![0_u8; 16], 2, 2, 7, PixelFormat::Bgra8888, 9, 900,),
            Err(Error::InvalidVideoFrame { .. })
        ));
    }

    #[test]
    fn remote_frame_damage_normalizes_patches_and_rejects_invalid_batches() {
        let damage = RemoteFrameDamage::new(
            2,
            2,
            PixelFormat::Bgra8888,
            vec![RemoteFramePatch::new(vec![1_u8; 4], 1, 1, 1, 1, 0)],
            8,
            800,
        )
        .expect("valid damage should be accepted");
        assert_eq!(damage.patches[0].stride, 4);

        assert!(matches!(
            RemoteFrameDamage::new(
                2,
                2,
                PixelFormat::Bgra8888,
                vec![RemoteFramePatch::new(vec![1_u8; 4], 2, 1, 1, 1, 4)],
                9,
                900,
            ),
            Err(Error::InvalidVideoFrame { .. })
        ));
        assert!(matches!(
            RemoteFrameDamage::new(2, 2, PixelFormat::Bgra8888, Vec::new(), 10, 1000),
            Err(Error::InvalidVideoFrame { .. })
        ));
    }

    #[test]
    fn remote_cursor_image_normalizes_stride_and_rejects_invalid_metadata() {
        let image = RemoteCursorImage::new(vec![0_u8; 16], 2, 2, 0, 1, 1)
            .expect("tightly packed cursor should be valid");
        assert_eq!(image.stride, 8);

        assert!(matches!(
            RemoteCursorImage::new(vec![0_u8; 15], 2, 2, 8, 1, 1),
            Err(Error::InvalidRemoteCursor { .. })
        ));
        assert!(matches!(
            RemoteCursorImage::new(vec![0_u8; 16], 2, 2, 8, 2, 1),
            Err(Error::InvalidRemoteCursor { .. })
        ));
        assert!(matches!(
            RemoteCursorImage::new(vec![0_u8; 513 * 4], 513, 1, 0, 0, 0),
            Err(Error::InvalidRemoteCursor { .. })
        ));
    }

    #[test]
    fn remote_input_handle_submits_worker_cursor_updates_on_the_window_thread() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let input = RemoteInputRegion::new().expect("remote input should be created");
        window.set_content(input.as_widget());
        let handle = window.remote_input_region_handle(&input);
        let worker = thread::spawn(move || {
            handle
                .set_remote_size(1920.0, 1080.0)
                .expect("worker should update the remote size");
            handle
                .submit_cursor_image(
                    RemoteCursorImage::new(vec![255_u8; 16], 2, 2, 0, 1, 1)
                        .expect("cursor should be valid"),
                )
                .expect("worker should submit a cursor image");
            handle
                .set_cursor_position(960.0, 540.0)
                .expect("worker should update the cursor position");
            handle
                .set_cursor_hidden()
                .expect("worker should hide the cursor");
            handle
                .set_cursor_default()
                .expect("worker should restore the cursor");
            handle
                .release_all_inputs()
                .expect("worker should release captured input state");
        });
        worker.join().expect("worker should finish");

        let close_dispatcher = window.dispatcher();
        window
            .dispatch(move || close_dispatcher.request_close())
            .expect("window should accept close request");
        assert_eq!(window.run(), 0);
    }

    #[test]
    fn remote_input_handle_rejects_updates_after_region_destruction() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let handle = {
            let input = RemoteInputRegion::new().expect("remote input should be created");
            window.remote_input_region_handle(&input)
        };
        assert!(matches!(
            handle.set_remote_size(1920.0, 1080.0),
            Err(Error::WidgetDestroyed)
        ));
        assert!(matches!(
            handle.set_cursor_default(),
            Err(Error::WidgetDestroyed)
        ));
        assert!(matches!(
            handle.release_all_inputs(),
            Err(Error::WidgetDestroyed)
        ));
    }

    #[test]
    fn realtime_frame_handle_submits_worker_frames_on_the_window_thread() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let frame_view = RealtimeFrameView::new().expect("frame view should be created");
        window.set_content(frame_view.as_widget());
        let handle = window.realtime_frame_view_handle(&frame_view);
        let worker = thread::spawn(move || {
            for frame_id in 1..=3 {
                handle
                    .submit_frame(
                        RemoteFrame::new(
                            vec![frame_id as u8; 16],
                            2,
                            2,
                            0,
                            PixelFormat::Bgra8888,
                            frame_id,
                            frame_id * 100,
                        )
                        .expect("worker frame should be valid"),
                    )
                    .expect("worker should submit a realtime frame");
            }
            handle
                .submit_damage(
                    RemoteFrameDamage::new(
                        2,
                        2,
                        PixelFormat::Bgra8888,
                        vec![RemoteFramePatch::new(vec![9_u8; 4], 1, 1, 1, 1, 0)],
                        4,
                        400,
                    )
                    .expect("worker damage should be valid"),
                )
                .expect("worker should submit realtime damage");
        });
        worker.join().expect("worker should finish");

        let close_dispatcher = window.dispatcher();
        window
            .dispatch(move || close_dispatcher.request_close())
            .expect("window should accept close request");
        assert_eq!(window.run(), 0);
    }

    #[test]
    fn realtime_frame_handle_rejects_updates_after_view_destruction() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let handle = {
            let frame_view = RealtimeFrameView::new().expect("frame view should be created");
            window.realtime_frame_view_handle(&frame_view)
        };
        let frame = RemoteFrame::new(vec![0_u8; 4], 1, 1, 0, PixelFormat::Bgra8888, 1, 100)
            .expect("remote frame should be valid");
        assert!(matches!(
            handle.submit_frame(frame),
            Err(Error::WidgetDestroyed)
        ));
    }

    #[test]
    fn label_handle_coalesces_worker_updates_on_the_window_thread() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let label = Label::new("connecting").expect("label should be created");
        window.set_content(label.as_widget());
        let handle = window.label_handle(&label);
        let worker = thread::spawn(move || {
            handle
                .set_text("authenticating")
                .expect("worker should submit label text");
            handle
                .set_text("connected")
                .expect("worker should replace pending label text");
        });
        worker.join().expect("worker should finish");

        let close_dispatcher = window.dispatcher();
        window
            .dispatch(move || close_dispatcher.request_close())
            .expect("window should accept close request");
        assert_eq!(window.run(), 0);
    }

    #[test]
    fn label_handle_rejects_updates_after_the_label_is_destroyed() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let handle = {
            let label = Label::new("temporary").expect("label should be created");
            window.label_handle(&label)
        };

        assert!(matches!(
            handle.set_text("too late"),
            Err(Error::WidgetDestroyed)
        ));
    }

    #[test]
    fn text_area_handle_coalesces_worker_updates_on_the_window_thread() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let text_area = TextArea::new("").expect("text area should be created");
        window.set_content(text_area.as_widget());
        let handle = window.text_area_handle(&text_area);
        let worker = thread::spawn(move || {
            handle
                .set_text("connecting")
                .expect("worker should submit text area content");
            handle
                .set_text("connected")
                .expect("worker should replace pending text area content");
        });
        worker.join().expect("worker should finish");

        let close_dispatcher = window.dispatcher();
        window
            .dispatch(move || close_dispatcher.request_close())
            .expect("window should accept close request");
        assert_eq!(window.run(), 0);
    }

    #[test]
    fn text_area_handle_rejects_updates_after_the_editor_is_destroyed() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let handle = {
            let text_area = TextArea::new("").expect("text area should be created");
            window.text_area_handle(&text_area)
        };

        assert!(matches!(
            handle.set_text("too late"),
            Err(Error::WidgetDestroyed)
        ));
    }

    #[test]
    fn progress_bar_clamps_values_and_worker_handle_is_lifetime_safe() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let progress = ProgressBar::new().expect("progress bar should be created");
        progress.set_value(1.5);
        assert_eq!(progress.value(), 1.0);
        window.set_content(progress.as_widget());
        let handle = window.progress_bar_handle(&progress);
        let worker = thread::spawn(move || {
            handle
                .set_value(0.25)
                .expect("worker should submit progress value");
            handle
                .set_value(0.75)
                .expect("worker should coalesce progress value");
        });
        worker.join().expect("worker should finish");

        let close_dispatcher = window.dispatcher();
        window
            .dispatch(move || close_dispatcher.request_close())
            .expect("window should accept close request");
        assert_eq!(window.run(), 0);
    }

    #[test]
    fn progress_bar_handle_rejects_updates_after_widget_destruction() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let handle = {
            let progress = ProgressBar::new().expect("progress bar should be created");
            window.progress_bar_handle(&progress)
        };
        assert!(matches!(handle.set_value(0.5), Err(Error::WidgetDestroyed)));
    }

    #[test]
    fn widget_handle_rejects_layout_updates_after_widget_destruction() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let handle = {
            let panel = Panel::new().expect("panel should be created");
            window.widget_handle(panel.as_widget())
        };
        assert!(matches!(
            handle.set_visible(false),
            Err(Error::WidgetDestroyed)
        ));
        assert!(matches!(
            handle.set_preferred_size(240.0, 120.0),
            Err(Error::WidgetDestroyed)
        ));
    }

    #[test]
    fn virtual_list_handle_coalesces_row_updates_on_the_window_thread() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let list = VirtualList::new().expect("virtual list should be created");
        list.set_items(&[
            ListItem {
                title: "Alpha".to_owned(),
                detail: "Pending".to_owned(),
            },
            ListItem {
                title: "Beta".to_owned(),
                detail: "Pending".to_owned(),
            },
        ]);
        window.set_content(list.as_widget());
        let handle = window.virtual_list_handle(&list);
        let worker = thread::spawn(move || {
            handle
                .update_item(
                    1,
                    ListItem {
                        title: "Beta".to_owned(),
                        detail: "Checking".to_owned(),
                    },
                )
                .expect("worker should submit a row update");
            handle
                .update_item(
                    1,
                    ListItem {
                        title: "Beta".to_owned(),
                        detail: "Online".to_owned(),
                    },
                )
                .expect("worker should replace the pending row update");
            handle
                .update_rich_item(
                    0,
                    VirtualListItem {
                        title: "Alpha".to_owned(),
                        detail: "Online".to_owned(),
                        badge: "HTTPS".to_owned(),
                        trailing: "Ready".to_owned(),
                        indicator_color: Some(Color::rgb(34, 197, 94)),
                        trailing_color: None,
                    },
                )
                .expect("worker should submit a rich row update");
        });
        worker.join().expect("worker should finish");

        let close_dispatcher = window.dispatcher();
        window
            .dispatch(move || close_dispatcher.request_close())
            .expect("window should accept close request");
        assert_eq!(window.run(), 0);
    }

    #[test]
    fn virtual_list_handle_replaces_a_background_data_revision() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let list = VirtualList::new().expect("virtual list should be created");
        list.set_items(&[ListItem {
            title: "Loading".to_owned(),
            detail: String::new(),
        }]);
        window.set_content(list.as_widget());
        let handle = window.virtual_list_handle(&list);
        let close_dispatcher = window.dispatcher();
        let worker = thread::spawn(move || {
            handle
                .set_items(vec![
                    ListItem {
                        title: "Alpha".to_owned(),
                        detail: "Ready".to_owned(),
                    },
                    ListItem {
                        title: "Beta".to_owned(),
                        detail: "Ready".to_owned(),
                    },
                ])
                .expect("background revision should be accepted");
            close_dispatcher.request_close();
        });
        assert_eq!(window.run(), 0);
        worker.join().expect("worker should finish");
    }

    #[test]
    fn virtual_list_handle_rejects_updates_after_the_list_is_destroyed() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let handle = {
            let list = VirtualList::new().expect("virtual list should be created");
            window.virtual_list_handle(&list)
        };

        assert!(matches!(
            handle.update_item(
                0,
                ListItem {
                    title: "Too late".to_owned(),
                    detail: String::new(),
                },
            ),
            Err(Error::WidgetDestroyed)
        ));
    }

    #[test]
    fn table_preserves_utf8_selection_and_coalesces_background_row_updates() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let table = Table::new().expect("table should be created");
        table.set_columns(&[
            TableColumn {
                header: "主机".to_owned(),
                width: 0.0,
            },
            TableColumn {
                header: "状态".to_owned(),
                width: 84.0,
            },
        ]);
        table.set_rows(&[
            TableRow {
                cells: vec!["生产节点".to_owned(), "在线".to_owned()],
            },
            TableRow {
                cells: vec!["数据库 🚀".to_owned(), "检测中".to_owned()],
            },
        ]);
        table.set_selection_mode(SelectionMode::Multiple);
        table.set_selected_indices(&[0, 1]);
        assert_eq!(table.selected_indices(), vec![0, 1]);
        window.set_content(table.as_widget());
        let handle = window.table_handle(&table);
        let worker = thread::spawn(move || {
            handle
                .update_row(
                    1,
                    TableRow {
                        cells: vec!["数据库 🚀".to_owned(), "在线".to_owned()],
                    },
                )
                .expect("worker should submit a table row update");
            handle
                .update_row(
                    1,
                    TableRow {
                        cells: vec!["数据库 🚀".to_owned(), "12 ms".to_owned()],
                    },
                )
                .expect("worker should coalesce a table row update");
        });
        worker.join().expect("worker should finish");

        let close_dispatcher = window.dispatcher();
        window
            .dispatch(move || close_dispatcher.request_close())
            .expect("window should accept close request");
        assert_eq!(window.run(), 0);
    }

    #[test]
    fn table_handle_replaces_rows_and_clears_selection_atomically() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let table = Table::new().expect("table should be created");
        table.set_rows(&[
            TableRow {
                cells: vec!["旧目录".to_owned()],
            },
            TableRow {
                cells: vec!["旧文件".to_owned()],
            },
        ]);
        table.set_selection_mode(SelectionMode::Multiple);
        table.set_selected_indices(&[1]);
        window.set_content(table.as_widget());

        let handle = window.table_handle(&table);
        handle
            .set_rows_and_clear_selection(vec![TableRow {
                cells: vec!["新目录".to_owned()],
            }])
            .expect("table revision should be accepted");
        let close_dispatcher = window.dispatcher();
        window
            .dispatch(move || close_dispatcher.request_close())
            .expect("window should accept close request");

        assert_eq!(window.run(), 0);
        assert!(table.selected_indices().is_empty());
    }

    #[test]
    fn virtual_list_full_reset_discards_row_patches_from_the_previous_revision() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let list = VirtualList::new().expect("virtual list should be created");
        list.set_items(&[ListItem {
            title: "Old".to_owned(),
            detail: "Pending".to_owned(),
        }]);
        window.set_content(list.as_widget());
        let handle = window.virtual_list_handle(&list);
        handle
            .update_item(
                0,
                ListItem {
                    title: "Old".to_owned(),
                    detail: "Online".to_owned(),
                },
            )
            .expect("row patch should be queued");

        list.set_items(&[ListItem {
            title: "New".to_owned(),
            detail: "Unknown".to_owned(),
        }]);
        assert!(list
            .state
            .pending_items
            .lock()
            .expect("virtual list pending items lock")
            .is_empty());

        let close_dispatcher = window.dispatcher();
        window
            .dispatch(move || close_dispatcher.request_close())
            .expect("window should accept close request");
        assert_eq!(window.run(), 0);
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
    fn dispatcher_defers_non_send_ui_work_without_crossing_threads() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let dispatcher = window.dispatcher();
        let observed = Rc::new(Cell::new(false));
        let observed_from_task = Rc::clone(&observed);
        let close_dispatcher = dispatcher.clone();
        dispatcher
            .dispatch_local(move || {
                observed_from_task.set(true);
                close_dispatcher.request_close();
            })
            .expect("window should accept local deferred work");

        assert!(!observed.get());
        assert_eq!(window.run(), 0);
        assert!(observed.get());
    }

    #[test]
    fn window_state_allows_reentrant_ui_thread_raw_access() {
        let state = WindowState {
            raw: Mutex::new(Some(NonNull::dangling())),
            ui_thread: thread::current().id(),
        };

        let nested = state.with_raw(|outer| state.with_raw(|inner| std::ptr::eq(outer, inner)));

        assert_eq!(nested, Some(Some(true)));
    }

    #[test]
    fn dispatcher_runs_one_shot_animation_frame_on_the_window_thread() {
        let _guard = window_test_lock().lock().expect("window test lock");
        let window = Window::new(&WindowOptions::default()).expect("window should be created");
        let dispatcher = window.dispatcher();
        let observed = Rc::new(Cell::new(false));
        let observed_from_frame = Rc::clone(&observed);
        let close_dispatcher = dispatcher.clone();
        dispatcher
            .request_animation_frame_local(move |now_ms| {
                assert!(now_ms.is_finite());
                observed_from_frame.set(true);
                close_dispatcher.request_close();
            })
            .expect("window should accept animation frame work");

        assert!(!observed.get());
        assert_eq!(window.run(), 0);
        assert!(observed.get());
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
        assert!(matches!(
            dispatcher.confirm_blocking("Confirm", "Continue?"),
            Err(Error::UiThreadBlockingOperation)
        ));
        assert!(matches!(
            dispatcher.prompt_blocking(
                "Authentication",
                "Enter a value",
                PromptOptions {
                    placeholder: "Value",
                    ..PromptOptions::default()
                }
            ),
            Err(Error::UiThreadBlockingOperation)
        ));
        let background_dispatcher = dispatcher.clone();
        let direct_prompt_result = thread::spawn(move || {
            (
                background_dispatcher.confirm("Confirm", "Continue?"),
                background_dispatcher.prompt("Prompt", "Enter a value", PromptOptions::default()),
            )
        })
        .join()
        .expect("worker should finish");
        assert!(matches!(direct_prompt_result.0, Err(Error::WrongThread)));
        assert!(matches!(direct_prompt_result.1, Err(Error::WrongThread)));
        window.close();

        assert!(dropped.load(Ordering::Acquire));
        assert!(matches!(
            dispatcher.dispatch(|| {}),
            Err(Error::WindowClosed)
        ));
        let closed_result =
            thread::spawn(move || dispatcher.confirm_blocking("Confirm", "Continue?"))
                .join()
                .expect("worker should finish");
        assert!(matches!(closed_result, Err(Error::WindowClosed)));
    }
}
