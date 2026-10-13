//! Virtual list bindings.

use super::list::{
    native_virtual_list_item, run_context_menu_requested_callback, run_list_changed_callback,
    run_list_selection_changed_callback, run_reorder_requested_callback, ContextMenuRequest,
    ContextMenuRequestedCallback, ListChangedCallback, ListSelectionChangedCallback,
    ReorderRequest, ReorderRequestedCallback,
};
use super::pointer_surface::{run_item_drag_callback, ItemDragCallback, ItemDragEvent};
use super::{
    sys, trace_ui_task, traced_value_callback, Arc, AtomicBool, AtomicPtr, BTreeMap, Error,
    InteractionTrace, ListItem, Mutex, Ordering, SelectionMode, UiDispatcher, VirtualListItem,
    VirtualListRichMetrics, Widget,
};

impl UiDispatcher {
    /// Creates a thread-safe producer for in-place virtual-list row updates.
    pub fn virtual_list_handle(&self, list: &VirtualList) -> VirtualListHandle {
        VirtualListHandle {
            state: Arc::clone(&list.state),
            dispatcher: self.clone(),
        }
    }
}

/// A fixed-height viewport-virtualized list for large structured data sets.
///
/// Unlike [`List`], this control retains all data but only paints rows visible
/// in its viewport. Keep row content to a title and optional detail line.
pub(super) enum PendingVirtualListItem {
    Basic(ListItem),
    Rich(VirtualListItem),
}

pub(super) struct VirtualListState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pub(super) pending_items: Mutex<BTreeMap<usize, PendingVirtualListItem>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for in-place row updates on a mounted virtual list.
///
/// Updates are coalesced per row and applied on the owning UI thread. Unlike a
/// full item reset, row updates preserve selection, scroll offset, and active
/// scroll motion.
#[derive(Clone)]
pub struct VirtualListHandle {
    state: Arc<VirtualListState>,
    dispatcher: UiDispatcher,
}

