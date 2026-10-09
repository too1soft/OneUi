//! Native application slots; no copied layout algorithm or implicit window ownership.
use crate::{sys, Error, Insets, Widget};

fn raw(child: Option<&Widget>) -> *mut sys::OneUiWidget {
    child.map_or(std::ptr::null_mut(), Widget::as_raw)
}

/// Sidebar, header, content and footer in one retained native layout.
///
/// Slots retain native children. Keep Rust control wrappers alive while their
/// callbacks are needed. Passing `None` clears a slot. Hiding the sidebar
/// preserves its native child, but removes it from drawing and input traversal.
pub struct AppShell {
    widget: Widget,
}

impl AppShell {
    pub fn new() -> Result<Self, Error> {
        Ok(Self {
            widget: Widget::from_raw(unsafe { sys::oneui_app_shell_create() })?,
        })
    }
    pub fn set_sidebar(&self, child: Option<&Widget>) {
        unsafe { sys::oneui_app_shell_set_sidebar(self.widget.as_raw(), raw(child)) };
    }
    pub fn set_header(&self, child: Option<&Widget>) {
        unsafe { sys::oneui_app_shell_set_header(self.widget.as_raw(), raw(child)) };
    }
    pub fn set_content(&self, child: Option<&Widget>) {
        unsafe { sys::oneui_app_shell_set_content(self.widget.as_raw(), raw(child)) };
    }
    pub fn set_footer(&self, child: Option<&Widget>) {
        unsafe { sys::oneui_app_shell_set_footer(self.widget.as_raw(), raw(child)) };
    }
    pub fn set_sidebar_width(&self, width: f32) {
        unsafe { sys::oneui_app_shell_set_sidebar_width(self.widget.as_raw(), width) };
    }
    pub fn set_header_height(&self, height: f32) {
        unsafe { sys::oneui_app_shell_set_header_height(self.widget.as_raw(), height) };
    }
    pub fn set_footer_height(&self, height: f32) {
        unsafe { sys::oneui_app_shell_set_footer_height(self.widget.as_raw(), height) };
    }
    pub fn set_footer_span_sidebar(&self, span: bool) {
        unsafe {
            sys::oneui_app_shell_set_footer_span_sidebar(self.widget.as_raw(), i32::from(span))
        };
    }
    pub fn set_gap(&self, gap: f32) {
        unsafe { sys::oneui_app_shell_set_gap(self.widget.as_raw(), gap) };
    }
    pub fn set_padding(&self, padding: Insets) {
        unsafe { sys::oneui_app_shell_set_padding(self.widget.as_raw(), padding.into()) };
    }
    pub fn set_sidebar_visible(&self, visible: bool) {
        unsafe {
            sys::oneui_app_shell_set_sidebar_visible(self.widget.as_raw(), i32::from(visible))
        };
    }
    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

/// Leading content and trailing actions, measured by the shared native layout.
pub struct TopBar {
    widget: Widget,
}

impl TopBar {
    pub fn new() -> Result<Self, Error> {
        Ok(Self {
            widget: Widget::from_raw(unsafe { sys::oneui_top_bar_create() })?,
        })
    }
    pub fn set_leading(&self, child: Option<&Widget>) {
        unsafe { sys::oneui_top_bar_set_leading(self.widget.as_raw(), raw(child)) };
    }
    pub fn add_action(&self, child: &Widget) {
        unsafe { sys::oneui_top_bar_add_action(self.widget.as_raw(), child.as_raw()) };
    }
    pub fn clear_actions(&self) {
        unsafe { sys::oneui_top_bar_clear_actions(self.widget.as_raw()) };
    }
    pub fn set_gap(&self, gap: f32) {
        unsafe { sys::oneui_top_bar_set_gap(self.widget.as_raw(), gap) };
    }
    pub fn set_padding(&self, padding: Insets) {
        unsafe { sys::oneui_top_bar_set_padding(self.widget.as_raw(), padding.into()) };
    }
    pub fn set_leading_width(&self, width: f32) {
        unsafe { sys::oneui_top_bar_set_leading_width(self.widget.as_raw(), width) };
    }
    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}
