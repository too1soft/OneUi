//! Overlays bindings.

use super::visual_style::Insets;
use super::{
    run_callback_guarded, run_void_callback, sys, traced_callback, traced_value_callback,
    wide_null_terminated, Error, IconSymbol, InteractionTrace, Rc, RefCell, VoidCallback, Widget,
};

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

    /// Update bounds in place without removing a floating widget or losing its focus.
    #[allow(clippy::too_many_arguments)]
    pub fn update_anchored_overlay(
        &self,
        child: &Widget,
        width: f32,
        height: f32,
        margin: Insets,
        horizontal_alignment: OverlayAlignment,
        vertical_alignment: OverlayAlignment,
    ) -> bool {
        unsafe {
            sys::oneui_overlay_host_update_anchored_overlay(
                self.widget.as_raw(),
                child.as_raw(),
                width,
                height,
                margin.into(),
                horizontal_alignment as i32,
                vertical_alignment as i32,
            ) != 0
        }
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
    closed_callback: RefCell<Option<Box<VoidCallback>>>,
}

impl Popup {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_popup_create() })?;
        Ok(Self {
            widget,
            closed_callback: RefCell::new(None),
        })
    }

    pub fn set_anchor(&self, anchor: &Widget) {
        unsafe { sys::oneui_popup_set_anchor(self.widget.as_raw(), anchor.as_raw()) };
    }

    pub fn set_content(&self, content: &Widget) {
        unsafe { sys::oneui_popup_set_content(self.widget.as_raw(), content.as_raw()) };
    }

    /// Called after a real open -> closed transition (Escape, outside click or API).
    pub fn set_on_closed<F: FnMut() + 'static>(&self, callback: F) {
        self.clear_on_closed();
        let mut slot = self.closed_callback.borrow_mut();
        *slot = Some(Box::new(VoidCallback {
            handler: Rc::new(RefCell::new(Box::new(callback))),
        }));
        let data = (slot.as_deref_mut().unwrap() as *mut VoidCallback).cast();
        unsafe {
            sys::oneui_popup_set_on_closed(self.widget.as_raw(), Some(run_void_callback), data)
        };
    }
    pub fn clear_on_closed(&self) {
        unsafe { sys::oneui_popup_set_on_closed(self.widget.as_raw(), None, std::ptr::null_mut()) };
        self.closed_callback.borrow_mut().take();
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

impl Drop for Popup {
    fn drop(&mut self) {
        self.clear_on_closed();
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
