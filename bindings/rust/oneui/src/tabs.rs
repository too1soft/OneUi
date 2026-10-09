use crate::*;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TabEditReason {
    Submitted,
    Cancelled,
    FocusLost,
}

struct TabEditCallback {
    handler: Box<dyn FnMut(i32, String, TabEditReason) + 'static>,
}
unsafe extern "C" fn run_tab_edit_callback(
    index: i32,
    text: *const std::ffi::c_char,
    length: usize,
    reason: i32,
    user_data: *mut std::ffi::c_void,
) {
    if text.is_null() || user_data.is_null() {
        return;
    }
    let reason = match reason {
        0 => TabEditReason::Submitted,
        1 => TabEditReason::Cancelled,
        2 => TabEditReason::FocusLost,
        _ => return,
    };
    let bytes = unsafe { std::slice::from_raw_parts(text.cast::<u8>(), length) };
    let value = String::from_utf8_lossy(bytes).into_owned();
    let callback = unsafe { &mut *user_data.cast::<TabEditCallback>() };
    run_callback_guarded("tabs.edit_finished", || {
        (callback.handler)(index, value, reason)
    });
}

/// Keyboard-accessible tab strip for switching between peer workspace views.
pub struct Tabs {
    pub(crate) widget: Widget,
    overflow_callback: Option<Box<IndexChangedCallback>>,
    changed_callback: Option<Box<IndexChangedCallback>>,
    close_requested_callback: Option<Box<IndexChangedCallback>>,
    context_menu_requested_callback: Option<Box<ContextMenuRequestedCallback>>,
    reorder_requested_callback: Option<Box<ReorderRequestedCallback>>,
    edit_callback: Option<Box<TabEditCallback>>,
    detach_callback: Option<Box<ContextMenuRequestedCallback>>,
    drag_out_callback: Option<Box<TabDragOutCallback>>,
}

/// A tab dragged out of its strip. Coordinates share the window coordinate
/// space of widget frames, so they can be hit-tested against other widgets.
#[derive(Clone, Copy, Debug)]
pub struct TabDragOutEvent {
    pub index: usize,
    pub phase: ItemDragPhase,
    pub x: f32,
    pub y: f32,
}

struct TabDragOutCallback {
    handler: Box<dyn FnMut(TabDragOutEvent)>,
}

unsafe extern "C" fn run_tab_drag_out_callback(
    index: std::ffi::c_int,
    phase: std::ffi::c_int,
    x: f32,
    y: f32,
    data: *mut std::ffi::c_void,
) {
    if data.is_null() || index < 0 {
        return;
    }
    let event = TabDragOutEvent {
        index: index as usize,
        phase: ItemDragPhase::from_raw(phase),
        x,
        y,
    };
    let callback = unsafe { &mut *data.cast::<TabDragOutCallback>() };
    run_callback_guarded("tabs.drag_out", || (callback.handler)(event));
}

