//! Safe foundation bindings for OneUI.
//!
//! This crate owns the safe Rust boundary for OneUI's UTF-8 C ABI. It begins
//! with window lifetime and grows reusable controls without exposing raw FFI to
//! product applications.

mod style_sheet;
pub use style_sheet::{StyleSheet, StyleSheetError};
mod clipboard;
pub use clipboard::{clipboard_text, set_clipboard_text};
mod window_operations;
#[cfg(test)]
use window_operations::parse_multiple_file_dialog_output;
pub use window_operations::{FileDialogFilter, FileDialogMode, FileDialogOptions, PromptOptions};
mod text_field;
pub use text_field::{TextField, TextFieldHandle};
mod text_area;
pub use text_area::{TextArea, TextAreaHandle};
mod select;
pub use select::{Select, SelectHandle};
mod progress_bar;
pub use progress_bar::{ProgressBar, ProgressBarHandle, ProgressSegment};
mod sparkline;
pub use sparkline::{Sparkline, SparklineHandle};
mod time_series;
#[cfg(test)]
use time_series::{run_time_series_inspection_callback, TimeSeriesInspectionCallback};
pub use time_series::{
    TimeSeries, TimeSeriesChart, TimeSeriesChartHandle, TimeSeriesInspection, TimeSeriesThreshold,
};
mod virtual_list;
pub use virtual_list::{VirtualList, VirtualListHandle};
mod table;
pub use table::{
    Table, TableCellKind, TableColumn, TableHandle, TableRichCell, TableRichRow, TableRow,
};
mod terminal;
pub use terminal::{TerminalView, TerminalViewHandle};
mod terminal_types;
pub use terminal_types::{
    terminal_style, TerminalAuxiliaryButtonAction, TerminalCell, TerminalColor, TerminalCursor,
    TerminalCursorStyle, TerminalFrame, TerminalPointerAction, TerminalPointerButton,
    TerminalPointerEvent, TerminalSelection, TerminalUnderlineStyle, TerminalViewOptions,
    TerminalViewport,
};
mod realtime_frame_view;
pub use realtime_frame_view::{RealtimeFrameView, RealtimeFrameViewHandle};
mod remote_input;
#[cfg(test)]
use remote_input::{run_remote_text_input_callback, RemoteTextInputCallback};
pub use remote_input::{RemoteInputRegion, RemoteInputRegionHandle};
mod visual_style;
pub use visual_style::{
    ButtonContentAlign, ButtonStateStyle, ButtonStyle, ButtonVariant, Color, FocusRingStyle,
    Insets, InteractiveSurfaceStateStyle, InteractiveSurfaceStyle, LabelAlign, LabelTextSpan, Rect,
};
mod overlays;
pub use overlays::{
    Dialog, Menu, OverlayAlignment, OverlayHost, Popup, PopupInteractionMode,
    PopupPreferredPlacement,
};
mod containers;
pub use containers::{Panel, StateView};
mod button;
pub use button::Button;
mod pointer_surface;
pub use pointer_surface::{
    GridReorderRequest, InteractiveSurface, ItemDragEvent, ItemDragPhase, PointerButton,
    PointerCursor, PointerDragEvent, PointerEvent, ReorderableGrid,
};
mod images;
pub use images::{Icon, IconButton, ImageContentAlignment, ImageContentMode, ImageView};
mod navigation;
pub use navigation::{NavItem, WindowTitleBar};
mod choice_controls;
use choice_controls::{run_index_changed_callback, IndexChangedCallback};
pub use choice_controls::{Checkbox, SegmentedControl, Switch};
mod key_event;
pub use key_event::RawKeyEvent;
mod remote_frame;
pub use remote_frame::{
    PixelFormat, RemoteCursorImage, RemoteCursorMode, RemoteFrame, RemoteFrameDamage,
    RemoteFramePatch, RemotePointerButton, RemotePointerEvent, VideoScaleMode,
};
mod log_view;
pub use log_view::{LogLine, LogView};
mod list;
use list::{
    run_context_menu_requested_callback, run_reorder_requested_callback,
    ContextMenuRequestedCallback, ReorderRequestedCallback,
};
pub use list::{ContextMenuRequest, List, ReorderRequest};
mod tree;
pub use tree::{TreeItem, TreeReorderRequest, TreeView};
mod terminal_render;
#[cfg(test)]
use terminal_render::{
    native_terminal_cells, should_apply_application_cursor_style, terminal_dirty_ranges,
};

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::{CStr, CString};
use std::marker::PhantomData;
use std::ops::Range;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::path::{Path, PathBuf};
use std::ptr::NonNull;
use std::rc::Rc;
use std::sync::{
    atomic::{AtomicBool, AtomicPtr, Ordering},
    Arc, Mutex, OnceLock,
};

