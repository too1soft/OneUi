//! Containers bindings.

use super::visual_style::{Color, Insets};
use super::{
    run_void_callback, sys, traced_callback, wide_null_terminated, Error, IconSymbol,
    InteractionTrace, Rc, RefCell, VoidCallback, Widget,
};

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
    pub(super) widget: Widget,
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
