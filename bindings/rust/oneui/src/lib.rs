//! Safe foundation bindings for OneUI.
//!
//! This crate intentionally starts with window lifecycle and UTF-8 title
//! handling. Higher-level controls and async UI dispatch will be added without
//! changing the raw ABI exposed by `oneui-sys`.

use std::marker::PhantomData;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr::NonNull;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

pub use oneui_sys as sys;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    AbiVersionMismatch { expected: u32, actual: u32 },
    WindowCreationFailed,
    WidgetCreationFailed,
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
    use super::{Error, Insets, Label, Stack, StackDirection, Window, WindowOptions};
    use std::sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex,
    };
    use std::thread;

    #[test]
    fn creates_hidden_window_through_utf8_abi() {
        let window = Window::new(&WindowOptions {
            title: "iShellPro 麒麟 🚀".to_owned(),
            ..WindowOptions::default()
        })
        .expect("OneUI window should be created through the UTF-8 ABI");
        window.set_title("兴业银行股份有限公司");
    }

    #[test]
    fn mounts_rust_composed_content_into_a_hidden_window() {
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
    fn dispatcher_runs_work_on_the_window_thread() {
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
