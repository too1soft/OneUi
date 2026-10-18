//! Data api for the UTF-8 C ABI.

use super::abi_types::{
    OneUiIndexPointCallback, OneUiInsets, OneUiIntCallback, OneUiItemDragCallback,
    OneUiListItemUtf8, OneUiProgressSegmentUtf8, OneUiReorderRequestedCallback,
    OneUiTableColumnUtf8, OneUiTableRichRowUtf8, OneUiTableRowUtf8, OneUiTimeSeriesThreshold,
    OneUiTimeSeriesUtf8, OneUiTreeExpansionCallback, OneUiTreeItemUtf8,
    OneUiTreeReorderRequestedCallback, OneUiUtf8String, OneUiUtf8TextCallback, OneUiWidget,
};
use std::ffi::{c_char, c_float, c_int, c_void};

extern "C" {

    pub fn oneui_progress_bar_set_segments_utf8(
        bar: *mut OneUiWidget,
        segments: *const OneUiProgressSegmentUtf8,
        count: usize,
    ) -> i32;
    pub fn oneui_table_set_rich_rows_utf8(
        table: *mut OneUiWidget,
        rows: *const OneUiTableRichRowUtf8,
        count: usize,
    );
    pub fn oneui_table_update_rich_row_utf8(
        table: *mut OneUiWidget,
        index: usize,
        row: *const OneUiTableRichRowUtf8,
    ) -> i32;
    pub fn oneui_table_set_column_presentation(
        table: *mut OneUiWidget,
        column: i32,
        alignment: i32,
        action: i32,
    );
    pub fn oneui_table_set_column_dividers_visible(table: *mut OneUiWidget, visible: c_int);
    pub fn oneui_table_set_column_kind(table: *mut OneUiWidget, column: c_int, kind: c_int);
    pub fn oneui_table_set_selection_column_visible(table: *mut OneUiWidget, visible: c_int);
    pub fn oneui_table_set_selection_column_width(table: *mut OneUiWidget, width: c_float);
    pub fn oneui_table_set_header_sort(table: *mut OneUiWidget, column: i32, descending: i32);
    pub fn oneui_table_set_header_height(table: *mut OneUiWidget, height: f32);
    pub fn oneui_table_set_on_cell_action(
        table: *mut OneUiWidget,
        callback: Option<unsafe extern "C" fn(i32, i32, *mut std::ffi::c_void)>,
        user_data: *mut std::ffi::c_void,
    );
    pub fn oneui_progress_bar_create() -> *mut OneUiWidget;
    pub fn oneui_progress_bar_set_value(progress_bar: *mut OneUiWidget, value: f64);
    pub fn oneui_progress_bar_value(progress_bar: *mut OneUiWidget) -> f64;
    pub fn oneui_progress_bar_set_smooth(
        progress_bar: *mut OneUiWidget,
        enabled: c_int,
        duration_ms: f64,
    );
    pub fn oneui_progress_bar_set_indeterminate(
        progress_bar: *mut OneUiWidget,
        indeterminate: c_int,
    );
    pub fn oneui_progress_bar_is_indeterminate(progress_bar: *mut OneUiWidget) -> c_int;
    pub fn oneui_progress_bar_set_animations_enabled(
        progress_bar: *mut OneUiWidget,
        enabled: c_int,
    );
    pub fn oneui_progress_bar_set_colors(
        progress_bar: *mut OneUiWidget,
        track_r: u8,
        track_g: u8,
        track_b: u8,
        track_a: u8,
        fill_r: u8,
        fill_g: u8,
        fill_b: u8,
        fill_a: u8,
        radius: c_float,
    );

    pub fn oneui_sparkline_create() -> *mut OneUiWidget;
    pub fn oneui_sparkline_set_values(
        sparkline: *mut OneUiWidget,
        values: *const f64,
        count: usize,
    );
    pub fn oneui_time_series_chart_create() -> *mut OneUiWidget;
    pub fn oneui_time_series_chart_set_series(
        chart: *mut OneUiWidget,
        series: *const OneUiTimeSeriesUtf8,
        count: usize,
    );
    pub fn oneui_time_series_chart_set_range(chart: *mut OneUiWidget, minimum: f64, maximum: f64);
    pub fn oneui_time_series_chart_set_grid_lines(chart: *mut OneUiWidget, count: c_int);
    pub fn oneui_time_series_chart_set_visual_style(
        chart: *mut OneUiWidget,
        smooth_curves: c_int,
        area_fill: c_int,
        dashed_grid: c_int,
        axes_visible: c_int,
        line_width: c_float,
        fill_alpha: u8,
    );
    pub fn oneui_time_series_chart_set_plot_insets(chart: *mut OneUiWidget, insets: OneUiInsets);
    pub fn oneui_time_series_chart_set_grid_style(
        chart: *mut OneUiWidget,
        vertical_lines: i32,
        line_width: f32,
    );
    pub fn oneui_time_series_chart_set_axis_labels_utf8(
        chart: *mut OneUiWidget,
        labels: *const OneUiUtf8String,
        count: usize,
    );
    pub fn oneui_time_series_chart_set_sample_positions(
        chart: *mut OneUiWidget,
        positions: *const f64,
        count: usize,
    ) -> i32;
    pub fn oneui_time_series_chart_set_latest_point_visible(chart: *mut OneUiWidget, visible: i32);
    pub fn oneui_time_series_chart_set_latest_point_size(
        chart: *mut OneUiWidget,
        width: f32,
        height: f32,
    );
    pub fn oneui_time_series_chart_set_thresholds(
        chart: *mut OneUiWidget,
        thresholds: *const OneUiTimeSeriesThreshold,
        count: usize,
    );
    pub fn oneui_time_series_chart_set_inspection(
        chart: *mut OneUiWidget,
        index: c_int,
        pinned: c_int,
    );
    pub fn oneui_time_series_chart_inspection_index(chart: *const OneUiWidget) -> c_int;
    pub fn oneui_time_series_chart_inspection_pinned(chart: *const OneUiWidget) -> c_int;
    pub fn oneui_time_series_chart_set_on_inspection_changed(
        chart: *mut OneUiWidget,
        callback: Option<unsafe extern "C" fn(c_int, c_int, *mut c_void)>,
        user_data: *mut c_void,
    );
    pub fn oneui_list_create() -> *mut OneUiWidget;
    pub fn oneui_list_set_items_utf8(
        list: *mut OneUiWidget,
        items: *const OneUiListItemUtf8,
        count: usize,
    );
    pub fn oneui_list_set_selection_required(list: *mut OneUiWidget, required: c_int);
    pub fn oneui_list_set_selected_index(list: *mut OneUiWidget, index: c_int);
    pub fn oneui_list_selected_index(list: *mut OneUiWidget) -> c_int;
    pub fn oneui_list_set_on_changed(
        list: *mut OneUiWidget,
        callback: Option<unsafe extern "C" fn(value: c_int, user_data: *mut c_void)>,
        user_data: *mut c_void,
    );
    pub fn oneui_virtual_list_create() -> *mut OneUiWidget;
    pub fn oneui_virtual_list_set_items_utf8(
        list: *mut OneUiWidget,
        items: *const OneUiListItemUtf8,
        count: usize,
    );
    pub fn oneui_virtual_list_update_item_utf8(
        list: *mut OneUiWidget,
        index: usize,
        item: *const OneUiListItemUtf8,
    ) -> c_int;
    pub fn oneui_virtual_list_set_selected_index(list: *mut OneUiWidget, index: c_int);
    pub fn oneui_virtual_list_selected_index(list: *mut OneUiWidget) -> c_int;
    pub fn oneui_virtual_list_set_selection_mode(list: *mut OneUiWidget, mode: c_int);
    pub fn oneui_virtual_list_set_selected_indices(
        list: *mut OneUiWidget,
        indices: *const c_int,
        count: usize,
    );
    pub fn oneui_virtual_list_selected_indices(
        list: *mut OneUiWidget,
        buffer: *mut c_int,
        buffer_len: usize,
    ) -> usize;
    pub fn oneui_virtual_list_set_row_height(list: *mut OneUiWidget, height: f32);
    pub fn oneui_virtual_list_set_activate_on_single_click(list: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_virtual_list_set_scroll_offset(list: *mut OneUiWidget, offset: f32);
    pub fn oneui_virtual_list_scroll_offset(list: *mut OneUiWidget) -> f32;
    pub fn oneui_virtual_list_max_scroll_offset(list: *mut OneUiWidget) -> f32;
    pub fn oneui_virtual_list_set_on_changed(
        list: *mut OneUiWidget,
        callback: Option<unsafe extern "C" fn(value: c_int, user_data: *mut c_void)>,
        user_data: *mut c_void,
    );
    pub fn oneui_virtual_list_set_on_selection_changed(
        list: *mut OneUiWidget,
        callback: Option<
            unsafe extern "C" fn(values: *const c_int, count: usize, user_data: *mut c_void),
        >,
        user_data: *mut c_void,
    );
    pub fn oneui_virtual_list_set_on_activated(
        list: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_virtual_list_set_on_edit_requested(
        list: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_virtual_list_set_on_delete_requested(
        list: *mut OneUiWidget,
        callback: Option<
            unsafe extern "C" fn(values: *const c_int, count: usize, user_data: *mut c_void),
        >,
        user_data: *mut c_void,
    );
    pub fn oneui_virtual_list_set_on_context_menu_requested(
        list: *mut OneUiWidget,
        callback: OneUiIndexPointCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_virtual_list_set_reorder_enabled(list: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_virtual_list_reorder_enabled(list: *mut OneUiWidget) -> c_int;
    pub fn oneui_virtual_list_set_on_reorder_requested(
        list: *mut OneUiWidget,
        callback: OneUiReorderRequestedCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_virtual_list_set_item_drag_ids_utf8(
        list: *mut OneUiWidget,
        ids: *const OneUiUtf8String,
        count: usize,
    ) -> c_int;
    pub fn oneui_virtual_list_set_item_drag_enabled(list: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_virtual_list_item_drag_enabled(list: *mut OneUiWidget) -> c_int;
    pub fn oneui_virtual_list_set_on_item_drag_utf8(
        list: *mut OneUiWidget,
        callback: OneUiItemDragCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_table_create() -> *mut OneUiWidget;
    pub fn oneui_table_set_columns_utf8(
        table: *mut OneUiWidget,
        columns: *const OneUiTableColumnUtf8,
        count: usize,
    );
    pub fn oneui_table_set_rows_utf8(
        table: *mut OneUiWidget,
        rows: *const OneUiTableRowUtf8,
        count: usize,
    );
    pub fn oneui_table_update_row_utf8(
        table: *mut OneUiWidget,
        index: usize,
        row: *const OneUiTableRowUtf8,
    ) -> c_int;
    pub fn oneui_table_set_selection_mode(table: *mut OneUiWidget, mode: c_int);
    pub fn oneui_table_set_selected_index(table: *mut OneUiWidget, index: c_int);
    pub fn oneui_table_selected_index(table: *mut OneUiWidget) -> c_int;
    pub fn oneui_table_set_selected_indices(
        table: *mut OneUiWidget,
        indices: *const c_int,
        count: usize,
    );
    pub fn oneui_table_selected_indices(
        table: *mut OneUiWidget,
        buffer: *mut c_int,
        buffer_len: usize,
    ) -> usize;
    pub fn oneui_table_set_row_height(table: *mut OneUiWidget, height: f32);
    pub fn oneui_table_set_scroll_offset(table: *mut OneUiWidget, offset: f32);
    pub fn oneui_table_scroll_offset(table: *mut OneUiWidget) -> f32;
    pub fn oneui_table_max_scroll_offset(table: *mut OneUiWidget) -> f32;
    pub fn oneui_table_set_on_changed(
        table: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_table_set_on_selection_changed(
        table: *mut OneUiWidget,
        callback: Option<unsafe extern "C" fn(*const c_int, usize, *mut c_void)>,
        user_data: *mut c_void,
    );
    pub fn oneui_table_set_on_activated(
        table: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_table_set_on_edit_requested(
        table: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_table_set_on_delete_requested(
        table: *mut OneUiWidget,
        callback: Option<unsafe extern "C" fn(*const c_int, usize, *mut c_void)>,
        user_data: *mut c_void,
    );
    pub fn oneui_table_set_on_context_menu_requested(
        table: *mut OneUiWidget,
        callback: OneUiIndexPointCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_table_set_reorder_enabled(table: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_table_reorder_enabled(table: *mut OneUiWidget) -> c_int;
    pub fn oneui_table_set_on_reorder_requested(
        table: *mut OneUiWidget,
        callback: OneUiReorderRequestedCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_table_set_item_drag_ids_utf8(
        table: *mut OneUiWidget,
        ids: *const OneUiUtf8String,
        count: usize,
    ) -> c_int;
    pub fn oneui_table_set_item_drag_enabled(table: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_table_item_drag_enabled(table: *mut OneUiWidget) -> c_int;
    pub fn oneui_table_set_on_item_drag_utf8(
        table: *mut OneUiWidget,
        callback: OneUiItemDragCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_tree_view_create() -> *mut OneUiWidget;
    pub fn oneui_tree_view_set_items_utf8(
        tree_view: *mut OneUiWidget,
        items: *const OneUiTreeItemUtf8,
        count: usize,
    );
    pub fn oneui_tree_view_set_selected_id_utf8(tree_view: *mut OneUiWidget, id: OneUiUtf8String);
    pub fn oneui_tree_view_content_height(tree_view: *mut OneUiWidget) -> f32;
    pub fn oneui_tree_view_selected_id_utf8(
        tree_view: *mut OneUiWidget,
        buffer: *mut c_char,
        buffer_len: usize,
    ) -> usize;
    pub fn oneui_tree_view_set_on_selection_changed_utf8(
        tree_view: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_tree_view_set_on_expansion_changed_utf8(
        tree_view: *mut OneUiWidget,
        callback: OneUiTreeExpansionCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_tree_view_set_reorder_enabled(tree_view: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_tree_view_reorder_enabled(tree_view: *mut OneUiWidget) -> c_int;
    pub fn oneui_tree_view_set_on_reorder_requested_utf8(
        tree_view: *mut OneUiWidget,
        callback: OneUiTreeReorderRequestedCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_tree_view_update_external_drop_target(tree_view: *mut OneUiWidget, x: f32, y: f32);
    pub fn oneui_tree_view_clear_external_drop_target(tree_view: *mut OneUiWidget);
    pub fn oneui_tree_view_external_drop_target_id_utf8(
        tree_view: *mut OneUiWidget,
        buffer: *mut c_char,
        buffer_len: usize,
    ) -> usize;
}
