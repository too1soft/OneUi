//! Table bindings.

use super::list::{
    run_context_menu_requested_callback, run_list_changed_callback,
    run_list_selection_changed_callback, run_reorder_requested_callback, ContextMenuRequest,
    ContextMenuRequestedCallback, ListChangedCallback, ListSelectionChangedCallback,
    ReorderRequest, ReorderRequestedCallback,
};
use super::pointer_surface::{run_item_drag_callback, ItemDragCallback, ItemDragEvent};
use super::visual_style::{Color, LabelAlign};
use super::{
    run_callback_guarded, sys, traced_value_callback, Arc, AtomicBool, AtomicPtr, BTreeMap, Error,
    IconSymbol, InteractionTrace, Mutex, Ordering, SelectionMode, UiDispatcher, Widget,
};

impl UiDispatcher {
    /// Creates a thread-safe producer for table data revisions and row updates.
    pub fn table_handle(&self, table: &Table) -> TableHandle {
        TableHandle {
            state: Arc::clone(&table.state),
            dispatcher: self.clone(),
        }
    }
}

/// A column in a native table. A width of `0` consumes the remaining space.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct TableColumn {
    pub header: String,
    pub width: f32,
}

/// Rendering is explicit and independent of localized column headings.
#[derive(Debug, Clone, Copy, Default, Eq, PartialEq)]
#[repr(i32)]
pub enum TableCellKind {
    #[default]
    Text = 0,
    Percent = 1,
}

/// Structured UTF-8 data for one native table row.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct TableRow {
    pub cells: Vec<String>,
}

struct TableState {
    raw: AtomicPtr<sys::OneUiWidget>,
    pending_rows: Mutex<BTreeMap<usize, TableRow>>,
    update_scheduled: AtomicBool,
}

/// Thread-safe producer for table data owned by a window thread.
#[derive(Clone)]
pub struct TableHandle {
    state: Arc<TableState>,
    dispatcher: UiDispatcher,
}