impl Tabs {
    pub fn new(items: &[String]) -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_tabs_create() })?;
        let control = Self {
            widget,
            overflow_callback: None,
            changed_callback: None,
            close_requested_callback: None,
            context_menu_requested_callback: None,
            reorder_requested_callback: None,
            edit_callback: None,
            detach_callback: None,
            drag_out_callback: None,
        };
        control.set_items(items);
        Ok(control)
    }

    /// Start editing the visible label. Focus the Tabs widget through Window or
    /// UiDispatcher after this call. Changed items cancel an outstanding edit.
    pub fn begin_edit(&self, index: i32) -> bool {
        unsafe { sys::oneui_tabs_begin_edit(self.widget.as_raw(), index) != 0 }
    }
    pub fn cancel_edit(&self) {
        unsafe { sys::oneui_tabs_cancel_edit(self.widget.as_raw()) };
    }
    pub fn editing_index(&self) -> Option<usize> {
        usize::try_from(unsafe { sys::oneui_tabs_editing_index(self.widget.as_raw()) }).ok()
    }
    pub fn set_on_edit_finished<F>(&mut self, callback: F)
    where
        F: FnMut(i32, String, TabEditReason) + 'static,
    {
        self.clear_on_edit_finished();
        self.edit_callback = Some(Box::new(TabEditCallback {
            handler: Box::new(callback),
        }));
        let data = (self.edit_callback.as_deref_mut().unwrap() as *mut TabEditCallback).cast();
        unsafe {
            sys::oneui_tabs_set_on_edit_finished(
                self.widget.as_raw(),
                Some(run_tab_edit_callback),
                data,
            )
        };
    }
    pub fn clear_on_edit_finished(&mut self) {
        unsafe {
            sys::oneui_tabs_set_on_edit_finished(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.edit_callback = None;
    }

    pub fn set_items(&self, items: &[String]) {
        let views: Vec<sys::OneUiUtf8String> = items
            .iter()
            .map(|item| sys::OneUiUtf8String::from_str(item))
            .collect();
        unsafe {
            sys::oneui_tabs_set_items_utf8(self.widget.as_raw(), views.as_ptr(), views.len())
        };
    }

    /// Sets an optional leading icon for each tab. Missing entries and `None`
    /// keep the corresponding tab text-only.
    pub fn set_item_icons(&self, icons: &[Option<IconSymbol>]) {
        let symbols = icons
            .iter()
            .map(|icon| icon.map_or(-1, |symbol| symbol as i32))
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_tabs_set_item_icons(self.widget.as_raw(), symbols.as_ptr(), symbols.len())
        };
    }

    /// Opt in to document chrome: reserved 24px close targets, 13px glyphs,
    /// persistent status indicators, separators and a selected underline.
    pub fn set_document_mode(&self, enabled: bool) {
        unsafe { sys::oneui_tabs_set_document_mode(self.widget.as_raw(), i32::from(enabled)) };
    }
    /// Number of items outside the most recently measured viewport.
    pub fn overflow_item_count(&self) -> usize {
        unsafe { sys::oneui_tabs_overflow_item_count(self.widget.as_raw()).max(0) as usize }
    }
    /// Reports measured overflow after layout; enqueue any geometry mutations.
    pub fn set_on_overflow_changed<F: FnMut(i32) + 'static>(&mut self, callback: F) {
        unsafe {
            sys::oneui_tabs_set_on_overflow_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.overflow_callback = Some(Box::new(IndexChangedCallback {
            handler: Box::new(callback),
        }));
        let data =
            (self.overflow_callback.as_deref_mut().unwrap() as *mut IndexChangedCallback).cast();
        unsafe {
            sys::oneui_tabs_set_on_overflow_changed(
                self.widget.as_raw(),
                Some(run_index_changed_callback),
                data,
            )
        };
    }
    /// Per-item status colors; missing or transparent entries draw no dot.
    pub fn set_item_status_colors(&self, colors: &[Color]) {
        let values: Vec<sys::OneUiColor> = colors.iter().copied().map(Into::into).collect();
        unsafe {
            sys::oneui_tabs_set_item_status_colors(
                self.widget.as_raw(),
                values.as_ptr(),
                values.len(),
            )
        };
    }
    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_tabs_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_tabs_selected_index(self.widget.as_raw()) }
    }

    /// Uses content-oriented tab widths with bounded horizontal overflow.
    pub fn set_compact(&self, compact: bool) {
        unsafe { sys::oneui_tabs_set_compact(self.widget.as_raw(), i32::from(compact)) };
    }

    pub fn set_item_width_range(&self, minimum: f32, maximum: f32) {
        unsafe { sys::oneui_tabs_set_item_width_range(self.widget.as_raw(), minimum, maximum) };
    }

    pub fn set_closable(&self, closable: bool) {
        unsafe { sys::oneui_tabs_set_closable(self.widget.as_raw(), i32::from(closable)) };
    }

    pub fn set_reorder_enabled(&self, enabled: bool) {
        unsafe { sys::oneui_tabs_set_reorder_enabled(self.widget.as_raw(), i32::from(enabled)) };
    }

    pub fn reorder_enabled(&self) -> bool {
        unsafe { sys::oneui_tabs_reorder_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Tabs", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(IndexChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .changed_callback
            .as_deref_mut()
            .expect("tabs callback was just installed")
            as *mut IndexChangedCallback)
            .cast();
        unsafe {
            sys::oneui_tabs_set_on_changed(
                self.widget.as_raw(),
                Some(run_index_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe { sys::oneui_tabs_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut()) };
        self.changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_close_requested<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Tabs", "close_requested", std::panic::Location::caller());
        self.clear_on_close_requested();
        self.close_requested_callback = Some(Box::new(IndexChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .close_requested_callback
            .as_deref_mut()
            .expect("tabs close callback was just installed")
            as *mut IndexChangedCallback)
            .cast();
        unsafe {
            sys::oneui_tabs_set_on_close_requested(
                self.widget.as_raw(),
                Some(run_index_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_close_requested(&mut self) {
        unsafe {
            sys::oneui_tabs_set_on_close_requested(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.close_requested_callback = None;
    }

    #[track_caller]
    pub fn set_on_context_menu_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ContextMenuRequest) + 'static,
    {
        let trace = InteractionTrace::at(
            "Tabs",
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
            .expect("tabs context menu callback was just installed")
            as *mut ContextMenuRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_tabs_set_on_context_menu_requested(
                self.widget.as_raw(),
                Some(run_context_menu_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_context_menu_requested(&mut self) {
        unsafe {
            sys::oneui_tabs_set_on_context_menu_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.context_menu_requested_callback = None;
    }

    #[track_caller]
    pub fn set_on_reorder_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ReorderRequest) + 'static,
    {
        let trace =
            InteractionTrace::at("Tabs", "reorder_requested", std::panic::Location::caller());
        self.clear_on_reorder_requested();
        self.reorder_requested_callback = Some(Box::new(ReorderRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self
            .reorder_requested_callback
            .as_deref_mut()
            .expect("tabs reorder callback was just installed")
            as *mut ReorderRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_tabs_set_on_reorder_requested(
                self.widget.as_raw(),
                Some(run_reorder_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_reorder_requested(&mut self) {
        unsafe {
            sys::oneui_tabs_set_on_reorder_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.reorder_requested_callback = None;
    }

    /// Starts a native drag from a pointer gesture outside the strip.
    pub fn set_on_detach_requested(&mut self, callback: impl FnMut(ContextMenuRequest) + 'static) {
        self.clear_on_detach_requested();
        self.detach_callback = Some(Box::new(ContextMenuRequestedCallback {
            handler: Box::new(callback),
        }));
        let data = (self.detach_callback.as_deref_mut().unwrap()
            as *mut ContextMenuRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_tabs_set_on_detach_requested(
                self.widget.as_raw(),
                Some(run_context_menu_requested_callback),
                data,
            );
        }
    }
    /// Hands a tab dragged outside the strip to the owner until release, for
    /// docking it inside the window. Takes precedence over detach requests.
    pub fn set_on_tab_drag_out(&mut self, callback: impl FnMut(TabDragOutEvent) + 'static) {
        self.clear_on_tab_drag_out();
        self.drag_out_callback = Some(Box::new(TabDragOutCallback {
            handler: Box::new(callback),
        }));
        let data = (self.drag_out_callback.as_deref_mut().unwrap() as *mut TabDragOutCallback)
            .cast();
        unsafe {
            sys::oneui_tabs_set_on_drag_out(
                self.widget.as_raw(),
                Some(run_tab_drag_out_callback),
                data,
            );
        }
    }
    pub fn clear_on_tab_drag_out(&mut self) {
        unsafe {
            sys::oneui_tabs_set_on_drag_out(self.widget.as_raw(), None, std::ptr::null_mut());
        }
        self.drag_out_callback = None;
    }
    pub fn clear_on_detach_requested(&mut self) {
        unsafe {
            sys::oneui_tabs_set_on_detach_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            );
        }
        self.detach_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Tabs {
    fn drop(&mut self) {
        unsafe {
            sys::oneui_tabs_set_on_overflow_changed(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.clear_on_detach_requested();
        self.clear_on_tab_drag_out();
        self.clear_on_edit_finished();
        self.clear_on_changed();
        self.clear_on_close_requested();
        self.clear_on_context_menu_requested();
        self.clear_on_reorder_requested();
    }
}
