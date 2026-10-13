//! Control api for the UTF-8 C ABI.

use super::abi_types::{
    OneUiBoolCallback, OneUiButtonStyle, OneUiColor, OneUiFloatCallback,
    OneUiGridReorderRequestedCallback, OneUiIndexPointCallback, OneUiInsets, OneUiIntCallback,
    OneUiInteractiveSurfaceStyle, OneUiItemDragCallback, OneUiLabelTextSpanUtf8,
    OneUiPointerCallback, OneUiPointerDragCallback, OneUiReorderRequestedCallback,
    OneUiSliderInteractionCallback, OneUiUtf8String, OneUiUtf8TextCallback, OneUiVoidCallback,
    OneUiWidget, WChar,
};
use super::{OneUiButtonVariant, OneUiSplitOrientation, OneUiStackAlign, OneUiStackDirection};
use std::ffi::{c_char, c_float, c_int, c_uint, c_void};

extern "C" {

    pub fn oneui_interactive_surface_set_on_drag(
        surface: *mut OneUiWidget,
        callback: OneUiPointerDragCallback,
        threshold: c_float,
        user_data: *mut c_void,
    );
    pub fn oneui_interactive_surface_set_pointer_cursor(surface: *mut OneUiWidget, cursor: c_int);
    pub fn oneui_text_measure_width_utf8(
        text: OneUiUtf8String,
        family: OneUiUtf8String,
        size: f32,
        weight: i32,
        scale: f32,
        width: *mut f32,
    ) -> i32;
    pub fn oneui_enumerate_system_fonts(
        callback: Option<unsafe extern "C" fn(OneUiUtf8String, c_int, *mut c_void)>,
        user_data: *mut c_void,
    ) -> c_int;
    pub fn oneui_double_click_interval_ms() -> u32;
    // 0 = column, 1 = row. These values are part of the stable C ABI.
    pub fn oneui_stack_create(direction: OneUiStackDirection) -> *mut OneUiWidget;
    pub fn oneui_stack_add(stack: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_stack_set_direction(stack: *mut OneUiWidget, direction: c_int);
    pub fn oneui_stack_set_gap(stack: *mut OneUiWidget, gap: f32);
    pub fn oneui_stack_set_padding(stack: *mut OneUiWidget, insets: OneUiInsets);
    pub fn oneui_stack_set_align(stack: *mut OneUiWidget, align: OneUiStackAlign);
    pub fn oneui_stack_set_engine(stack: *mut OneUiWidget, engine: c_int) -> c_int;
    pub fn oneui_stack_set_wrap(stack: *mut OneUiWidget, enabled: c_int) -> c_int;
    pub fn oneui_stack_set_justify(stack: *mut OneUiWidget, justify: c_int) -> c_int;
    pub fn oneui_stack_set_flex(
        stack: *mut OneUiWidget,
        child: *mut OneUiWidget,
        grow: f32,
        shrink: f32,
        basis_mode: c_int,
        basis: f32,
        min: f32,
        max: f32,
    ) -> c_int;
    pub fn oneui_stack_clear_flex(stack: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_stack_content_width(stack: *mut OneUiWidget) -> f32;
    pub fn oneui_stack_content_height(stack: *mut OneUiWidget) -> f32;

    // 0 = horizontal, 1 = vertical. These values are part of the stable C ABI.
    pub fn oneui_split_view_create(orientation: OneUiSplitOrientation) -> *mut OneUiWidget;
    pub fn oneui_split_view_set_first(split_view: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_split_view_set_second(split_view: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_split_view_set_orientation(
        split_view: *mut OneUiWidget,
        orientation: OneUiSplitOrientation,
    );
    pub fn oneui_split_view_set_ratio(split_view: *mut OneUiWidget, ratio: f32);
    pub fn oneui_split_view_ratio(split_view: *mut OneUiWidget) -> f32;
    pub fn oneui_split_view_set_gap(split_view: *mut OneUiWidget, gap: f32);
    pub fn oneui_split_view_set_divider_hit_extent(split_view: *mut OneUiWidget, extent: f32);
    pub fn oneui_split_view_set_divider_colors(
        split_view: *mut OneUiWidget,
        normal: OneUiColor,
        active: OneUiColor,
    );
    pub fn oneui_split_view_set_padding(split_view: *mut OneUiWidget, insets: OneUiInsets);
    pub fn oneui_split_view_set_resizable(split_view: *mut OneUiWidget, resizable: c_int);
    pub fn oneui_split_view_set_minimum_pane_extent(
        split_view: *mut OneUiWidget,
        first: f32,
        second: f32,
    );
    pub fn oneui_split_view_set_on_ratio_changed(
        split_view: *mut OneUiWidget,
        callback: OneUiFloatCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_split_view_set_on_ratio_committed(
        split_view: *mut OneUiWidget,
        callback: OneUiFloatCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_overlay_host_create() -> *mut OneUiWidget;
    pub fn oneui_overlay_host_set_content(host: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_overlay_host_add_overlay(
        host: *mut OneUiWidget,
        child: *mut OneUiWidget,
        layer: c_int,
    );
    pub fn oneui_overlay_host_add_anchored_overlay(
        host: *mut OneUiWidget,
        child: *mut OneUiWidget,
        layer: c_int,
        width: f32,
        height: f32,
        margin: OneUiInsets,
        horizontal_alignment: c_int,
        vertical_alignment: c_int,
    );
    pub fn oneui_overlay_host_add_modal_anchored_overlay(
        host: *mut OneUiWidget,
        child: *mut OneUiWidget,
        layer: c_int,
        width: f32,
        height: f32,
        margin: OneUiInsets,
        horizontal_alignment: c_int,
        vertical_alignment: c_int,
    );
    pub fn oneui_overlay_host_update_anchored_overlay(
        host: *mut OneUiWidget,
        child: *mut OneUiWidget,
        width: f32,
        height: f32,
        margin: OneUiInsets,
        horizontal_alignment: c_int,
        vertical_alignment: c_int,
    ) -> c_int;
    pub fn oneui_overlay_host_remove_overlay(
        host: *mut OneUiWidget,
        child: *mut OneUiWidget,
    ) -> c_int;

    pub fn oneui_popup_create() -> *mut OneUiWidget;
    pub fn oneui_popup_set_anchor(popup: *mut OneUiWidget, anchor: *mut OneUiWidget);
    pub fn oneui_popup_set_content(popup: *mut OneUiWidget, content: *mut OneUiWidget);
    pub fn oneui_popup_set_on_closed(
        popup: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_popup_set_open(popup: *mut OneUiWidget, open: c_int);
    pub fn oneui_popup_is_open(popup: *mut OneUiWidget) -> c_int;
    pub fn oneui_popup_set_anchor_rect(
        popup: *mut OneUiWidget,
        x: f32,
        y: f32,
        width: f32,
        height: f32,
    );
    pub fn oneui_popup_clear_anchor_rect(popup: *mut OneUiWidget);
    pub fn oneui_popup_set_preferred_placement(popup: *mut OneUiWidget, placement: c_int);
    pub fn oneui_popup_set_interaction_mode(popup: *mut OneUiWidget, mode: c_int);

    pub fn oneui_dialog_create(title: *const WChar, subtitle: *const WChar) -> *mut OneUiWidget;
    pub fn oneui_dialog_set_title(dialog: *mut OneUiWidget, title: *const WChar);
    pub fn oneui_dialog_set_subtitle(dialog: *mut OneUiWidget, subtitle: *const WChar);
    pub fn oneui_dialog_set_icon(dialog: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_dialog_set_close_visible(dialog: *mut OneUiWidget, visible: c_int);
    pub fn oneui_dialog_set_on_close(
        dialog: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_dialog_set_content(dialog: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_dialog_set_actions(dialog: *mut OneUiWidget, child: *mut OneUiWidget);

    pub fn oneui_panel_create() -> *mut OneUiWidget;
    pub fn oneui_panel_set_content(panel: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_panel_set_background(panel: *mut OneUiWidget, r: u8, g: u8, b: u8, a: u8);
    pub fn oneui_panel_set_border(panel: *mut OneUiWidget, r: u8, g: u8, b: u8, a: u8, width: f32);
    pub fn oneui_panel_set_radius(panel: *mut OneUiWidget, radius: f32);
    pub fn oneui_panel_set_padding(panel: *mut OneUiWidget, insets: OneUiInsets);
    pub fn oneui_reorderable_grid_create() -> *mut OneUiWidget;
    pub fn oneui_reorderable_grid_clear_items(grid: *mut OneUiWidget);
    pub fn oneui_reorderable_grid_add_item_utf8(
        grid: *mut OneUiWidget,
        id: OneUiUtf8String,
        child: *mut OneUiWidget,
    );
    pub fn oneui_reorderable_grid_move_item_utf8(
        grid: *mut OneUiWidget,
        source_id: OneUiUtf8String,
        target_index: c_int,
    ) -> c_int;
    pub fn oneui_reorderable_grid_set_column_count(grid: *mut OneUiWidget, columns: c_int);
    pub fn oneui_reorderable_grid_set_gaps(grid: *mut OneUiWidget, column_gap: f32, row_gap: f32);
    pub fn oneui_reorderable_grid_set_item_height(grid: *mut OneUiWidget, height: f32);
    pub fn oneui_reorderable_grid_content_height(grid: *mut OneUiWidget) -> f32;
    pub fn oneui_reorderable_grid_set_reorder_enabled(grid: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_reorderable_grid_reorder_enabled(grid: *mut OneUiWidget) -> c_int;
    pub fn oneui_reorderable_grid_set_on_reorder_requested_utf8(
        grid: *mut OneUiWidget,
        callback: OneUiGridReorderRequestedCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_reorderable_grid_set_item_drag_enabled(grid: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_reorderable_grid_item_drag_enabled(grid: *mut OneUiWidget) -> c_int;
    pub fn oneui_reorderable_grid_set_on_item_drag_utf8(
        grid: *mut OneUiWidget,
        callback: OneUiItemDragCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_interactive_surface_create() -> *mut OneUiWidget;
    pub fn oneui_interactive_surface_set_content(
        surface: *mut OneUiWidget,
        child: *mut OneUiWidget,
    );
    pub fn oneui_interactive_surface_set_padding(surface: *mut OneUiWidget, insets: OneUiInsets);
    pub fn oneui_interactive_surface_set_style(
        surface: *mut OneUiWidget,
        style: *const OneUiInteractiveSurfaceStyle,
    );
    pub fn oneui_interactive_surface_set_on_click(
        surface: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_interactive_surface_set_on_pointer_activated(
        surface: *mut OneUiWidget,
        callback: OneUiPointerCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_interactive_surface_set_on_pointer_moved(
        surface: *mut OneUiWidget,
        callback: OneUiPointerCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_interactive_surface_set_on_hover_changed(
        surface: *mut OneUiWidget,
        callback: OneUiBoolCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_interactive_surface_set_on_focus_changed(
        surface: *mut OneUiWidget,
        callback: OneUiBoolCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_interactive_surface_set_on_context_menu_requested(
        surface: *mut OneUiWidget,
        callback: OneUiPointerCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_scroll_view_create() -> *mut OneUiWidget;
    pub fn oneui_scroll_view_set_content(view: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_scroll_view_set_content_width(view: *mut OneUiWidget, width: f32);
    pub fn oneui_scroll_view_set_content_height(view: *mut OneUiWidget, height: f32);
    pub fn oneui_scroll_view_set_wheel_step(view: *mut OneUiWidget, step: f32);
    pub fn oneui_scroll_view_set_chrome_visible(view: *mut OneUiWidget, visible: c_int);
    pub fn oneui_scroll_view_set_scrollbar_inset(view: *mut OneUiWidget, inset: f32);
    pub fn oneui_scroll_view_set_scrollbar_style(
        view: *mut OneUiWidget,
        r: u8,
        g: u8,
        b: u8,
        a: u8,
        thickness: f32,
    );
    pub fn oneui_scroll_view_set_scroll_offset(view: *mut OneUiWidget, offset: f32);
    pub fn oneui_scroll_view_scroll_offset(view: *mut OneUiWidget) -> f32;
    pub fn oneui_scroll_view_max_scroll_offset(view: *mut OneUiWidget) -> f32;
    pub fn oneui_scroll_view_scroll_to_bottom(view: *mut OneUiWidget);
    pub fn oneui_label_create_utf8(text: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_label_set_text_utf8(label: *mut OneUiWidget, text: OneUiUtf8String);
    pub fn oneui_label_set_rich_text_utf8(
        label: *mut OneUiWidget,
        text: OneUiUtf8String,
        spans: *const OneUiLabelTextSpanUtf8,
        span_count: usize,
    ) -> c_int;
    pub fn oneui_label_set_color(label: *mut OneUiWidget, r: u8, g: u8, b: u8, a: u8);
    pub fn oneui_label_set_status_indicator(label: *mut OneUiWidget, diameter: f32, gap: f32);
    pub fn oneui_label_set_font_size(label: *mut OneUiWidget, font_size: f32);
    pub fn oneui_label_natural_text_width(label: *mut OneUiWidget) -> f32;
    pub fn oneui_button_set_trailing_icon(button: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_text_field_set_focused_placeholder(field: *mut OneUiWidget, visible: c_int);
    pub fn oneui_label_set_font_weight(label: *mut OneUiWidget, font_weight: c_int);
    pub fn oneui_label_set_align(label: *mut OneUiWidget, align: c_int);
    pub fn oneui_label_set_text_wrapping(label: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_label_set_max_lines(label: *mut OneUiWidget, max_lines: c_int);
    pub fn oneui_label_set_line_height(label: *mut OneUiWidget, line_height: f32);

    pub fn oneui_slider_create() -> *mut OneUiWidget;
    pub fn oneui_slider_set_range(slider: *mut OneUiWidget, minimum: f64, maximum: f64);
    pub fn oneui_slider_set_step(slider: *mut OneUiWidget, step: f64);
    pub fn oneui_slider_set_value(slider: *mut OneUiWidget, value: f64);
    pub fn oneui_slider_value(slider: *mut OneUiWidget) -> f64;
    pub fn oneui_slider_is_dragging(slider: *mut OneUiWidget) -> c_int;
    pub fn oneui_slider_cancel_interaction(slider: *mut OneUiWidget);
    pub fn oneui_slider_set_on_interaction(
        slider: *mut OneUiWidget,
        callback: OneUiSliderInteractionCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_icon_create(symbol: c_int) -> *mut OneUiWidget;
    pub fn oneui_icon_set_symbol(icon: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_icon_set_color(icon: *mut OneUiWidget, r: u8, g: u8, b: u8, a: u8);
    pub fn oneui_image_view_create() -> *mut OneUiWidget;
    pub fn oneui_image_view_set_rgba(
        image: *mut OneUiWidget,
        pixels: *const u8,
        length: usize,
        width: c_int,
        height: c_int,
        stride: c_int,
    ) -> c_int;
    pub fn oneui_image_view_clear(image: *mut OneUiWidget);
    pub fn oneui_image_view_set_content_mode(image: *mut OneUiWidget, content_mode: c_int);
    pub fn oneui_image_view_set_content_alignment(
        image: *mut OneUiWidget,
        horizontal_alignment: c_int,
        vertical_alignment: c_int,
    );
    pub fn oneui_image_view_set_corner_radius(image: *mut OneUiWidget, radius: f32);
    pub fn oneui_image_view_set_background(image: *mut OneUiWidget, r: u8, g: u8, b: u8, a: u8);

    pub fn oneui_icon_button_create(symbol: c_int) -> *mut OneUiWidget;
    pub fn oneui_icon_button_set_symbol(icon_button: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_icon_button_set_on_click(
        icon_button: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_switch_create(text: *const WChar) -> *mut OneUiWidget;
    pub fn oneui_switch_set_text(switch_widget: *mut OneUiWidget, text: *const WChar);
    pub fn oneui_switch_set_checked(switch_widget: *mut OneUiWidget, checked: c_int);
    pub fn oneui_switch_checked(switch_widget: *mut OneUiWidget) -> c_int;
    pub fn oneui_switch_set_on_changed(
        switch_widget: *mut OneUiWidget,
        callback: OneUiBoolCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_checkbox_create(text: *const WChar) -> *mut OneUiWidget;
    pub fn oneui_checkbox_set_text(checkbox: *mut OneUiWidget, text: *const WChar);
    pub fn oneui_checkbox_set_checked(checkbox: *mut OneUiWidget, checked: c_int);
    pub fn oneui_checkbox_checked(checkbox: *mut OneUiWidget) -> c_int;
    pub fn oneui_checkbox_set_on_changed(
        checkbox: *mut OneUiWidget,
        callback: OneUiBoolCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_segmented_control_create() -> *mut OneUiWidget;
    pub fn oneui_segmented_control_set_items(
        segmented_control: *mut OneUiWidget,
        items: *const WChar,
    );
    pub fn oneui_segmented_control_set_selected_index(
        segmented_control: *mut OneUiWidget,
        index: c_int,
    );
    pub fn oneui_segmented_control_selected_index(segmented_control: *mut OneUiWidget) -> c_int;
    pub fn oneui_segmented_control_set_on_changed(
        segmented_control: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_tabs_create() -> *mut OneUiWidget;
    pub fn oneui_tabs_begin_edit(tabs: *mut OneUiWidget, index: i32) -> i32;
    pub fn oneui_tabs_cancel_edit(tabs: *mut OneUiWidget);
    pub fn oneui_tabs_editing_index(tabs: *mut OneUiWidget) -> i32;
    pub fn oneui_tabs_set_on_edit_finished(
        tabs: *mut OneUiWidget,
        callback: Option<unsafe extern "C" fn(i32, *const c_char, usize, i32, *mut c_void)>,
        user_data: *mut c_void,
    );
    pub fn oneui_tabs_set_items_utf8(
        tabs: *mut OneUiWidget,
        items: *const OneUiUtf8String,
        count: usize,
    );
    pub fn oneui_tabs_set_item_icons(tabs: *mut OneUiWidget, symbols: *const c_int, count: usize);
    pub fn oneui_tabs_set_document_mode(tabs: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_tabs_overflow_item_count(tabs: *mut OneUiWidget) -> c_int;
    pub fn oneui_tabs_set_on_overflow_changed(
        tabs: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_tabs_set_item_status_colors(
        tabs: *mut OneUiWidget,
        colors: *const OneUiColor,
        count: usize,
    );
    pub fn oneui_tabs_set_selected_index(tabs: *mut OneUiWidget, index: c_int);
    pub fn oneui_tabs_selected_index(tabs: *mut OneUiWidget) -> c_int;
    pub fn oneui_tabs_set_compact(tabs: *mut OneUiWidget, compact: c_int);
    pub fn oneui_tabs_set_item_width_range(
        tabs: *mut OneUiWidget,
        minimum: c_float,
        maximum: c_float,
    );
    pub fn oneui_tabs_set_closable(tabs: *mut OneUiWidget, closable: c_int);
    pub fn oneui_tabs_set_reorder_enabled(tabs: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_tabs_reorder_enabled(tabs: *mut OneUiWidget) -> c_int;
    pub fn oneui_tabs_set_on_changed(
        tabs: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_tabs_set_on_close_requested(
        tabs: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_tabs_set_on_context_menu_requested(
        tabs: *mut OneUiWidget,
        callback: OneUiIndexPointCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_tabs_set_on_reorder_requested(
        tabs: *mut OneUiWidget,
        callback: OneUiReorderRequestedCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_title_bar_create(title: *const WChar) -> *mut OneUiWidget;
    pub fn oneui_title_bar_set_title(title_bar: *mut OneUiWidget, title: *const WChar);
    pub fn oneui_title_bar_set_icon_symbol(title_bar: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_title_bar_set_variant(title_bar: *mut OneUiWidget, variant: *const c_char);
    pub fn oneui_title_bar_set_leading(title_bar: *mut OneUiWidget, leading: *mut OneUiWidget);
    pub fn oneui_title_bar_set_accessory(title_bar: *mut OneUiWidget, accessory: *mut OneUiWidget);
    pub fn oneui_title_bar_set_on_minimize(
        title_bar: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_title_bar_set_on_maximize(
        title_bar: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_title_bar_set_on_close(
        title_bar: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_nav_item_create(
        text: *const WChar,
        symbol: c_int,
        selected: c_int,
    ) -> *mut OneUiWidget;
    pub fn oneui_nav_item_set_selected(nav_item: *mut OneUiWidget, selected: c_int);
    pub fn oneui_nav_item_set_on_click(
        nav_item: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_button_create_utf8(text: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_button_set_text_utf8(button: *mut OneUiWidget, text: OneUiUtf8String);
    pub fn oneui_button_set_icon(button: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_button_set_content_align(button: *mut OneUiWidget, align: c_int);
    pub fn oneui_button_set_trailing_text_utf8(button: *mut OneUiWidget, text: OneUiUtf8String);
    pub fn oneui_button_set_variant(button: *mut OneUiWidget, variant: OneUiButtonVariant);
    pub fn oneui_button_set_on_click(
        button: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_button_set_style(button: *mut OneUiWidget, style: *const OneUiButtonStyle);
    pub fn oneui_button_clear_style(button: *mut OneUiWidget);
    pub fn oneui_menu_create() -> *mut OneUiWidget;
    pub fn oneui_menu_add_header(
        menu: *mut OneUiWidget,
        title: *const WChar,
        subtitle: *const WChar,
    );
    pub fn oneui_menu_add_item(
        menu: *mut OneUiWidget,
        text: *const WChar,
        icon_symbol: c_int,
        danger: c_int,
    ) -> c_int;
    pub fn oneui_menu_add_separator(menu: *mut OneUiWidget);
    pub fn oneui_menu_clear_items(menu: *mut OneUiWidget);
    pub fn oneui_menu_set_item_disabled(menu: *mut OneUiWidget, index: c_int, disabled: c_int);
    pub fn oneui_menu_preferred_height(menu: *mut OneUiWidget) -> f32;
    pub fn oneui_menu_set_on_activated(
        menu: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_select_create() -> *mut OneUiWidget;
    pub fn oneui_select_set_items_utf8(
        select: *mut OneUiWidget,
        items: *const OneUiUtf8String,
        count: usize,
    );
    pub fn oneui_select_set_selected_index(select: *mut OneUiWidget, index: c_int);
    pub fn oneui_select_selected_index(select: *mut OneUiWidget) -> c_int;
    pub fn oneui_select_set_on_changed(
        select: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_text_field_create_utf8(placeholder: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_text_field_set_text_utf8(text_field: *mut OneUiWidget, text: OneUiUtf8String);
    pub fn oneui_text_field_undo(text_field: *mut OneUiWidget) -> c_int;
    pub fn oneui_text_field_redo(text_field: *mut OneUiWidget) -> c_int;
    pub fn oneui_text_field_set_read_only(text_field: *mut OneUiWidget, read_only: c_int);
    pub fn oneui_text_field_set_password_mode(text_field: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_text_field_set_password_mask(text_field: *mut OneUiWidget, codepoint: c_uint);
    pub fn oneui_text_field_set_multiline(text_field: *mut OneUiWidget, multiline: c_int);
    pub fn oneui_text_field_set_submit_on_enter(
        text_field: *mut OneUiWidget,
        submit_on_enter: c_int,
    );
    pub fn oneui_text_field_set_line_height(text_field: *mut OneUiWidget, line_height: f32);
    pub fn oneui_text_field_set_font_size(text_field: *mut OneUiWidget, font_size: f32);
    pub fn oneui_text_field_set_prefix_icon(text_field: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_text_field_clear_prefix_icon(text_field: *mut OneUiWidget);
    pub fn oneui_text_field_set_suffix_icon(text_field: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_text_field_clear_suffix_icon(text_field: *mut OneUiWidget);
    pub fn oneui_text_area_create_utf8(placeholder: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_text_area_set_vertical_scroll_offset(text_area: *mut OneUiWidget, offset: f32);
    pub fn oneui_text_area_vertical_scroll_offset(text_area: *mut OneUiWidget) -> f32;
    pub fn oneui_text_area_max_vertical_scroll_offset(text_area: *mut OneUiWidget) -> f32;
    pub fn oneui_text_area_scroll_to_top(text_area: *mut OneUiWidget);
    pub fn oneui_text_field_set_on_changed_utf8(
        text_field: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_text_field_set_on_submitted_utf8(
        text_field: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_search_box_create_utf8(placeholder: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_state_view_create(title: *const WChar, message: *const WChar) -> *mut OneUiWidget;
    pub fn oneui_state_view_set_title(state_view: *mut OneUiWidget, title: *const WChar);
    pub fn oneui_state_view_set_message(state_view: *mut OneUiWidget, message: *const WChar);
    pub fn oneui_state_view_set_icon(state_view: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_state_view_set_action(state_view: *mut OneUiWidget, text: *const WChar);
    pub fn oneui_state_view_set_on_action(
        state_view: *mut OneUiWidget,
        callback: Option<unsafe extern "C" fn(user_data: *mut c_void)>,
        user_data: *mut c_void,
    );
}