impl TableHandle {
    /// Publishes keyed rows only while the producer is current. Selection is
    /// remapped by key on the UI thread, never by a stale row index.
    pub fn set_rich_rows_keyed<F, C, O>(
        &self,
        rows: Vec<TableRichRow>,
        old_keys: O,
        keys: Vec<String>,
        current: F,
        commit: C,
    ) -> Result<(), Error>
    where
        F: Fn() -> bool + Send + 'static,
        C: FnOnce() + Send + 'static,
        O: FnOnce() -> Vec<String> + Send + 'static,
    {
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() || !current() {
                return;
            }
            state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
                .clear();
            let old_keys = old_keys();
            let count = unsafe { sys::oneui_table_selected_indices(raw, std::ptr::null_mut(), 0) };
            let mut selected = vec![0i32; count];
            unsafe {
                sys::oneui_table_selected_indices(raw, selected.as_mut_ptr(), selected.len());
            }
            let selected = selected
                .into_iter()
                .filter_map(|i| old_keys.get(i as usize))
                .filter_map(|key| keys.iter().position(|v| v == key))
                .map(|i| i as i32)
                .collect::<Vec<_>>();
            let offset = unsafe { sys::oneui_table_scroll_offset(raw) };
            let cells = rows.iter().map(rich_table_row_cells).collect::<Vec<_>>();
            let values = cells
                .iter()
                .map(|cells| sys::OneUiTableRichRowUtf8 {
                    cells: cells.as_ptr(),
                    count: cells.len(),
                })
                .collect::<Vec<_>>();
            commit();
            unsafe {
                sys::oneui_table_set_rich_rows_utf8(raw, values.as_ptr(), values.len());
                sys::oneui_table_set_selected_indices(raw, selected.as_ptr(), selected.len());
                sys::oneui_table_set_scroll_offset(raw, offset);
            }
        })
    }

    pub fn set_header_sort(&self, column: i32, descending: bool) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if !raw.is_null() {
                unsafe {
                    sys::oneui_table_set_header_sort(raw, column, i32::from(descending));
                }
            }
        })
    }

    /// Publishes a rich revision atomically, retaining selection only when row
    /// identities are unchanged. Cell strings remain owned until the C call returns.
    pub fn set_rich_rows(
        &self,
        rows: Vec<TableRichRow>,
        preserve_selection: bool,
    ) -> Result<(), Error> {
        self.set_rich_rows_with_commit(rows, move || preserve_selection)
    }

    /// Commits the associated row model on the UI thread immediately before
    /// publishing rows. Return true only when selected indices keep identity.
    /// The callback is not run if the table has been destroyed.
    pub fn set_rich_rows_with_commit<F>(
        &self,
        rows: Vec<TableRichRow>,
        commit: F,
    ) -> Result<(), Error>
    where
        F: FnOnce() -> bool + Send + 'static,
    {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
                .clear();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            let cells = rows.iter().map(rich_table_row_cells).collect::<Vec<_>>();
            let values = cells
                .iter()
                .map(|cells| sys::OneUiTableRichRowUtf8 {
                    cells: cells.as_ptr(),
                    count: cells.len(),
                })
                .collect::<Vec<_>>();
            let preserve_selection = commit();
            unsafe {
                sys::oneui_table_set_rich_rows_utf8(raw, values.as_ptr(), values.len());
                if !preserve_selection {
                    sys::oneui_table_set_selected_indices(raw, std::ptr::null(), 0);
                }
            }
        })
    }

    /// Replaces the column model on the window thread. This is useful for
    /// result grids whose schema is produced by a background query.
    pub fn set_columns(&self, columns: Vec<TableColumn>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            set_table_columns_raw(raw, &columns);
        })
    }

    pub fn set_rows(&self, rows: Vec<TableRow>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
                .clear();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            set_table_rows_raw(raw, &rows);
        })
    }

    /// Replaces the current revision and clears index-based selection in the
    /// same window-thread transaction. Use this when row identity changed so
    /// commands cannot accidentally target a different row that inherited the
    /// previous index.
    pub fn set_rows_and_clear_selection(&self, rows: Vec<TableRow>) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
                .clear();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            set_table_rows_raw(raw, &rows);
            unsafe {
                sys::oneui_table_set_selected_indices(raw, std::ptr::null(), 0);
            }
        })
    }

    /// Replaces the current revision and selects one row in the same
    /// window-thread transaction. This prevents a queued row refresh from
    /// clearing a newer logical selection made by the producer.
    pub fn set_rows_and_selected_index(
        &self,
        rows: Vec<TableRow>,
        index: i32,
    ) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        let state = Arc::clone(&self.state);
        self.dispatcher.dispatch(move || {
            state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
                .clear();
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                return;
            }
            set_table_rows_raw(raw, &rows);
            unsafe {
                sys::oneui_table_set_selected_index(raw, index);
            }
        })
    }

    pub fn update_row(&self, index: usize, row: TableRow) -> Result<(), Error> {
        if self.state.raw.load(Ordering::Acquire).is_null() {
            return Err(Error::WidgetDestroyed);
        }
        self.state
            .pending_rows
            .lock()
            .expect("table pending rows lock poisoned")
            .insert(index, row);
        if self.state.update_scheduled.swap(true, Ordering::AcqRel) {
            return Ok(());
        }
        let state = Arc::clone(&self.state);
        let dispatched_state = Arc::clone(&state);
        if let Err(error) = self
            .dispatcher
            .dispatch(move || Self::drain_pending_rows(&dispatched_state))
        {
            state.update_scheduled.store(false, Ordering::Release);
            state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
                .clear();
            return Err(error);
        }
        Ok(())
    }

    fn drain_pending_rows(state: &TableState) {
        loop {
            let rows = std::mem::take(
                &mut *state
                    .pending_rows
                    .lock()
                    .expect("table pending rows lock poisoned"),
            );
            let raw = state.raw.load(Ordering::Acquire);
            if raw.is_null() {
                state.update_scheduled.store(false, Ordering::Release);
                return;
            }
            for (index, row) in rows {
                update_table_row_raw(raw, index, &row);
            }
            state.update_scheduled.store(false, Ordering::Release);
            if state
                .pending_rows
                .lock()
                .expect("table pending rows lock poisoned")
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

fn set_table_columns_raw(raw: *mut sys::OneUiWidget, columns: &[TableColumn]) {
    let native_columns = columns
        .iter()
        .map(|column| sys::OneUiTableColumnUtf8 {
            header: sys::OneUiUtf8String::from_str(&column.header),
            width: column.width,
        })
        .collect::<Vec<_>>();
    unsafe {
        sys::oneui_table_set_columns_utf8(raw, native_columns.as_ptr(), native_columns.len())
    };
}

fn set_table_rows_raw(raw: *mut sys::OneUiWidget, rows: &[TableRow]) {
    let native_cells = rows
        .iter()
        .map(|row| {
            row.cells
                .iter()
                .map(|cell| sys::OneUiUtf8String::from_str(cell))
                .collect::<Vec<_>>()
        })
        .collect::<Vec<_>>();
    let native_rows = native_cells
        .iter()
        .map(|cells| sys::OneUiTableRowUtf8 {
            cells: cells.as_ptr(),
            cell_count: cells.len(),
        })
        .collect::<Vec<_>>();
    unsafe { sys::oneui_table_set_rows_utf8(raw, native_rows.as_ptr(), native_rows.len()) };
}

fn update_table_row_raw(raw: *mut sys::OneUiWidget, index: usize, row: &TableRow) -> bool {
    let cells = row
        .cells
        .iter()
        .map(|cell| sys::OneUiUtf8String::from_str(cell))
        .collect::<Vec<_>>();
    let native_row = sys::OneUiTableRowUtf8 {
        cells: cells.as_ptr(),
        cell_count: cells.len(),
    };
    unsafe { sys::oneui_table_update_row_utf8(raw, index, &native_row) != 0 }
}

#[derive(Clone, Debug)]
pub struct TableRichCell {
    pub text: String,
    pub detail: String,
    pub badge: String,
    pub badge_foreground: Option<Color>,
    pub badge_background: Option<Color>,
    pub alignment: LabelAlign,
    pub icon: Option<IconSymbol>,
    pub foreground: Option<Color>,
    pub font_size: f32,
    pub font_weight: i32,
}
impl Default for TableRichCell {
    fn default() -> Self {
        Self {
            badge_foreground: None,
            badge_background: None,
            text: String::new(),
            detail: String::new(),
            badge: String::new(),
            alignment: LabelAlign::Start,
            icon: None,
            foreground: None,
            font_size: 0.0,
            font_weight: 400,
        }
    }
}
#[derive(Clone, Debug, Default)]
pub struct TableRichRow {
    pub cells: Vec<TableRichCell>,
}
fn rich_table_row_cells(row: &TableRichRow) -> Vec<sys::OneUiTableRichCellUtf8> {
    row.cells
        .iter()
        .map(|c| sys::OneUiTableRichCellUtf8 {
            text: sys::OneUiUtf8String::from_str(&c.text),
            detail: sys::OneUiUtf8String::from_str(&c.detail),
            badge: sys::OneUiUtf8String::from_str(&c.badge),
            badge_foreground: c
                .badge_foreground
                .unwrap_or(Color {
                    r: 0,
                    g: 0,
                    b: 0,
                    a: 0,
                })
                .into(),
            badge_background: c
                .badge_background
                .unwrap_or(Color {
                    r: 0,
                    g: 0,
                    b: 0,
                    a: 0,
                })
                .into(),
            badge_color_flags: u32::from(c.badge_foreground.is_some())
                | (u32::from(c.badge_background.is_some()) << 1),
            alignment: match c.alignment {
                LabelAlign::Start => 0,
                LabelAlign::Center => 1,
                LabelAlign::End => 2,
            },
            icon: c.icon.map(|s| s as i32).unwrap_or(-1),
            font_size: c.font_size,
            font_weight: c.font_weight,
            foreground: c
                .foreground
                .unwrap_or(Color {
                    r: 0,
                    g: 0,
                    b: 0,
                    a: 0,
                })
                .into(),
        })
        .collect()
}
struct TableCellActionCallback {
    handler: Box<dyn FnMut(i32, i32)>,
}
unsafe extern "C" fn run_table_cell_action(row: i32, column: i32, data: *mut std::ffi::c_void) {
    if data.is_null() {
        return;
    }
    let callback = unsafe { &mut *data.cast::<TableCellActionCallback>() };
    run_callback_guarded("table.cell_action", || (callback.handler)(row, column));
}

/// A fixed-row-height, viewport-virtualized native data table.
pub struct Table {
    widget: Widget,
    cell_action_callback: Option<Box<TableCellActionCallback>>,
    state: Arc<TableState>,
    changed_callback: Option<Box<ListChangedCallback>>,
    selection_changed_callback: Option<Box<ListSelectionChangedCallback>>,
    activated_callback: Option<Box<ListChangedCallback>>,
    edit_requested_callback: Option<Box<ListChangedCallback>>,
    delete_requested_callback: Option<Box<ListSelectionChangedCallback>>,
    context_menu_requested_callback: Option<Box<ContextMenuRequestedCallback>>,
    reorder_requested_callback: Option<Box<ReorderRequestedCallback>>,
    item_drag_callback: Option<Box<ItemDragCallback>>,
}

impl Table {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_table_create() })?;
        Ok(Self {
            state: Arc::new(TableState {
                raw: AtomicPtr::new(widget.as_raw()),
                pending_rows: Mutex::new(BTreeMap::new()),
                update_scheduled: AtomicBool::new(false),
            }),
            widget,
            cell_action_callback: None,
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

    pub fn set_rich_rows(&self, rows: &[TableRichRow]) {
        self.state
            .pending_rows
            .lock()
            .expect("table pending rows lock poisoned")
            .clear();
        let cells = rows.iter().map(rich_table_row_cells).collect::<Vec<_>>();
        let values = cells
            .iter()
            .map(|c| sys::OneUiTableRichRowUtf8 {
                cells: c.as_ptr(),
                count: c.len(),
            })
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_table_set_rich_rows_utf8(self.widget.as_raw(), values.as_ptr(), values.len())
        };
    }
    pub fn update_rich_row(&self, index: usize, row: &TableRichRow) -> bool {
        let cells = rich_table_row_cells(row);
        let native = sys::OneUiTableRichRowUtf8 {
            cells: cells.as_ptr(),
            count: cells.len(),
        };
        unsafe { sys::oneui_table_update_rich_row_utf8(self.widget.as_raw(), index, &native) != 0 }
    }
    pub fn set_column_presentation(&self, column: i32, alignment: LabelAlign, action: bool) {
        let align = match alignment {
            LabelAlign::Start => 0,
            LabelAlign::Center => 1,
            LabelAlign::End => 2,
        };
        unsafe {
            sys::oneui_table_set_column_presentation(
                self.widget.as_raw(),
                column,
                align,
                i32::from(action),
            )
        };
    }
    pub fn set_column_kind(&self, column: i32, kind: TableCellKind) {
        // SAFETY: the owned widget stays alive for this synchronous UI-thread
        // call. The C API validates column bounds and never retains a pointer.
        unsafe { sys::oneui_table_set_column_kind(self.widget.as_raw(), column, kind as i32) };
    }
    pub fn set_column_dividers_visible(&self, visible: bool) {
        unsafe {
            sys::oneui_table_set_column_dividers_visible(self.widget.as_raw(), i32::from(visible))
        };
    }
    pub fn set_selection_column_visible(&self, visible: bool) {
        unsafe {
            sys::oneui_table_set_selection_column_visible(self.widget.as_raw(), i32::from(visible))
        };
    }
    /// Width of the checkbox gutter; finite values are clamped to 24..128.
    pub fn set_selection_column_width(&self, width: f32) {
        unsafe { sys::oneui_table_set_selection_column_width(self.widget.as_raw(), width) };
    }
    /// Sortable headers call the cell action callback with row = -1.
    pub fn set_header_sort(&self, column: i32, descending: bool) {
        unsafe {
            sys::oneui_table_set_header_sort(self.widget.as_raw(), column, i32::from(descending));
        }
    }
    pub fn set_header_height(&self, height: f32) {
        unsafe { sys::oneui_table_set_header_height(self.widget.as_raw(), height) };
    }
    /// Pointer release inside an action cell, or Space on the selected row.
    pub fn set_on_cell_action<F: FnMut(i32, i32) + 'static>(&mut self, callback: F) {
        self.clear_on_cell_action();
        self.cell_action_callback = Some(Box::new(TableCellActionCallback {
            handler: Box::new(callback),
        }));
        let data = (self.cell_action_callback.as_deref_mut().unwrap()
            as *mut TableCellActionCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_cell_action(
                self.widget.as_raw(),
                Some(run_table_cell_action),
                data,
            )
        };
    }
    pub fn clear_on_cell_action(&mut self) {
        unsafe {
            sys::oneui_table_set_on_cell_action(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.cell_action_callback = None;
    }

    pub fn set_columns(&self, columns: &[TableColumn]) {
        set_table_columns_raw(self.widget.as_raw(), columns);
    }

    pub fn set_rows(&self, rows: &[TableRow]) {
        self.state
            .pending_rows
            .lock()
            .expect("table pending rows lock poisoned")
            .clear();
        set_table_rows_raw(self.widget.as_raw(), rows);
    }

    pub fn update_row(&self, index: usize, row: &TableRow) -> bool {
        update_table_row_raw(self.widget.as_raw(), index, row)
    }

    pub fn set_selection_mode(&self, mode: SelectionMode) {
        let mode = match mode {
            SelectionMode::Single => 0,
            SelectionMode::Multiple => 1,
        };
        unsafe { sys::oneui_table_set_selection_mode(self.widget.as_raw(), mode) };
    }

    pub fn set_selected_index(&self, index: i32) {
        unsafe { sys::oneui_table_set_selected_index(self.widget.as_raw(), index) };
    }

    pub fn selected_index(&self) -> i32 {
        unsafe { sys::oneui_table_selected_index(self.widget.as_raw()) }
    }

    pub fn set_selected_indices(&self, indices: &[i32]) {
        unsafe {
            sys::oneui_table_set_selected_indices(
                self.widget.as_raw(),
                indices.as_ptr(),
                indices.len(),
            )
        };
    }

    pub fn selected_indices(&self) -> Vec<i32> {
        let count = unsafe {
            sys::oneui_table_selected_indices(self.widget.as_raw(), std::ptr::null_mut(), 0)
        };
        let mut values = vec![0; count];
        if count > 0 {
            unsafe {
                sys::oneui_table_selected_indices(
                    self.widget.as_raw(),
                    values.as_mut_ptr(),
                    values.len(),
                )
            };
        }
        values
    }

    pub fn set_row_height(&self, height: f32) {
        unsafe { sys::oneui_table_set_row_height(self.widget.as_raw(), height) };
    }

    pub fn set_scroll_offset(&self, offset: f32) {
        unsafe { sys::oneui_table_set_scroll_offset(self.widget.as_raw(), offset) };
    }

    pub fn scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_table_scroll_offset(self.widget.as_raw()) }
    }

    pub fn max_scroll_offset(&self) -> f32 {
        unsafe { sys::oneui_table_max_scroll_offset(self.widget.as_raw()) }
    }

    #[track_caller]
    pub fn set_on_changed<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Table", "changed", std::panic::Location::caller());
        self.clear_on_changed();
        self.changed_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data =
            (self.changed_callback.as_deref_mut().unwrap() as *mut ListChangedCallback).cast();
        unsafe {
            sys::oneui_table_set_on_changed(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_changed(&mut self) {
        unsafe {
            sys::oneui_table_set_on_changed(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.changed_callback = None;
    }

    #[track_caller]
    pub fn set_on_selection_changed<F>(&mut self, callback: F)
    where
        F: FnMut(Vec<i32>) + 'static,
    {
        let trace =
            InteractionTrace::at("Table", "selection_changed", std::panic::Location::caller());
        self.clear_on_selection_changed();
        self.selection_changed_callback = Some(Box::new(ListSelectionChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self.selection_changed_callback.as_deref_mut().unwrap()
            as *mut ListSelectionChangedCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_selection_changed(
                self.widget.as_raw(),
                Some(run_list_selection_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_selection_changed(&mut self) {
        unsafe {
            sys::oneui_table_set_on_selection_changed(
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
        let trace = InteractionTrace::at("Table", "activated", std::panic::Location::caller());
        self.clear_on_activated();
        self.activated_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data =
            (self.activated_callback.as_deref_mut().unwrap() as *mut ListChangedCallback).cast();
        unsafe {
            sys::oneui_table_set_on_activated(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_activated(&mut self) {
        unsafe {
            sys::oneui_table_set_on_activated(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.activated_callback = None;
    }

    #[track_caller]
    pub fn set_on_edit_requested<F>(&mut self, callback: F)
    where
        F: FnMut(i32) + 'static,
    {
        let trace = InteractionTrace::at("Table", "edit_requested", std::panic::Location::caller());
        self.clear_on_edit_requested();
        self.edit_requested_callback = Some(Box::new(ListChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self.edit_requested_callback.as_deref_mut().unwrap()
            as *mut ListChangedCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_edit_requested(
                self.widget.as_raw(),
                Some(run_list_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_edit_requested(&mut self) {
        unsafe {
            sys::oneui_table_set_on_edit_requested(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.edit_requested_callback = None;
    }

    #[track_caller]
    pub fn set_on_delete_requested<F>(&mut self, callback: F)
    where
        F: FnMut(Vec<i32>) + 'static,
    {
        let trace =
            InteractionTrace::at("Table", "delete_requested", std::panic::Location::caller());
        self.clear_on_delete_requested();
        self.delete_requested_callback = Some(Box::new(ListSelectionChangedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self.delete_requested_callback.as_deref_mut().unwrap()
            as *mut ListSelectionChangedCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_delete_requested(
                self.widget.as_raw(),
                Some(run_list_selection_changed_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_delete_requested(&mut self) {
        unsafe {
            sys::oneui_table_set_on_delete_requested(
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
            "Table",
            "context_menu_requested",
            std::panic::Location::caller(),
        );
        self.clear_on_context_menu_requested();
        self.context_menu_requested_callback = Some(Box::new(ContextMenuRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self.context_menu_requested_callback.as_deref_mut().unwrap()
            as *mut ContextMenuRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_context_menu_requested(
                self.widget.as_raw(),
                Some(run_context_menu_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_context_menu_requested(&mut self) {
        unsafe {
            sys::oneui_table_set_on_context_menu_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.context_menu_requested_callback = None;
    }

    /// Enables pointer and `Alt+Up`/`Alt+Down` row reorder requests.
    pub fn set_reorder_enabled(&self, enabled: bool) {
        unsafe { sys::oneui_table_set_reorder_enabled(self.widget.as_raw(), i32::from(enabled)) };
    }

    pub fn reorder_enabled(&self) -> bool {
        unsafe { sys::oneui_table_reorder_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_reorder_requested<F>(&mut self, callback: F)
    where
        F: FnMut(ReorderRequest) + 'static,
    {
        let trace =
            InteractionTrace::at("Table", "reorder_requested", std::panic::Location::caller());
        self.clear_on_reorder_requested();
        self.reorder_requested_callback = Some(Box::new(ReorderRequestedCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data = (self.reorder_requested_callback.as_deref_mut().unwrap()
            as *mut ReorderRequestedCallback)
            .cast();
        unsafe {
            sys::oneui_table_set_on_reorder_requested(
                self.widget.as_raw(),
                Some(run_reorder_requested_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_reorder_requested(&mut self) {
        unsafe {
            sys::oneui_table_set_on_reorder_requested(
                self.widget.as_raw(),
                None,
                std::ptr::null_mut(),
            )
        };
        self.reorder_requested_callback = None;
    }

    /// Assigns stable domain identifiers to the current rows for external drag operations.
    pub fn set_item_drag_ids(&self, ids: &[String]) -> bool {
        let native_ids = ids
            .iter()
            .map(|id| sys::OneUiUtf8String::from_str(id))
            .collect::<Vec<_>>();
        unsafe {
            sys::oneui_table_set_item_drag_ids_utf8(
                self.widget.as_raw(),
                native_ids.as_ptr(),
                native_ids.len(),
            ) != 0
        }
    }

    pub fn set_item_drag_enabled(&self, enabled: bool) {
        unsafe { sys::oneui_table_set_item_drag_enabled(self.widget.as_raw(), i32::from(enabled)) };
    }

    pub fn item_drag_enabled(&self) -> bool {
        unsafe { sys::oneui_table_item_drag_enabled(self.widget.as_raw()) != 0 }
    }

    #[track_caller]
    pub fn set_on_item_drag<F>(&mut self, callback: F)
    where
        F: FnMut(ItemDragEvent) + 'static,
    {
        let trace = InteractionTrace::at("Table", "item_drag", std::panic::Location::caller());
        self.clear_on_item_drag();
        self.item_drag_callback = Some(Box::new(ItemDragCallback {
            handler: Box::new(traced_value_callback(trace, callback)),
        }));
        let user_data =
            (self.item_drag_callback.as_deref_mut().unwrap() as *mut ItemDragCallback).cast();
        unsafe {
            sys::oneui_table_set_on_item_drag_utf8(
                self.widget.as_raw(),
                Some(run_item_drag_callback),
                user_data,
            )
        };
    }

    pub fn clear_on_item_drag(&mut self) {
        unsafe {
            sys::oneui_table_set_on_item_drag_utf8(self.widget.as_raw(), None, std::ptr::null_mut())
        };
        self.item_drag_callback = None;
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}

impl Drop for Table {
    fn drop(&mut self) {
        self.clear_on_cell_action();
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
            .pending_rows
            .lock()
            .expect("table pending rows lock poisoned")
            .clear();
        self.state.update_scheduled.store(false, Ordering::Release);
    }
}
