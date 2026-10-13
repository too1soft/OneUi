//! Tree bindings.

use super::{
    run_callback_guarded, sys, traced_value_callback, traced_values_callback, Error,
    InteractionTrace, Widget,
};

/// Structured tree data. `id` is a stable opaque identifier; an empty
/// `parent_id` denotes a root and every field remains UTF-8 at the ABI edge.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct TreeItem {
    pub id: String,
    pub parent_id: String,
    pub title: String,
    pub detail: String,
    pub expanded: bool,
}

struct TreeViewSelectionCallback {
    handler: Box<dyn FnMut(String) + 'static>,
}

struct TreeViewExpansionCallback {
    handler: Box<dyn FnMut(String, bool) + 'static>,
}

/// A proposed tree reorder addressed by stable application IDs.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TreeReorderRequest {
    pub source_id: String,
    pub target_id: String,
}

struct TreeViewReorderCallback {
    handler: Box<dyn FnMut(TreeReorderRequest) + 'static>,
}

unsafe extern "C" fn run_tree_view_selection_callback(
    text: *const std::ffi::c_char,
    length: usize,
    user_data: *mut std::ffi::c_void,
) {
    if text.is_null() || user_data.is_null() {
        return;
    }
    let bytes = unsafe { std::slice::from_raw_parts(text.cast::<u8>(), length) };
    let id = String::from_utf8_lossy(bytes).into_owned();
    let callback = unsafe { &mut *user_data.cast::<TreeViewSelectionCallback>() };
    run_callback_guarded("tree.selection_changed", || (callback.handler)(id));
}

unsafe extern "C" fn run_tree_view_expansion_callback(
    id: *const std::ffi::c_char,
    length: usize,
    expanded: i32,
    user_data: *mut std::ffi::c_void,
) {
    if id.is_null() || user_data.is_null() {
        return;
    }
    let bytes = unsafe { std::slice::from_raw_parts(id.cast::<u8>(), length) };
    let id = String::from_utf8_lossy(bytes).into_owned();
    let callback = unsafe { &mut *user_data.cast::<TreeViewExpansionCallback>() };
    run_callback_guarded("tree.expansion_changed", || {
        (callback.handler)(id, expanded != 0)
    });
}

unsafe extern "C" fn run_tree_view_reorder_callback(
    source_id: *const std::ffi::c_char,
    source_length: usize,
    target_id: *const std::ffi::c_char,
    target_length: usize,
    user_data: *mut std::ffi::c_void,
) {
    if source_id.is_null() || target_id.is_null() || user_data.is_null() {
        return;
    }
    let source = unsafe { std::slice::from_raw_parts(source_id.cast::<u8>(), source_length) };
    let target = unsafe { std::slice::from_raw_parts(target_id.cast::<u8>(), target_length) };
    let request = TreeReorderRequest {
        source_id: String::from_utf8_lossy(source).into_owned(),
        target_id: String::from_utf8_lossy(target).into_owned(),
    };
    let callback = unsafe { &mut *user_data.cast::<TreeViewReorderCallback>() };
    run_callback_guarded("tree.reorder_requested", || (callback.handler)(request));
}

/// Native hierarchical navigation with ID-based selection and local expansion.
pub struct TreeView {
    widget: Widget,
    selection_callback: Option<Box<TreeViewSelectionCallback>>,
    expansion_callback: Option<Box<TreeViewExpansionCallback>>,
    reorder_callback: Option<Box<TreeViewReorderCallback>>,
}