impl VirtualListHandle {
    /// Replaces a revision and applies its selected index in the same UI task.
    pub fn set_items_and_selected_index(
        &self,
        items: Vec<ListItem>,
        index: i32,
    ) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            state
                .pending_items
                .lock()
                .expect("virtual list pending items lock poisoned")
                .clear();
            let native = items
                .iter()
                .map(|item| sys::OneUiListItemUtf8 {
                    title: sys::OneUiUtf8String::from_str(&item.title),
                    detail: sys::OneUiUtf8String::from_str(&item.detail),
                })
                .collect::<Vec<_>>();
            // SAFETY: the UI-thread dispatch owns the widget access; items and
            // strings live until the synchronous C calls finish copying them.
            // The native control validates and clamps the selection index.
            unsafe {
                sys::oneui_virtual_list_set_items_utf8(raw, native.as_ptr(), native.len());
                sys::oneui_virtual_list_set_selected_index(raw, index);
            }
        })
    }

    /// Replace a scoped keyed revision without retargeting the selection.
    pub fn set_items_keyed<F, C, O, K>(
        &self,
        items: Vec<ListItem>,
        old_keys: O,
        keys: Vec<K>,
        current: F,
        commit: C,
    ) -> Result<(), Error>
    where
        F: Fn() -> bool + Send + 'static,
        C: FnOnce() + Send + 'static,
        O: FnOnce() -> Vec<K> + Send + 'static,
        K: Eq + Send + 'static,
    {
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() || !current() {
                return;
            }
            state
                .pending_items
                .lock()
                .expect("virtual list pending items lock poisoned")
                .clear();
            let old_keys = old_keys();
            let old = unsafe { sys::oneui_virtual_list_selected_index(raw) };
            let selected = old_keys
                .get(old as usize)
                .and_then(|k| keys.iter().position(|v| v == k))
                .map(|i| i as i32)
                .unwrap_or(-1);
            let offset = unsafe { sys::oneui_virtual_list_scroll_offset(raw) };
            let native = items
                .iter()
                .map(|item| sys::OneUiListItemUtf8 {
                    title: sys::OneUiUtf8String::from_str(&item.title),
                    detail: sys::OneUiUtf8String::from_str(&item.detail),
                })
                .collect::<Vec<_>>();
            commit();
            unsafe {
                sys::oneui_virtual_list_set_items_utf8(raw, native.as_ptr(), native.len());
                sys::oneui_virtual_list_set_selected_index(raw, selected);
                sys::oneui_virtual_list_set_scroll_offset(raw, offset);
            }
        })
    }

    /// Replaces the complete data revision on the owning window thread.
    ///
    /// Use this for background loads, filters, and directory scans. Incremental
    /// status changes should continue to use [`Self::update_item`] so active
    /// scrolling and selection are preserved.
    pub fn set_items(&self, items: Vec<ListItem>) -> Result<(), Error> {
        self.replace_items(items, false)
    }

    /// Replaces the complete data revision with structured operational rows.
    pub fn set_rich_items(&self, items: Vec<VirtualListItem>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            state
                .pending_items
                .lock()
                .expect("virtual list pending items lock poisoned")
                .clear();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            let native_items = items
                .iter()
                .map(native_virtual_list_item)
                .collect::<Vec<_>>();
            unsafe {
                sys::oneui_virtual_list_set_rich_items_utf8(
                    raw,
                    native_items.as_ptr(),
                    native_items.len(),
                );
            }
        })
    }

    /// Replaces the complete data revision and clears the selection in the
    /// same UI-thread task.
    ///
    /// Use this when row indices do not identify the same logical objects
    /// across revisions, such as directory listings after rename or delete.
    /// Clearing atomically prevents a retained index from targeting a
    /// different row between the revision and a later selection reset.
    pub fn set_items_and_clear_selection(&self, items: Vec<ListItem>) -> Result<(), Error> {
        self.replace_items(items, true)
    }

    fn replace_items(&self, items: Vec<ListItem>, clear_selection: bool) -> Result<(), Error> {
        self.set_items_with_commit(items, move || !clear_selection)
    }

    /// Commits the row model and visual revision in one UI task. Return true
    /// only when row identities are unchanged and selection may be retained.
    pub fn set_items_with_commit<F>(&self, items: Vec<ListItem>, commit: F) -> Result<(), Error>
    where
        F: FnOnce() -> bool + Send + 'static,
    {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            trace_ui_task("virtual-list revision started");
            state
                .pending_items
                .lock()
                .expect("virtual list pending items lock poisoned")
                .clear();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            let native_items = items
                .iter()
                .map(|item| sys::OneUiListItemUtf8 {
                    title: sys::OneUiUtf8String::from_str(&item.title),
                    detail: sys::OneUiUtf8String::from_str(&item.detail),
                })
                .collect::<Vec<_>>();
            let clear_selection = !commit();
            unsafe {
                sys::oneui_virtual_list_set_items_utf8(
                    raw,
                    native_items.as_ptr(),
                    native_items.len(),
                );
                if clear_selection {
                    sys::oneui_virtual_list_set_selected_index(raw, -1);
                }
            };
            trace_ui_task("virtual-list revision completed");
        })
    }

    pub fn update_item(&self, index: usize, item: ListItem) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }

        self.state
            .pending_items
            .lock()
            .expect("virtual list pending items lock poisoned")
            .insert(index, PendingVirtualListItem::Basic(item));
        self.schedule_pending_items()
    }

    /// Coalesces one operational-row update onto the owning window thread.
    pub fn update_rich_item(&self, index: usize, item: VirtualListItem) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        self.state
            .pending_items
            .lock()
            .expect("virtual list pending items lock poisoned")
            .insert(index, PendingVirtualListItem::Rich(item));
        self.schedule_pending_items()
    }

    fn schedule_pending_items(&self) -> Result<(), Error> {
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }

        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_items(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_items
                .lock()
                .expect("virtual list pending items lock poisoned")
                .clear();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_items(state: &VirtualListState) {
        loop {
            let items = std::mem::take(
                &mut *state
                    .pending_items
                    .lock()
                    .expect("virtual list pending items lock poisoned"),
            );
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }

            for (index, item) in items {
                match item {
                    PendingVirtualListItem::Basic(item) => {
                        let native_item = sys::OneUiListItemUtf8 {
                            title: sys::OneUiUtf8String::from_str(&item.title),
                            detail: sys::OneUiUtf8String::from_str(&item.detail),
                        };
                        unsafe {
                            sys::oneui_virtual_list_update_item_utf8(raw, index, &native_item);
                        }
                    }
                    PendingVirtualListItem::Rich(item) => {
                        let native_item = native_virtual_list_item(&item);
                        unsafe {
                            sys::oneui_virtual_list_update_rich_item_utf8(raw, index, &native_item);
                        }
                    }
                }
            }

            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_items
                .lock()
                .expect("virtual list pending items lock poisoned")
                .is_empty()
            {
                return;
            }
            if !state.update_scheduled.swap(true, Ordering::AcqRel) {
                continue;
            }
            return;
        }
    }
}

