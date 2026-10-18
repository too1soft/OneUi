//! Button bindings.

use super::visual_style::{ButtonContentAlign, ButtonStyle, ButtonVariant};
use super::{
    run_void_callback, sys, traced_callback, Error, IconSymbol, InteractionTrace, Rc, RefCell,
    VoidCallback, Widget,
};

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

    /// Removes the leading icon while retaining button identity and handlers.
    pub fn clear_icon(&self) {
        unsafe { sys::oneui_button_set_icon(self.widget.as_raw(), -1) };
    }

    pub fn set_content_align(&self, align: ButtonContentAlign) {
        unsafe { sys::oneui_button_set_content_align(self.widget.as_raw(), align as i32) };
    }

    pub fn set_trailing_text(&self, text: &str) {
        let text = sys::OneUiUtf8String::from_str(text);
        unsafe { sys::oneui_button_set_trailing_text_utf8(self.widget.as_raw(), text) };
    }

    pub fn set_trailing_icon(&self, symbol: IconSymbol) {
        unsafe { sys::oneui_button_set_trailing_icon(self.widget.as_raw(), symbol as i32) };
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