pub use oneui_sys as sys;
mod dispatcher;
pub mod view;
pub use dispatcher::UiDispatcher;
use dispatcher::{trace_ui_task, WindowState};

pub mod controls;
pub mod handles;
pub mod interaction;
mod latest_signal;
pub mod layout;
mod qa_input;
pub use layout::{
    AppShell, ScrollView, ScrollViewHandle, SplitOrientation, SplitView, Stack, StackAlign,
    StackDirection, TopBar,
};
mod layout_authoring;
pub mod ui;
pub use latest_signal::{LatestSignal, LocalSignalSubscription};
pub use layout_authoring::{Flex, FlexBasis, StackEngine, StackJustify};
pub mod platform;
pub use interaction::{
    CommandRegistration, CommandResult, KeyChord, KeyModifiers, TextAffinity, TextDirection,
    TextOptions, TextPosition, TextWrapMode,
};
pub use platform::{WindowBackend, WindowCapabilities};
mod application;
mod desktop_window;
mod notifications;
mod toast_host;
pub use notifications::{
    Notification, NotificationAction, NotificationPermission, NotificationPermissionResult,
    NotificationResult, NotificationService, NotificationStatus,
};
pub use toast_host::{ToastHost, ToastMessage};
mod fonts;
mod localization;
mod native_error;
mod window_callbacks;
mod window_native;
pub use application::{Application, ApplicationActivation, CloseDecision, QuitPolicy};
pub use desktop_window::{DesktopWindowState, ModalSession, WindowStateSubscription};
pub use fonts::{enumerate_system_fonts, measure_text_width, FontFamilyInfo};
pub use localization::{set_builtin_text, BuiltinText, LocalizationError};
pub use native_error::{clear_native_error, last_native_error, NativeError, NativeErrorCode};
mod slider;
mod types;
pub use slider::{Slider, SliderHandle, SliderInteraction};