pub struct VirtualList {
    widget: Widget,
    pub(super) state: Arc<VirtualListState>,
    changed_callback: Option<Box<ListChangedCallback>>,
    selection_changed_callback: Option<Box<ListSelectionChangedCallback>>,
    activated_callback: Option<Box<ListChangedCallback>>,
    edit_requested_callback: Option<Box<ListChangedCallback>>,
    delete_requested_callback: Option<Box<ListSelectionChangedCallback>>,
    context_menu_requested_callback: Option<Box<ContextMenuRequestedCallback>>,
    reorder_requested_callback: Option<Box<ReorderRequestedCallback>>,
    item_drag_callback: Option<Box<ItemDragCallback>>,
}

impl VirtualList {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_virtual_list_create() })?;
        Ok(Self {
            state: Arc::new(VirtualListState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_items: Mutex::new(BTreeMap::new()),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
            changed_callback: None,
            selection_changed_callback: None,
            activated_callback: None,
            edit_requested_callback: None,
            delete_requested_callback: None,
            context_menu_requested_callback: None,
            reorder_requested_callback: None,
            item_drag_callback: None,
        })
    }

    pub fn set_items(&self, items: &[ListItem]) {
        // A full data revision invalidates queued row patches from the previous
        // revision. Keep the scheduled drain alive so updates submitted after
        // this reset still use the already queued UI-thread task.
        self.state
            .pending_items
            .lock()
            .expect("virtual list pending items lock poisoned")
            .clear();
        let native_items: Vec<sys::OneUiListItemUtf8> = items
            .iter()
            .map(|item| sys::OneUiListItemUtf8 {
                title: sys::OneUiUtf8String::from_str(&item.title),
                detail: sys::OneUiUtf8String::from_str(&item.detail),
            })
            .collect();
        unsafe {
            sys::oneui_virtual_list_set_items_utf8(
                self.widget.as_raw(),
                native_items.as_ptr(),
                native_items.len(),
            )
        };
    }

    pub fn set_rich_items(&self, items: &[VirtualListItem]) {
        self.state
            .pending_items
            .lock()
            .expect("virtual list pending items lock poisoned")
            .clear();
        let native_items = items
            .iter()
            .map(native_virtual_list_item)
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_virtual_list_set_rich_items_utf8(
                self.widget.as_raw(),
                native_items.as_ptr(),
                native_items.len(),
            )
        };
    }

    /// Replaces one row without resetting selection, scrolling, or motion.
    pub fn update_item(&self, index: usize, item: &ListItem) -> bool {
        let native_item = sys::OneUiListItemUtf8 {
            title: sys::OneUiUtf8String::from_str(&item.title),
            detail: sys::OneUiUtf8String::from_str(&item.detail),
        };
        unsafe {
            sys::oneui_virtual_list_update_item_utf8(self.widget.as_raw(), index, &native_item) != 0
        }
    }

    /// Replaces one operational row without resetting selection or scrolling.
    pub fn update_rich_item(&self, index: usize, item: &VirtualListItem) -> bool {
        let native_item = native_virtual_list_item(item);
        unsafe {
            sys::oneui_virtual_list_update_rich_item_utf8(self.widget.as_raw(), index, &native_item)
                != 0
        }
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_virtual_list_set_selected_index(self.widget.as_raw(), index) };
    }

    /// Clears the active row while preserving the list contents and scroll position.
    pub fn clear_selection(&self) {
        self.set_selected_index(-1);
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_virtual_list_selected_index(self.widget.as_raw()) }
    }

    pub fn set_selection_mode(&self, mode: SelectionMode) {
        let native_mode = match mode {
            SelectionMode::Single => 0,
            SelectionMode::Multiple => 1,
        };
        unsafe { sys::oneui_virtual_list_set_selection_mode(self.widget.as_raw(), native_mode) };
    }

    pub fn set_selected_indices(&self, indices: &[i32]) {
        unsafe {
            sys::oneui_virtual_list_set_selected_indices(
                self.widget.as_raw(),
                indices.as_ptr(),
                indices.len(),
            )
        };
    }

    pub fn selected_indices(&self) -> Vec<i32> {
        let count = unsafe {
            sys::oneui_virtual_list_selected_indices(self.widget.as_raw(), std::ptr::null_mut(), 0)
        };
        let mut values = vec![0; count];
        if count > 0 {
            unsafe {
                sys::oneui_virtual_list_selected_indices(
                    self.widget.as_raw(),
                    values.as_mut_ptr(),
                    values.len(),
                )
            };
        }
        values
    }

    pub fn set_row_height(&self, height: f32) {
        unsafe { sys::oneui_virtual_list_set_row_height(self.widget.as_raw(), height) };
    }

    /// Opt in to single-click activation for completion menus. Default is double click.
    pub fn set_activate_on_single_click(&self, enabled: bool) {
        unsafe {
            sys::oneui_virtual_list_set_activate_on_single_click(
                self.widget.as_raw(),
                i32::from(enabled),
            )
        };
    }

    pub fn set_rich_metrics(&self, metrics: VirtualListRichMetrics) {
        unsafe {
            sys::oneui_virtual_list_set_rich_metrics(
                self.widget.as_raw(),
                metrics.indicator_space,
                metrics.indicator_diameter,
                metrics.badge_height,
                metrics.badge_radius,
                metrics.badge_horizontal_padding,
                metrics.title_badge_gap,
                metrics.trailing_width,
                metrics.trailing_gap,
            )
        };
    }

    pub fn rich_metrics(&self) -> VirtualListRichMetrics {
        let mut metrics = sys::OneUiVirtualListRichMetrics::default();
        let read =
            unsafe { sys::oneui_virtual_list_rich_metrics(self.widget.as_raw(), &mut metrics) };
        if read == 0 {
            return VirtualListRichMetrics::default();
        }
        VirtualListRichMetrics {
            indicator_space: metrics.indicator_space,
            indicator_diameter: metrics.indicator_diameter,
            badge_height: metrics.badge_height,
            badge_radius: metrics.badge_radius,
            badge_horizontal_padding: metrics.badge_horizontal_padding,
            title_badge_gap: metrics.title_badge_gap,
            trailing_width: metrics.trailing_width,
            trailing_gap: metrics.trailing_gap,
        }
    }

    pub fn set_scroll_offset(&self, offset: f32) {
        unsafe { sys::oneui_virtual_list_set_scroll_offset(self.widget.as_raw(), offset) };
    }

    pub fn scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_virtual_list_scroll_offset(self.widget.as_raw()) }
    }

    pub fn max_scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_virtual_list_max_scroll_offset(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("VirtualList", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("virtual list callback was just installed")
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_changed(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_selection_changed<F>(&mut self, callback: F)
    where
        F: FnMut(Vec<i32>) + 'static,
    {
        let trace = InteractionTrace::at(
            "VirtualList",
            "selection_changed",
            std::panic::Location::caller(),
        );
        self.clear_on_selection_changed();
        self.selection_changed_callback = Some(Box::new(ListSelectionChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .selection_changed_callback
            .as_deref_mut()
            .expect("virtual list selection callback was just installed")
            as *mut ListSelectionChangedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_selection_changed(
                self.widget.as_raw(),
                Some(run_list_selection_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_selection_changed(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_selection_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.selection_changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_activated<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace =
            InteractionTrace::at("VirtualList", "activated", std::panic::Location::caller());
        self.clear_on_activated();
        self.activated_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .activated_callback
            .as_deref_mut()
            .expect("virtual list activation callback was just installed")
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_activated(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_activated(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_activated(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.activated_callback = None;
    }

    #[track_caller]
    pub fn set_on_edit_requested<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at(
            "VirtualList",
            "edit_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_edit_requested();
        self.edit_requested_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .edit_requested_callback
            .as_deref_mut()
            .expect("virtual list edit callback was just installed")
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_edit_requested(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_edit_requested(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_edit_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.edit_requested_callback = None;
    }

    /// Runs the standard Delete command with the complete current selection.
    #[track_caller]
    pub fn set_on_delete_requested<F>(&mut self, callback: F)
    where
        F: FnMut(Vec<i32>) + 'static,
    {
        let trace = InteractionTrace::at(
            "VirtualList",
            "delete_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_delete_requested();
        self.delete_requested_callback = Some(Box::new(ListSelectionChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .delete_requested_callback
            .as_deref_mut()
            .expect("virtual list delete callback was just installed")
            as *mut ListSelectionChangedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_delete_requested(
                self.widget.as_raw(),
                Some(run_list_selection_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_delete_requested(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_delete_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.delete_requested_callback = None;
    }

    #[track_caller]
    pub fn set_on_context_menu_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ContextMenuRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "VirtualList",
            "context_menu_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_context_menu_requested();
        self.context_menu_requested_callback = Some(Box::new(ContextMenuRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .context_menu_requested_callback
            .as_deref_mut()
            .expect("virtual list context menu callback was just installed")
            as *mut ContextMenuRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_context_menu_requested(
                self.widget.as_raw(),
                Some(run_context_menu_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_context_menu_requested(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_context_menu_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.context_menu_requested_callback = None;
    }

    /// Enables pointer and `Alt+Up`/`Alt+Down` reorder requests.
    pub fn set_reorder_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_virtual_list_set_reorder_enabled(self.widget.as_raw(), i32::from(enabled))
        };
    }

    pub fn reorder_enabled(&self) -> bool {
        unsafe { sys::oneui_virtual_list_reorder_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_reorder_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ReorderRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "VirtualList",
            "reorder_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_reorder_requested();
        self.reorder_requested_callback = Some(Box::new(ReorderRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .reorder_requested_callback
            .as_deref_mut()
            .expect("virtual list reorder callback was just installed")
            as *mut ReorderRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_reorder_requested(
                self.widget.as_raw(),
                Some(run_reorder_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_reorder_requested(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_reorder_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.reorder_requested_callback = None;
    }

    /// Assigns stable domain identifiers to the current rows for external drag operations.
    ///
    /// The identifiers must be non-empty, unique, and exactly match the current row count.
    /// A full [`Self::set_items`] reset clears them so stale identities cannot be emitted.
    pub fn set_item_drag_ids(&self, ids: &[String]) -> bool {
        let native_ids: Vec<sys::OneUiUtf8String> = ids
            .iter()
            .map(|id| sys::OneUiUtf8String::from_str(id))
            .collect();
        unsafe {
            sys::oneui_virtual_list_set_item_drag_ids_utf8(
                self.widget.as_raw(),
                native_ids.as_ptr(),
                native_ids.len(),
            ) != 0
        }
    }

    pub fn set_item_drag_enabled(&self, enabled: bool) {
        unsafe {
            sys::oneui_virtual_list_set_item_drag_enabled(self.widget.as_raw(), i32::from(enabled))
        };
    }

    pub fn item_drag_enabled(&self) -> bool {
        unsafe { sys::oneui_virtual_list_item_drag_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_item_drag<F>(&mut self, callback: F)
    where
        F: FnMut(ItemDragEvent) + 'static,
    {
        let trace =
            InteractionTrace::at("VirtualList", "item_drag", std::panic::Location::caller());
        self.clear_on_item_drag();
        self.item_drag_callback = Some(Box::new(ItemDragCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .item_drag_callback
            .as_deref_mut()
            .expect("virtual list item drag callback was just installed")
            as *mut ItemDragCallback)
            .cast();
        unsafe {
            sys::oneui_virtual_list_set_on_item_drag_utf8(
                self.widget.as_raw(),
                Some(run_item_drag_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_item_drag(&mut self) {
        unsafe {
            sys::oneui_virtual_list_set_on_item_drag_utf8(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.item_drag_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for VirtualList {
    fn drop(&mut self) {
        self.clear_on_item_drag();
        self.clear_on_reorder_requested();
        self.clear_on_context_menu_requested();
        self.clear_on_delete_requested();
        self.clear_on_edit_requested();
        self.clear_on_activated();
        self.clear_on_selection_changed();
        self.clear_on_changed();
        self.state
            .raw
            .store(std::ptr::null_mut(), Ordering::Release);
        self.state
            .pending_items
            .lock()
            .expect("virtual list pending items lock poisoned")
            .clear();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}
