//! Scroll container lifetime and its thread-safe update handle.
use super::super::{sys, Color, Error, UiDispatcher, Widget};
use std::sync::{
    atomic::{AtomicPtr, Ordering},
    Arc,
};

/// A scroll container whose child remains owned by the Rust composition tree.
pub struct ScrollView {
    widget: Widget,
    state: Arc<ScrollViewState>,
}

struct ScrollViewState {
    raw: AtomicPtr<sys::OneUiWidget>,
}

/// Thread-safe producer for scroll position changes on a mounted scroll view.
#[derive(Clone)]
pub struct ScrollViewHandle {
    state: Arc<ScrollViewState>,
    dispatcher: UiDispatcher,
}

impl ScrollViewHandle {
    pub fn set_scroll_offset(&self, offset: f32) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe { sys::oneui_scroll_view_set_scroll_offset(raw, offset) };
            }
        })
    }

    pub fn scroll_to_bottom(&self) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe { sys::oneui_scroll_view_scroll_to_bottom(raw) };
            }
        })
    }
}

impl ScrollView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_scroll_view_create() })?;
        Ok(Self {
            state: Arc::new(ScrollViewState {
                raw: AtomicPtr::new(widget.as_raw()),
            }),
            widget,
        })
    }

    pub fn set_content(&self, child: &Widget) {
        unsafe { sys::oneui_scroll_view_set_content(self.widget.as_raw(), child.as_raw()) };
    }

    pub fn set_content_width(&self, width: f32) {
        unsafe { sys::oneui_scroll_view_set_content_width(self.widget.as_raw(), width) };
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

    /// Space between the scrollbar and viewport edge. The default is 5 pixels.
    pub fn set_scrollbar_inset(&self, inset: f32) {
        unsafe { sys::oneui_scroll_view_set_scrollbar_inset(self.widget.as_raw(), inset) };
    }

    pub fn set_scroll_offset(&self, offset: f32) {
        unsafe { sys::oneui_scroll_view_set_scroll_offset(self.widget.as_raw(), offset) };
    }

    pub fn scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_scroll_view_scroll_offset(self.widget.as_raw()) }
    }

    pub fn max_scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_scroll_view_max_scroll_offset(self.widget.as_raw()) }
    }

    pub fn scroll_to_bottom(&self) {
        unsafe { sys::oneui_scroll_view_scroll_to_bottom(self.widget.as_raw()) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for ScrollView {
    fn drop(&mut self) {
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
    }
}

impl UiDispatcher {
    /// Creates a thread-safe scroll producer for a mounted scroll view.
    pub fn scroll_view_handle(&self, scroll_view: &ScrollView) -> ScrollViewHandle {
        ScrollViewHandle {
            state: Arc::clone(&scroll_view.state),
            dispatcher: self.clone(),
        }
    }
}