pub use types::{IconSymbol, ListItem, SelectionMode, VirtualListItem, VirtualListRichMetrics};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    AbiVersionMismatch { expected: u32, actual: u32 },
    WindowCreationFailed,
    ApplicationOperationFailed,
    WidgetCreationFailed,
    WidgetDestroyed,
    CommandRegistrationFailed,
    InvalidTextOptions,
    InvalidMenu,
    InvalidLayout,
    InvalidTextPosition,
    WindowClosed,
    WrongThread,
    UiThreadBlockingOperation,
    FileDialogFailed,
    LayoutSnapshotFailed,
    InvalidVideoFrame { reason: &'static str },
    InvalidRemoteCursor { reason: &'static str },
    InvalidImagePixels { reason: &'static str },
    FontRegistrationFailed,
    FontEnumerationFailed,
}

/// Registers an application font directly from memory.
///
/// OneUI copies the bytes during this call and never installs the font into the
/// operating system. Register fonts before constructing the first window so
/// every text layout cache sees the same set of families.
pub fn register_font_from_memory(data: &[u8], family_alias: &str) -> Result<(), Error> {
    let family_alias = family_alias.trim();
    if data.is_empty() || family_alias.is_empty() {
        return Err(Error::FontRegistrationFailed);
    }
    let family_alias = sys::OneUiUtf8String::from_str(family_alias);
    let registered =
        unsafe { sys::oneui_font_register_memory(data.as_ptr(), data.len(), family_alias) };
    if registered == 1 {
        Ok(())
    } else {
        Err(Error::FontRegistrationFailed)
    }
}

/// Family alias for OneUI's optional bundled Simplified Chinese UI font.
#[cfg(feature = "bundled-fonts")]
pub const BUNDLED_UI_FONT_FAMILY: &str = "Noto Sans SC";

/// SIL Open Font License text shipped with the bundled Noto Sans SC binary.
#[cfg(feature = "bundled-fonts")]
pub const BUNDLED_UI_FONT_LICENSE: &str =
    include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/assets/fonts/OFL.txt"));

/// Registers the unmodified Noto Sans SC variable font embedded in this crate.
#[cfg(feature = "bundled-fonts")]
pub fn register_bundled_fonts() -> Result<(), Error> {
    static FONT: &[u8] = include_bytes!(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/assets/fonts/NotoSansSC-wght.ttf"
    ));
    register_font_from_memory(FONT, BUNDLED_UI_FONT_FAMILY)
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

/// Owned native filesystem paths and their logical client-space drop point.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct FileDropEvent {
    pub paths: Vec<String>,
    pub x: f32,
    pub y: f32,
}

pub struct Window {
    tray_action_callback: Option<Box<tray::MenuCallback>>,
    menu_action_callback: Option<Box<tray::MenuCallback>>,
    state: Arc<WindowState>,
    raw_key_callback: Option<Box<WindowRawKeyCallback>>,
    client_size_changed_callback: Option<Box<WindowClientSizeChangedCallback>>,
    activation_changed_callback: Option<Box<WindowActivationChangedCallback>>,
    file_drop_callback: Option<Box<WindowFileDropCallback>>,
    close_requested_callback: Option<Box<WindowCloseRequestedCallback>>,
    _ui_thread: PhantomData<Rc<()>>,
}

struct WindowRawKeyCallback {
    handler: Box<dyn FnMut(RawKeyEvent) -> bool + 'static>,
}

struct WindowClientSizeChangedCallback {
    handler: Box<dyn FnMut(f32, f32) + 'static>,
}

struct WindowActivationChangedCallback {
    handler: Box<dyn FnMut(bool) + 'static>,
}

struct WindowFileDropCallback {
    handler: Box<dyn FnMut(FileDropEvent) + 'static>,
}

struct WindowCloseRequestedCallback {
    handler: Box<dyn FnMut(bool) + 'static>,
}

impl UiDispatcher {
    /// Creates a thread-safe text producer for a label mounted in this window.
    pub fn label_handle(&self, label: &Label) -> LabelHandle {
        LabelHandle {
            state: Arc::clone(&label.state),
            dispatcher: self.clone(),
            guard: None,
            retention_guard: None,
            retained: None,
        }
    }
}

#[derive(Clone, Default)]
struct WidgetLayoutUpdate {
    visible: Option<bool>,
    preferred_size: Option<(f32, f32)>,
    classes: Option<String>,
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
        self.update_layout(Some(visible), None, None)
    }

    pub fn set_preferred_size(&self, width: f32, height: f32) -> Result<(), Error> {
        self.update_layout(None, Some((width, height)), None)
    }

    pub fn set_visible_and_preferred_size(
        &self,
        visible: bool,
        width: f32,
        height: f32,
    ) -> Result<(), Error> {
        self.update_layout(Some(visible), Some((width, height)), None)
    }

    /// Replaces semantic CSS classes on the owning UI thread. Updates are
    /// coalesced with pending visibility and size changes so a data-driven
    /// component can change presentation without posting redundant work.
    pub fn set_classes(&self, classes: impl Into<String>) -> Result<(), Error> {
        let classes = classes.into();
        if classes.contains('\0') {
            return Err(Error::InvalidTextOptions);
        }
        self.update_layout(None, None, Some(classes))
    }

    fn update_layout(
        &self,
        visible: Option<bool>,
        preferred_size: Option<(f32, f32)>,
        classes: Option<String>,
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
            if let Some(classes) = classes {
                pending.classes = Some(classes);
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
            if let Some(classes) = update.classes {
                if let Ok(classes) = CString::new(classes) {
                    unsafe { sys::oneui_widget_set_classes(raw, classes.as_ptr()) };
                }
            }

            state.update_scheduled.store(false, Ordering::Release);
            let has_more = {
                let pending = state
                    .pending_layout
                    .lock()
                    .expect("widget pending layout lock poisoned");
                pending.visible.is_some()
                    || pending.preferred_size.is_some()
                    || pending.classes.is_some()
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

/// A weak, UI-thread-only focus checkpoint. Restoring only searches the supplied
/// subtree; it never activates an operating-system window or revives a widget.
pub struct FocusBookmark {
    raw: NonNull<sys::OneUiFocusBookmark>,
    _ui_thread: std::marker::PhantomData<Rc<()>>,
}
impl FocusBookmark {
    pub fn restore(&self, root: &Widget) -> bool {
        unsafe { sys::oneui_focus_bookmark_restore(self.raw.as_ptr(), root.as_raw()) != 0 }
    }
}
impl Drop for FocusBookmark {
    fn drop(&mut self) {
        unsafe { sys::oneui_focus_bookmark_destroy(self.raw.as_ptr()) };
    }
}

type WidgetSizeHandler = std::rc::Rc<std::cell::RefCell<Box<dyn FnMut(f32, f32)>>>;
struct WidgetSizeCallback {
    handler: WidgetSizeHandler,
}
unsafe extern "C" fn run_widget_size_callback(
    width: f32,
    height: f32,
    data: *mut std::ffi::c_void,
) {
    if data.is_null() {
        return;
    }
    // Keep the handler alive when it clears its own registration or drops its wrapper.
    let handler = unsafe { std::rc::Rc::clone(&(*data.cast::<WidgetSizeCallback>()).handler) };
    run_callback_guarded("widget.size_changed", || {
        (handler.borrow_mut())(width, height)
    });
}
pub struct Widget {
    size_callback: std::cell::RefCell<Option<Box<WidgetSizeCallback>>>,
    raw: NonNull<sys::OneUiWidget>,
    state: Arc<WidgetState>,
}

impl Widget {
    /// Borrow the opaque C ABI widget handle for a native integration.
    ///
    /// # Safety
    /// The receiver/callback owner must outlive every foreign callback use.
    /// Call only on the UI thread; never destroy this borrowed handle or store
    /// it beyond `self`. A C++ view may retain the native widget separately,
    /// but that does not retain this Rust wrapper's callback allocations.
    pub unsafe fn with_native_handle<R>(&self, f: impl FnOnce(*mut sys::OneUiWidget) -> R) -> R {
        f(self.as_raw())
    }
    fn from_raw(raw: *mut sys::OneUiWidget) -> Result<Self, Error> {
        let raw = NonNull::new(raw).ok_or(Error::WidgetCreationFailed)?;
        Ok(Self {
            state: Arc::new(WidgetState {
                raw: AtomicPtr::new(raw.as_ptr()),
                pending_layout: Mutex::new(WidgetLayoutUpdate::default()),
                update_scheduled: AtomicBool::new(false),
            }),
            raw,
            size_callback: std::cell::RefCell::new(None),
        })
    }

    /// Observe actual dimensions committed by native layout. Position-only moves
    /// do not notify. Child layout hints can be updated synchronously; schedule
    /// mutations that replace the surrounding composition tree through a dispatcher.
    pub fn set_on_size_changed<F: FnMut(f32, f32) + 'static>(&self, callback: F) {
        self.clear_on_size_changed();
        let mut slot = self.size_callback.borrow_mut();
        *slot = Some(Box::new(WidgetSizeCallback {
            handler: std::rc::Rc::new(std::cell::RefCell::new(Box::new(callback))),
        }));
        let data = (slot.as_deref_mut().unwrap() as *mut WidgetSizeCallback).cast();
        unsafe {
            sys::oneui_widget_set_on_size_changed(
                self.as_raw(),
                Some(run_widget_size_callback),
                data,
            )
        };
    }
    pub fn clear_on_size_changed(&self) {
        unsafe { sys::oneui_widget_set_on_size_changed(self.as_raw(), None, std::ptr::null_mut()) };
        self.size_callback.borrow_mut().take();
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

    /// Weak checkpoint of the focused descendant, for moving existing content
    /// between containers without changing the user's keyboard target.
    pub fn capture_focus(&self) -> Option<FocusBookmark> {
        NonNull::new(unsafe { sys::oneui_widget_capture_focus(self.as_raw()) }).map(|raw| {
            FocusBookmark {
                raw,
                _ui_thread: std::marker::PhantomData,
            }
        })
    }

    /// Sets concise hover help for icon-only and unfamiliar controls.
    pub fn set_tooltip(&self, tooltip: &str) {
        let tooltip = wide_null_terminated(tooltip);
        unsafe { sys::oneui_widget_set_tooltip(self.as_raw(), tooltip.as_ptr()) };
    }

    /// Gives an icon-only or custom control its accessible action name.
    pub fn set_accessible_name(&self, name: &str) {
        let name = wide_null_terminated(name);
        unsafe { sys::oneui_widget_set_accessible_name(self.as_raw(), name.as_ptr()) };
    }

    pub fn set_accessible_description(&self, description: &str) {
        let description = wide_null_terminated(description);
        unsafe {
            sys::oneui_widget_set_accessible_description(self.as_raw(), description.as_ptr())
        };
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
        self.clear_on_size_changed();
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

fn wide_null_terminated(value: &str) -> Vec<sys::WChar> {
    #[cfg(windows)]
    {
        value.encode_utf16().chain(std::iter::once(0)).collect()
    }
    #[cfg(not(windows))]
    {
        value
            .chars()
            .map(u32::from)
            .chain(std::iter::once(0))
            .collect()
    }
}

fn wide_to_string(value: &[sys::WChar]) -> String {
    #[cfg(windows)]
    {
        String::from_utf16_lossy(value)
    }
    #[cfg(not(windows))]
    {
        value
            .iter()
            .map(|&ch| char::from_u32(ch).unwrap_or(char::REPLACEMENT_CHARACTER))
            .collect()
    }
}

struct SecretWide(Vec<sys::WChar>);

impl SecretWide {
    fn new(value: &str) -> Self {
        Self(wide_null_terminated(value))
    }

    fn as_slice(&self) -> &[sys::WChar] {
        &self.0
    }
}

impl Drop for SecretWide {
    fn drop(&mut self) {
        self.0.fill(0);
    }
}

fn validate_label_text_spans(text: &str, spans: &[LabelTextSpan]) -> Result<(), Error> {
    let mut previous_end = 0usize;
    for span in spans {
        let metrics_valid = span
            .font_size
            .is_none_or(|size| size.is_finite() && size > 0.0)
            && span
                .font_weight
                .is_none_or(|weight| (1..=1000).contains(&weight));
        let range_valid = span.range.start < span.range.end
            && span.range.end <= text.len()
            && span.range.start >= previous_end
            && text.is_char_boundary(span.range.start)
            && text.is_char_boundary(span.range.end);
        if !metrics_valid || !range_valid {
            return Err(Error::InvalidTextOptions);
        }
        previous_end = span.range.end;
    }
    Ok(())
}

fn apply_label_content(
    raw: *mut sys::OneUiWidget,
    text: &str,
    spans: &[LabelTextSpan],
) -> Result<(), Error> {
    validate_label_text_spans(text, spans)?;
    let native = spans
        .iter()
        .map(|span| sys::OneUiLabelTextSpanUtf8 {
            start_utf8_offset: span.range.start,
            end_utf8_offset: span.range.end,
            font_size: span.font_size.unwrap_or(0.0),
            font_weight: span.font_weight.unwrap_or(0),
            italic: i32::from(span.italic),
            monospace: i32::from(span.monospace),
            underline: i32::from(span.underline),
            strikethrough: i32::from(span.strikethrough),
            foreground: span.foreground.map(Into::into).unwrap_or_default(),
            background: span.background.map(Into::into).unwrap_or_default(),
        })
        .collect::<Vec<_>>();
    let text = sys::OneUiUtf8String::from_str(text);
    let applied =
        unsafe { sys::oneui_label_set_rich_text_utf8(raw, text, native.as_ptr(), native.len()) };
    if applied == 0 {
        Err(Error::InvalidTextOptions)
    } else {
        Ok(())
    }
}

#[derive(Clone)]
struct LabelContentUpdate {
    text: String,
    spans: Vec<LabelTextSpan>,
    guard: Option<Arc<dyn Fn() -> bool + Send + Sync>>,
    retention_guard: Option<Arc<dyn Fn() -> bool + Send + Sync>>,
}

impl LabelContentUpdate {
    fn is_current(&self) -> bool {
        self.guard.as_ref().is_none_or(|guard| guard())
    }
}

include!("label_channel.rs");

pub struct Label {
    widget: Widget,
    state: Arc<LabelState>,
}

impl Label {
    /// Draw a leading status dot using the text's current foreground color.
    /// Dimensions are logical pixels; a zero diameter disables it.
    pub fn set_status_indicator(&self, diameter: f32, gap: f32) {
        unsafe { sys::oneui_label_set_status_indicator(self.widget.as_raw(), diameter, gap) };
    }

    pub fn new(text: &str) -> Result<Self, Error> {
        let text = sys::OneUiUtf8String::from_str(text);
        let widget = Widget::from_raw(unsafe { sys::oneui_label_create_utf8(text) })?;
        Ok(Self {
            state: Arc::new(LabelState {
                raw: AtomicPtr::new(widget.as_raw()),
                closed: AtomicBool::new(false),
                sink: None,
                pending_content: Mutex::new(None),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
        })
    }

    pub fn set_text(&self, text: &str) {
        let text = sys::OneUiUtf8String::from_str(text);
        unsafe { sys::oneui_label_set_text_utf8(self.widget.as_raw(), text) };
    }

    pub fn set_rich_text(&self, text: &str, spans: &[LabelTextSpan]) -> Result<(), Error> {
        apply_label_content(self.widget.as_raw(), text, spans)
    }

    pub fn set_font_size(&self, font_size: f32) {
        unsafe { sys::oneui_label_set_font_size(self.widget.as_raw(), font_size) };
    }

    /// The shaped width in logical pixels, using the widget's font context.
    pub fn natural_text_width(&self) -> f32 {
        unsafe { sys::oneui_label_natural_text_width(self.widget.as_raw()) }
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

    pub fn set_text_wrapping(&self, enabled: bool) {
        unsafe { sys::oneui_label_set_text_wrapping(self.widget.as_raw(), enabled as i32) };
    }

    pub fn set_max_lines(&self, max_lines: i32) {
        unsafe { sys::oneui_label_set_max_lines(self.widget.as_raw(), max_lines) };
    }

    pub fn set_line_height(&self, line_height: f32) {
        unsafe { sys::oneui_label_set_line_height(self.widget.as_raw(), line_height) };
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
            .pending_content
            .lock()
            .expect("label pending content lock poisoned")
            .take();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}

mod tabs;
pub use tabs::{TabDragOutEvent, TabEditReason, Tabs};

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

unsafe extern "C" fn run_window_activation_changed_callback(
    active: std::ffi::c_int,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<WindowActivationChangedCallback>() };
    run_callback_guarded("window.activation_changed", || {
        (callback.handler)(active != 0)
    });
}

unsafe extern "C" fn run_window_file_drop_callback(
    paths: *const sys::OneUiUtf8String,
    count: usize,
    x: f32,
    y: f32,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() || (count > 0 && paths.is_null()) {
        return;
    }
    let views = if count == 0 {
        &[][..]
    } else {
        unsafe { std::slice::from_raw_parts(paths, count) }
    };
    let paths = views
        .iter()
        .filter_map(|path| {
            if path.length > 0 && path.data.is_null() {
                return None;
            }
            let bytes = if path.length == 0 {
                &[][..]
            } else {
                unsafe { std::slice::from_raw_parts(path.data.cast::<u8>(), path.length) }
            };
            Some(String::from_utf8_lossy(bytes).into_owned())
        })
        .collect();
    let callback = unsafe { &mut *user_data.cast::<WindowFileDropCallback>() };
    run_callback_guarded("window.file_drop", || {
        (callback.handler)(FileDropEvent { paths, x, y })
    });
}

unsafe extern "C" fn run_window_close_requested_callback(
    force_exit: std::ffi::c_int,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<WindowCloseRequestedCallback>() };
    run_callback_guarded("window.close_requested", || {
        (callback.handler)(force_exit != 0)
    });
}

impl Drop for Window {
    fn drop(&mut self) {
        self.clear_tray_action_callback();
        self.clear_menu_action_callback();
        self.clear_close_requested_callback();
        self.clear_file_drop_callback();
        self.clear_activation_changed_callback();
        self.clear_client_size_changed_callback();
        self.clear_raw_key_callback();
        self.state.destroy();
    }
}

#[cfg(test)]
mod binding_tests;
#[cfg(test)]
mod test_runner;

#[cfg(test)]
pub fn run_native_tests() -> std::process::ExitCode {
    binding_tests::run()
}

mod title_bar_presentation;
mod tray;
pub use title_bar_presentation::*;
pub use tray::{MenuCheck, NativeMenuItem, TrayMenuItem};
