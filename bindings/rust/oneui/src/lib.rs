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

    /// Sets the layout hint used by parent containers. A zero dimension stays
    /// flexible on that axis, matching OneUI's native layout contract.
    pub fn set_preferred_size(&self, width: f32, height: f32) {
        unsafe { sys::oneui_widget_set_preferred_size(self.as_raw(), width, height) };
    }

    pub fn set_disabled(&self, disabled: bool) {
        unsafe { sys::oneui_widget_set_disabled(self.as_raw(), i32::from(disabled)) };
    }

    pub fn set_visible(&self, visible: bool) {
        unsafe { sys::oneui_widget_set_visible(self.as_raw(), i32::from(visible)) };
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
}

impl From<InteractiveSurfaceStyle> for sys::OneUiInteractiveSurfaceStyle {
    fn from(value: InteractiveSurfaceStyle) -> Self {
        Self {
            normal: value.normal.into(),
            hovered: value.hovered.into(),
            pressed: value.pressed.into(),
            disabled: value.disabled.into(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IconSymbol {
    BrandBloom = 0,
    Search = 1,
    RemoteAssist = 2,
    Monitor = 3,
    Device = 4,
    Toolbox = 5,
    Compass = 6,
    Settings = 7,
    Bell = 8,
    Minimize = 9,
    Maximize = 10,
    Restore = 11,
    Close = 12,
    Heart = 13,
    Desktop = 14,
    File = 15,
    Sparkle = 16,
    RadioOn = 17,
    RadioOff = 18,
    ToggleOn = 19,
    KeyDots = 20,
    Copy = 21,
    ChevronDown = 22,
    ChevronUp = 23,
    Plus = 24,
    User = 25,
    Globe = 26,
    Play = 27,
    Check = 28,
    BrandMark = 29,
    CheckCircle = 30,
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
    handler: Box<dyn FnMut() + 'static>,
}

unsafe extern "C" fn run_void_callback(user_data: *mut std::ffi::c_void) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<VoidCallback>() };
    let _ = catch_unwind(AssertUnwindSafe(|| (callback.handler)()));
}

fn wide_null_terminated(value: &str) -> Vec<u16> {
    value.encode_utf16().chain(std::iter::once(0)).collect()
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

    pub fn as_widget(&self) -> &Widget {
        &self.widget
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

    pub fn set_on_click<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        self.clear_on_click();
        self.callback = Some(Box::new(VoidCallback {
            handler: Box::new(callback),
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

/// A reusable native card/list-row surface with hover and press transitions.
pub struct InteractiveSurface {
    widget: Widget,
    callback: Option<Box<VoidCallback>>,
}

impl InteractiveSurface {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_interactive_surface_create() })?;
        Ok(Self {
            widget,
            callback: None,
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

    pub fn set_on_click<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        self.clear_on_click();
        self.callback = Some(Box::new(VoidCallback {
            handler: Box::new(callback),
        }));
        let user_data = (self
            .callback
            .as_deref_mut()
            .expect("interactive surface callback was just installed")
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
        self.callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for InteractiveSurface {
    fn drop(&mut self) {
        self.clear_on_click();
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

    pub fn set_on_click<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        self.clear_on_click();
        self.callback = Some(Box::new(VoidCallback {
            handler: Box::new(callback),
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

    /// Selects a named built-in chrome variant, such as `dark`.
    pub fn set_variant(&self, variant: &str) {
        let variant = std::ffi::CString::new(variant)
            .expect("OneUI title bar variants cannot contain a NUL byte");
        unsafe { sys::oneui_title_bar_set_variant(self.widget.as_raw(), variant.as_ptr()) };
    }

    pub fn set_on_minimize<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        self.clear_on_minimize();
        self.minimize_callback = Some(Box::new(VoidCallback {
            handler: Box::new(callback),
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

    pub fn set_on_maximize<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        self.clear_on_maximize();
        self.maximize_callback = Some(Box::new(VoidCallback {
            handler: Box::new(callback),
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

    pub fn set_on_close<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        self.clear_on_close();
        self.close_callback = Some(Box::new(VoidCallback {
            handler: Box::new(callback),
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

    pub fn set_on_click<F>(&mut self, callback: F)
    where
        F: FnMut() + 'static,
    {
        self.clear_on_click();
        self.callback = Some(Box::new(VoidCallback {
            handler: Box::new(callback),
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

struct TerminalViewportCallback {
    handler: Box<dyn FnMut(TerminalViewport) + 'static>,
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

unsafe extern "C" fn run_terminal_viewport_callback(
    rows: u16,
    columns: u16,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<TerminalViewportCallback>() };
    let _ = catch_unwind(AssertUnwindSafe(|| {
        (callback.handler)(TerminalViewport { rows, columns })
    }));
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
    viewport_callback: Option<Box<TerminalViewportCallback>>,
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
            viewport_callback: None,
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

    pub fn set_on_viewport_changed<F>(&mut self, callback: F)
    where
        F: FnMut(TerminalViewport) + 'static,
    {
        self.clear_viewport_callback();
        self.viewport_callback = Some(Box::new(TerminalViewportCallback {
            handler: Box::new(callback),
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
        self.clear_viewport_callback();
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

struct ListChangedCallback {
    handler: Box<dyn FnMut(i32) + 'static>,
}

unsafe extern "C" fn run_list_changed_callback(value: i32, user_data: *mut std::ffi::c_void) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<ListChangedCallback>() };
    let _ = catch_unwind(AssertUnwindSafe(|| (callback.handler)(value)));
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

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_list_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_list_selected_index(self.widget.as_raw()) }
    }

    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(callback),
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
    let _ = catch_unwind(AssertUnwindSafe(|| (callback.handler)(id)));
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
    let _ = catch_unwind(AssertUnwindSafe(|| (callback.handler)(id, expanded != 0)));
}

/// Native hierarchical navigation with ID-based selection and local expansion.
pub struct TreeView {
    widget: Widget,
    selection_callback: Option<Box<TreeViewSelectionCallback>>,
    expansion_callback: Option<Box<TreeViewExpansionCallback>>,
}

impl TreeView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_tree_view_create() })?;
        Ok(Self {
            widget,
            selection_callback: None,
            expansion_callback: None,
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

    pub fn set_on_selection_changed<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        self.clear_selection_callback();
        self.selection_callback = Some(Box::new(TreeViewSelectionCallback {
            handler: Box::new(callback),
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

    pub fn set_on_expansion_changed<F>(&mut self, callback: F)
    where
        F: FnMut(String, bool) + 'static,
    {
        self.clear_expansion_callback();
        self.expansion_callback = Some(Box::new(TreeViewExpansionCallback {
            handler: Box::new(callback),
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

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for TreeView {
    fn drop(&mut self) {
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

    pub fn set_borderless(&self, borderless: bool) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_borderless(raw, i32::from(borderless));
        });
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
        terminal_style, Button, Color, Error, Insets, InteractiveSurface,
        InteractiveSurfaceStateStyle, InteractiveSurfaceStyle, Label, List, ListItem, ScrollView,
        Stack, StackDirection, TerminalCell, TerminalColor, TerminalCursor, TerminalFrame,
        TerminalView, TextField, TreeItem, TreeView, Window, WindowOptions,
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
        assert_eq!(tree.content_height(), 64.0);
        tree.as_widget().set_preferred_size(240.0, 0.0);
        window.set_content(tree.as_widget());
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
