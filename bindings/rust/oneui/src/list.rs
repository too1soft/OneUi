//! List bindings.

use super::visual_style::Color;
use super::{
    run_callback_guarded, sys, traced_value_callback, Error, InteractionTrace, ListItem,
    VirtualListItem, Widget,
};

pub(super) fn native_virtual_list_item(item: &VirtualListItem) -> sys::OneUiRichListItemUtf8 {
    sys::OneUiRichListItemUtf8 {
        title: sys::OneUiUtf8String::from_str(&item.title),
        detail: sys::OneUiUtf8String::from_str(&item.detail),
        badge: sys::OneUiUtf8String::from_str(&item.badge),
        trailing: sys::OneUiUtf8String::from_str(&item.trailing),
        indicator_color: item
            .indicator_color
            .unwrap_or(Color::rgba(0, 0, 0, 0))
            .into(),
        trailing_color: item
            .trailing_color
            .unwrap_or(Color::rgba(0, 0, 0, 0))
            .into(),
        indicator_visible: i32::from(item.indicator_color.is_some()),
    }
}

pub(super) struct ListChangedCallback {
    pub(super) handler: Box<dyn FnMut(i32) + 'static>,
}

pub(super) unsafe extern "C" fn run_list_changed_callback(
    value: i32,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<ListChangedCallback>() };
    run_callback_guarded("list.index_changed", || (callback.handler)(value));
}

pub(super) struct ListSelectionChangedCallback {
    pub(super) handler: Box<dyn FnMut(Vec<i32>) + 'static>,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ContextMenuRequest {
    pub index: i32,
    pub x: f32,
    pub y: f32,
}

/// A proposed list reorder. The control never mutates application data;
/// callers validate and apply this request before assigning updated items.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ReorderRequest {
    pub source_index: i32,
    pub target_index: i32,
}

pub(super) struct ContextMenuRequestedCallback {
    pub(super) handler: Box<dyn FnMut(ContextMenuRequest) + 'static>,
}

pub(super) struct ReorderRequestedCallback {
    pub(super) handler: Box<dyn FnMut(ReorderRequest) + 'static>,
}

pub(super) unsafe extern "C" fn run_list_selection_changed_callback(
    values: *const i32,
    count: usize,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() || (values.is_null() && count > 0) {
        return;
    }
    let selected = if count == 0 {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(values, count) }.to_vec()
    };
    let callback = unsafe { &mut *user_data.cast::<ListSelectionChangedCallback>() };
    run_callback_guarded("list.selection_changed", || (callback.handler)(selected));
}

pub(super) unsafe extern "C" fn run_context_menu_requested_callback(
    index: i32,
    x: f32,
    y: f32,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<ContextMenuRequestedCallback>() };
    let request = ContextMenuRequest { index, x, y };
    run_callback_guarded("list.context_menu", || (callback.handler)(request));
}

pub(super) unsafe extern "C" fn run_reorder_requested_callback(
    source_index: i32,
    target_index: i32,
    user_data: *mut std::ffi::c_void,
) {
    if user_data.is_null() {
        return;
    }
    let callback = unsafe { &mut *user_data.cast::<ReorderRequestedCallback>() };
    let request = ReorderRequest {
        source_index,
        target_index,
    };
    run_callback_guarded("list.reorder_requested", || (callback.handler)(request));
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

    /// Controls whether the list must always keep one item selected.
    ///
    /// The default is `true` for backward compatibility. Set this to `false`
    /// before assigning index `-1` when the surrounding editor has an explicit
    /// new/empty state.
    pub fn set_selection_required(&self, required: bool) {
        unsafe {
            sys::oneui_list_set_selection_required(self.widget.as_raw(), i32::from(required))
        };
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_list_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_list_selected_index(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("List", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
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
