use super::*;
use std::ffi::c_void;

/// `None` means that the window system cannot confirm this state.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DesktopWindowState {
    pub visible: Option<bool>,
    pub active: Option<bool>,
    pub minimized: Option<bool>,
    pub maximized: Option<bool>,
    pub fullscreen: Option<bool>,
    pub topmost: Option<bool>,
    pub resizable: Option<bool>,
    pub width: f32,
    pub height: f32,
    pub pixel_width: f32,
    pub pixel_height: f32,
    pub dpi_scale: f32,
}
impl From<sys::OneUiWindowState> for DesktopWindowState {
    fn from(v: sys::OneUiWindowState) -> Self {
        let field = |bit| (v.known_fields & bit != 0).then_some(v.flags & bit != 0);
        Self {
            visible: field(1),
            active: field(2),
            minimized: field(4),
            maximized: field(8),
            fullscreen: field(16),
            topmost: field(32),
            resizable: field(64),
            width: v.width,
            height: v.height,
            pixel_width: v.pixel_width,
            pixel_height: v.pixel_height,
            dpi_scale: v.dpi_scale,
        }
    }
}
pub struct WindowStateSubscription {
    raw: NonNull<sys::OneUiWindowSubscription>,
    _ui_thread: PhantomData<Rc<()>>,
}
impl Drop for WindowStateSubscription {
    fn drop(&mut self) {
        unsafe { sys::oneui_window_subscription_destroy(self.raw.as_ptr()) };
    }
}
type Observer = Box<dyn Fn(DesktopWindowState)>;
unsafe extern "C" fn destroy_observer(data: *mut c_void) {
    drop(Box::from_raw(data.cast::<Observer>()));
}
unsafe extern "C" fn observe(value: *const sys::OneUiWindowState, data: *mut c_void) {
    let callback = &*data.cast::<Observer>();
    let value = DesktopWindowState::from(*value);
    let _ = catch_unwind(AssertUnwindSafe(|| callback(value)));
}
impl Window {
    pub fn desktop_state(&self) -> Option<DesktopWindowState> {
        self.state.with_raw(|w| {
            let mut value = sys::OneUiWindowState::default();
            (unsafe { sys::oneui_window_get_state(w, &mut value) } != 0).then(|| value.into())
        })?
    }
    pub fn hide(&self) -> bool {
        self.state
            .with_raw(|w| unsafe { sys::oneui_window_hide(w) != 0 })
            .unwrap_or(false)
    }
    pub fn restore(&self) -> bool {
        self.state
            .with_raw(|w| unsafe { sys::oneui_window_restore(w) != 0 })
            .unwrap_or(false)
    }
    pub fn maximize(&self) -> bool {
        self.state
            .with_raw(|w| unsafe { sys::oneui_window_maximize(w) != 0 })
            .unwrap_or(false)
    }
    pub fn set_topmost(&self, enabled: bool) -> bool {
        self.state
            .with_raw(|w| unsafe { sys::oneui_window_set_topmost(w, i32::from(enabled)) != 0 })
            .unwrap_or(false)
    }
    pub fn set_resizable(&self, enabled: bool) -> bool {
        self.state
            .with_raw(|w| unsafe { sys::oneui_window_set_resizable(w, i32::from(enabled)) != 0 })
            .unwrap_or(false)
    }
    pub fn set_client_size(&self, width: f32, height: f32) -> bool {
        self.state
            .with_raw(|w| unsafe { sys::oneui_window_set_client_size(w, width, height) != 0 })
            .unwrap_or(false)
    }
    pub fn set_maximum_client_size(&self, width: f32, height: f32) -> bool {
        self.state
            .with_raw(|w| unsafe {
                sys::oneui_window_set_maximum_client_size(w, width, height) != 0
            })
            .unwrap_or(false)
    }
    pub fn set_owner(&self, parent: Option<&Window>) -> bool {
        let owner = parent
            .and_then(|p| p.state.current_raw())
            .map_or(std::ptr::null_mut(), |p| p.as_ptr());
        self.state
            .with_raw(|w| unsafe { sys::oneui_window_set_owner(w, owner) != 0 })
            .unwrap_or(false)
    }
    pub fn observe_state(
        &self,
        callback: impl Fn(DesktopWindowState) + 'static,
    ) -> Option<WindowStateSubscription> {
        self.state.with_raw(|w| {
            let data = Box::into_raw(Box::new(Box::new(callback) as Observer)).cast();
            NonNull::new(unsafe {
                sys::oneui_window_observe_state(w, Some(observe), data, Some(destroy_observer))
            })
            .map(|raw| WindowStateSubscription {
                raw,
                _ui_thread: PhantomData,
            })
        })?
    }
}
pub struct ModalSession {
    raw: NonNull<sys::OneUiModalSession>,
    _ui_thread: PhantomData<Rc<()>>,
}
type Completion = Box<dyn Fn(i32)>;
unsafe extern "C" fn destroy_completion(data: *mut c_void) {
    drop(Box::from_raw(data.cast::<Completion>()));
}
unsafe extern "C" fn completed(value: i32, data: *mut c_void) {
    let callback = &*data.cast::<Completion>();
    let _ = catch_unwind(AssertUnwindSafe(|| callback(value)));
}
impl ModalSession {
    pub fn show(child: &Window, parent: &Window, callback: impl Fn(i32) + 'static) -> Option<Self> {
        let child = child.state.current_raw()?;
        let parent = parent.state.current_raw()?;
        let data = Box::into_raw(Box::new(Box::new(callback) as Completion)).cast();
        NonNull::new(unsafe {
            sys::oneui_modal_show(
                child.as_ptr(),
                parent.as_ptr(),
                Some(completed),
                data,
                Some(destroy_completion),
            )
        })
        .map(|raw| Self {
            raw,
            _ui_thread: PhantomData,
        })
    }
    pub fn finish(&self, result: i32) {
        unsafe { sys::oneui_modal_finish(self.raw.as_ptr(), result) };
    }
}
impl Drop for ModalSession {
    fn drop(&mut self) {
        unsafe { sys::oneui_modal_destroy(self.raw.as_ptr()) };
    }
}
