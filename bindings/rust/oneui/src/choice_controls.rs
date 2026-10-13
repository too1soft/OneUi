//! Choice controls bindings.

use super::{
    run_callback_guarded, sys, traced_value_callback, wide_null_terminated, Error,
    InteractionTrace, Widget,
};

pub(super) struct BoolChangedCallback {
    pub(super) handler: Box<dyn FnMut(bool) + 'static>,
}

pub(super) unsafe extern "C" fn run_bool_changed_callback(
    value: i32,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<BoolChangedCallback>() };
    run_callback_guarded("value.bool_changed", || (callback.handler)(value != 0));
}

/// A native binary setting control. The callback is retained by the wrapper,
/// so the C++ event handler can never outlive Rust-owned state.
pub struct Switch {
    widget: Widget,
    changed_callback: Option<Box<BoolChangedCallback>>,
}

impl Switch {
    pub fn new(text: &str) -> Result<Self, Error> {
        let text = wide_null_terminated(text);
        let widget = Widget::from_raw(unsafe { sys::oneui_switch_create(text.as_ptr()) })?;
        Ok(Self {
            widget,
            changed_callback: None,
        })
    }

    pub fn set_text(&self, text: &str) {
        let text = wide_null_terminated(text);
        unsafe { sys::oneui_switch_set_text(self.widget.as_raw(), text.as_ptr()) };
    }

    pub fn set_checked(&self, checked: bool) {
        unsafe { sys::oneui_switch_set_checked(self.widget.as_raw(), i32::from(checked)) };
    }

    pub fn checked(&self) -> bool {
        unsafe { sys::oneui_switch_checked(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(bool) + 'static,
    {
        let trace = InteractionTrace::at("Switch", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(BoolChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("switch callback was just installed")
            as *mut BoolChangedCallback)
            .cast();
        unsafe {
            sys::oneui_switch_set_on_changed(
                self.widget.as_raw(),
                Some(run_bool_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_switch_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.changed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Switch {
    fn drop(&mut self) {
        self.clear_on_changed();
    }
}

/// A native check box for independent option selection. It shares OneUI's
/// focus, accessibility, keyboard, style-sheet and callback lifetime rules.
pub struct Checkbox {
    widget: Widget,
    changed_callback: Option<Box<BoolChangedCallback>>,
}

impl Checkbox {
    pub fn new(text: &str) -> Result<Self, Error> {
        let text = wide_null_terminated(text);
        let widget = Widget::from_raw(unsafe { sys::oneui_checkbox_create(text.as_ptr()) })?;
        Ok(Self {
            widget,
            changed_callback: None,
        })
    }

    pub fn set_text(&self, text: &str) {
        let text = wide_null_terminated(text);
        unsafe { sys::oneui_checkbox_set_text(self.widget.as_raw(), text.as_ptr()) };
    }

    pub fn set_checked(&self, checked: bool) {
        unsafe { sys::oneui_checkbox_set_checked(self.widget.as_raw(), i32::from(checked)) };
    }

    pub fn checked(&self) -> bool {
        unsafe { sys::oneui_checkbox_checked(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(bool) + 'static,
    {
        let trace = InteractionTrace::at("Checkbox", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(BoolChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("checkbox callback was just installed")
            as *mut BoolChangedCallback)
            .cast();
        unsafe {
            sys::oneui_checkbox_set_on_changed(
                self.widget.as_raw(),
                Some(run_bool_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_checkbox_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.changed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Checkbox {
    fn drop(&mut self) {
        self.clear_on_changed();
    }
}

pub(super) struct IndexChangedCallback {
    pub(super) handler: Box<dyn FnMut(i32) + 'static>,
}

pub(super) unsafe extern "C" fn run_index_changed_callback(
    value: i32,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<IndexChangedCallback>() };
    run_callback_guarded("value.index_changed", || (callback.handler)(value));
}

/// Keyboard-accessible segmented choice control backed by OneUI's `Tabs`.
/// Item labels are serialized only at the FFI boundary; product code works
/// exclusively with structured UTF-8 strings.
pub struct SegmentedControl {
    widget: Widget,
    changed_callback: Option<Box<IndexChangedCallback>>,
}

impl SegmentedControl {
    pub fn new(items: &[String]) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_segmented_control_create() })?;
        let control = Self {
            widget,
            changed_callback: None,
        };
        control.set_items(items);
        Ok(control)
    }

    pub fn set_items(&self, items: &[String]) {
        let native: Vec<_> = items
            .iter()
            .map(|item| sys::OneUiUtf8String::from_str(item))
            .collect();
        unsafe {
            sys::oneui_tabs_set_items_utf8(self.widget.as_raw(), native.as_ptr(), native.len())
        };
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_segmented_control_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_segmented_control_selected_index(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at(
            "SegmentedControl",
            "changed",
            std::panic::Location::caller(),
        );
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(IndexChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("segmented control callback was just installed")
            as *mut IndexChangedCallback)
            .cast();
        unsafe {
            sys::oneui_segmented_control_set_on_changed(
                self.widget.as_raw(),
                Some(run_index_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_segmented_control_set_on_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.changed_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for SegmentedControl {
    fn drop(&mut self) {
        self.clear_on_changed();
    }
}