impl TreeView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_tree_view_create() })?;
        Ok(Self {
            widget,
            selection_callback: None,
            expansion_callback: None,
            reorder_callback: None,
        })
    }

    pub fn set_items(&self, items: &[TreeItem]) {
        let native_items: Vec<sys::OneUiTreeItemUtf8> = items
            .iter()
            .map(|item| sys::OneUiTreeItemUtf8 {
                id: sys::OneUiUtf8String::from_str(&item.id),
                parent_id: sys::OneUiUtf8String::from_str(&item.parent_id),
                title: sys::OneUiUtf8String::from_str(&item.title),
                detail: sys::OneUiUtf8String::from_str(&item.detail),
                expanded: i32::from(item.expanded),
            })
            .collect();
        unsafe {
            sys::oneui_tree_view_set_items_utf8(
                self.widget.as_raw(),
                native_items.as_ptr(),
                native_items.len(),
            )
        };
    }

    pub fn set_selected_id(&self, id: &str) {
        unsafe {
            sys::oneui_tree_view_set_selected_id_utf8(
                self.widget.as_raw(),
                sys::OneUiUtf8String::from_str(id),
            )
        };
    }

    pub fn content_height(&self) -> f32 {
        unsafe { sys::oneui_tree_view_content_height(self.widget.as_raw()) }
    }

    pub fn selected_id(&self) -> String {
        let required = unsafe {
            sys::oneui_tree_view_selected_id_utf8(self.widget.as_raw(), std::ptr::null_mut(), 0)
        };
        if required <= 1 {
            return String::new();
        }
        let mut bytes = vec![0u8; required];
        unsafe {
            sys::oneui_tree_view_selected_id_utf8(
                self.widget.as_raw(),
                bytes.as_mut_ptr().cast(),
                bytes.len(),
            )
        };
        bytes.pop();
        String::from_utf8_lossy(&bytes).into_owned()
    }

    /// Updates the externally dragged item's drop target using client-space coordinates.
    ///
    /// The returned ID is empty when the pointer is outside a visible row. This API only
    /// owns transient presentation state; product code remains responsible for validating
    /// and applying the domain operation when the drag is dropped.
    pub fn update_external_drop_target(&self, x: f32, y: f32) -> String {
        unsafe { sys::oneui_tree_view_update_external_drop_target(self.widget.as_raw(), x, y) };
        self.external_drop_target_id()
    }

    pub fn clear_external_drop_target(&self) {
        unsafe { sys::oneui_tree_view_clear_external_drop_target(self.widget.as_raw()) };
    }

    pub fn external_drop_target_id(&self) -> String {
        let required = unsafe {
            sys::oneui_tree_view_external_drop_target_id_utf8(
                self.widget.as_raw(),
                std::ptr::null_mut(),
                0,
            )
        };
        if required <= 1 {
            return String::new();
        }
        let mut bytes = vec![0u8; required];
        unsafe {
            sys::oneui_tree_view_external_drop_target_id_utf8(
                self.widget.as_raw(),
                bytes.as_mut_ptr().cast(),
                bytes.len(),
            )
        };
        bytes.pop();
        String::from_utf8_lossy(&bytes).into_owned()
    }

    #[track_caller]
    pub fn set_on_selection_changed<F>(&mut self, callback: F)
    where
        F: FnMut(String) + 'static,
    {
        let trace = InteractionTrace::at(
            "TreeView",
            "selection_changed",
            std::panic::Location::caller(),
        );
        self.clear_selection_callback();
        self.selection_callback = Some(Box::new(TreeViewSelectionCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .selection_callback
            .as_deref_mut()
            .expect("tree view selection callback was just installed")
            as *mut TreeViewSelectionCallback)
            .cast();
        unsafe {
            sys::oneui_tree_view_set_on_selection_changed_utf8(
                self.widget.as_raw(),
                Some(run_tree_view_selection_callback),
                user_data,
            )
        };
    }

    pub fn clear_selection_callback(&mut self) {
        unsafe {
            sys::oneui_tree_view_set_on_selection_changed_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.selection_callback = None;
    }

    #[track_caller]
    pub fn set_on_expansion_changed<F>(&mut self, callback: F)
    where
        F: FnMut(String, bool) + 'static,
    {
        let trace = InteractionTrace::at(
            "TreeView",
            "expansion_changed",
            std::panic::Location::caller(),
        );
        self.clear_expansion_callback();
        self.expansion_callback = Some(Box::new(TreeViewExpansionCallback {
            handler: Box::new(traced_values_callback(trace, callback)),
        }));
        let user_data = (self
            .expansion_callback
            .as_deref_mut()
            .expect("tree view expansion callback was just installed")
            as *mut TreeViewExpansionCallback)
            .cast();
        unsafe {
            sys::oneui_tree_view_set_on_expansion_changed_utf8(
                self.widget.as_raw(),
                Some(run_tree_view_expansion_callback),
                user_data,
            )
        };
    }

    pub fn clear_expansion_callback(&mut self) {
        unsafe {
            sys::oneui_tree_view_set_on_expansion_changed_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.expansion_callback = None;
    }

    /// Enables pointer and `Alt+Up`/`Alt+Down` reorder requests.
    pub fn set_reorder_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_tree_view_set_reorder_enabled(self.widget.as_raw(), i32::from(enabled))
        };
    }

    pub fn reorder_enabled(&self) -> bool {
        unsafe { sys::oneui_tree_view_reorder_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_reorder_requested<F>(&mut self, callback: F)
    where
        F: FnMut(TreeReorderRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "TreeView",
            "reorder_requested",
            std::panic::Location::caller(),
        );
        self.clear_reorder_callback();
        self.reorder_callback = Some(Box::new(TreeViewReorderCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .reorder_callback
            .as_deref_mut()
            .expect("tree view reorder callback was just installed")
            as *mut TreeViewReorderCallback)
            .cast();
        unsafe {
            sys::oneui_tree_view_set_on_reorder_requested_utf8(
                self.widget.as_raw(),
                Some(run_tree_view_reorder_callback),
                user_data,
            )
        };
    }

    pub fn clear_reorder_callback(&mut self) {
        unsafe {
            sys::oneui_tree_view_set_on_reorder_requested_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.reorder_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for TreeView {
    fn drop(&mut self) {
        self.clear_reorder_callback();
        self.clear_selection_callback();
        self.clear_expansion_callback();
    }
}
