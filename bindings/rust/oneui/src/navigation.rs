//! Navigation bindings.

use super::{
    run_void_callback, sys, traced_callback, wide_null_terminated, Error, IconSymbol,
    InteractionTrace, Rc, RefCell, VoidCallback, Widget,
};

/// Native title bar for borderless desktop windows.
pub struct WindowTitleBar {
    pub(super) widget: Widget,
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
