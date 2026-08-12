//! Raw bindings for OneUI's portable UTF-8 C ABI.
//!
//! All UTF-8 strings are borrowed for the duration of the FFI call. Callback
//! bytes are also borrowed and must be copied before the callback returns.

use std::ffi::{c_char, c_float, c_int, c_uint, c_ushort, c_void};

pub const UTF8_ABI_VERSION: c_uint = 8;

#[repr(C)]
pub struct OneUiWindow {
    _private: [u8; 0],
}

#[repr(C)]
pub struct OneUiWidget {
    _private: [u8; 0],
}

#[repr(C)]
pub struct OneUiStyleSheet {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiUtf8String {
    pub data: *const c_char,
    pub length: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiListItemUtf8 {
    pub title: OneUiUtf8String,
    pub detail: OneUiUtf8String,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiTreeItemUtf8 {
    pub id: OneUiUtf8String,
    pub parent_id: OneUiUtf8String,
    pub title: OneUiUtf8String,
    pub detail: OneUiUtf8String,
    pub expanded: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiPointerEvent {
    pub x: f32,
    pub y: f32,
    pub button: c_int,
    pub click_count: c_int,
    pub shift: c_int,
    pub control: c_int,
    pub alt: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiFocusRingStyle {
    pub color: OneUiColor,
    pub width: f32,
    pub offset: f32,
    pub radius: f32,
    pub visible: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiButtonStateStyle {
    pub background: OneUiColor,
    pub foreground: OneUiColor,
    pub border: OneUiColor,
    pub border_width: f32,
    pub radius: f32,
    pub focus_ring: OneUiFocusRingStyle,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiButtonStyle {
    pub normal: OneUiButtonStateStyle,
    pub hovered: OneUiButtonStateStyle,
    pub pressed: OneUiButtonStateStyle,
    pub disabled: OneUiButtonStateStyle,
    pub focus_visible: OneUiButtonStateStyle,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiInteractiveSurfaceStateStyle {
    pub background: OneUiColor,
    pub border: OneUiColor,
    pub border_width: f32,
    pub radius: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiInteractiveSurfaceStyle {
    pub normal: OneUiInteractiveSurfaceStateStyle,
    pub hovered: OneUiInteractiveSurfaceStateStyle,
    pub pressed: OneUiInteractiveSurfaceStateStyle,
    pub disabled: OneUiInteractiveSurfaceStateStyle,
    pub focus_visible: OneUiInteractiveSurfaceStateStyle,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiTerminalCellUtf8 {
    pub text: OneUiUtf8String,
    pub foreground: OneUiColor,
    pub background: OneUiColor,
    pub style: c_uint,
    pub hyperlink_id: c_uint,
    pub underline_style: c_uint,
    pub underline_color: OneUiColor,
    pub underline_color_set: c_int,
}

impl OneUiUtf8String {
    // This ABI view borrows the input bytes, so the owned FromStr contract is
    // intentionally not applicable.
    #[allow(clippy::should_implement_trait)]
    pub fn from_str(value: &str) -> Self {
        Self {
            data: value.as_ptr().cast(),
            length: value.len(),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiWindowOptionsUtf8 {
    pub title: OneUiUtf8String,
    pub width: c_int,
    pub height: c_int,
    pub visible: c_int,
    pub borderless: c_int,
    pub fullscreen: c_int,
    pub topmost: c_int,
    pub resizable: c_int,
}

pub const FILE_DIALOG_OPEN_FILE: c_int = 0;
pub const FILE_DIALOG_SAVE_FILE: c_int = 1;
pub const FILE_DIALOG_SELECT_FOLDER: c_int = 2;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiFileDialogFilterUtf8 {
    pub name: OneUiUtf8String,
    pub pattern: OneUiUtf8String,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OneUiFileDialogOptionsUtf8 {
    pub mode: c_int,
    pub title: OneUiUtf8String,
    pub initial_directory: OneUiUtf8String,
    pub default_name: OneUiUtf8String,
    pub default_extension: OneUiUtf8String,
    pub filters: *const OneUiFileDialogFilterUtf8,
    pub filter_count: usize,
    pub confirm_overwrite: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct OneUiWindowPlacement {
    pub x: c_int,
    pub y: c_int,
    pub width: c_int,
    pub height: c_int,
    pub maximized: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiInsets {
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub left: f32,
}

pub type OneUiUtf8TextCallback =
    Option<unsafe extern "C" fn(text: *const c_char, length: usize, user_data: *mut c_void)>;
pub type OneUiTreeExpansionCallback = Option<
    unsafe extern "C" fn(id: *const c_char, length: usize, expanded: c_int, user_data: *mut c_void),
>;
pub type OneUiVoidCallback = Option<unsafe extern "C" fn(user_data: *mut c_void)>;
pub type OneUiBoolCallback = Option<unsafe extern "C" fn(value: c_int, user_data: *mut c_void)>;
pub type OneUiIntCallback = Option<unsafe extern "C" fn(value: c_int, user_data: *mut c_void)>;
pub type OneUiFloatCallback = Option<unsafe extern "C" fn(value: f32, user_data: *mut c_void)>;
pub type OneUiIndexPointCallback =
    Option<unsafe extern "C" fn(index: c_int, x: f32, y: f32, user_data: *mut c_void)>;
pub type OneUiReorderRequestedCallback =
    Option<unsafe extern "C" fn(source_index: c_int, target_index: c_int, user_data: *mut c_void)>;
pub type OneUiTreeReorderRequestedCallback = Option<
    unsafe extern "C" fn(
        source_id: *const c_char,
        source_length: usize,
        target_id: *const c_char,
        target_length: usize,
        user_data: *mut c_void,
    ),
>;
pub type OneUiGridReorderRequestedCallback = Option<
    unsafe extern "C" fn(
        source_id: *const c_char,
        source_length: usize,
        target_index: c_int,
        user_data: *mut c_void,
    ),
>;
pub type OneUiItemDragCallback = Option<
    unsafe extern "C" fn(
        source_id: *const c_char,
        source_length: usize,
        phase: c_int,
        x: f32,
        y: f32,
        user_data: *mut c_void,
    ),
>;
pub type OneUiPointerCallback =
    Option<unsafe extern "C" fn(event: *const OneUiPointerEvent, user_data: *mut c_void)>;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiRawKeyEvent {
    pub virtual_key: c_uint,
    pub scan_code: c_uint,
    pub pressed: c_int,
    pub repeat: c_int,
    pub extended: c_int,
    pub alt: c_int,
    pub ctrl: c_int,
    pub shift: c_int,
    pub win: c_int,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct OneUiTerminalPointerEvent {
    pub action: c_int,
    pub button: c_int,
    pub x: c_float,
    pub y: c_float,
    pub row: c_ushort,
    pub column: c_ushort,
    pub wheel_delta: c_int,
    pub shift: c_int,
    pub control: c_int,
    pub alt: c_int,
}

pub type OneUiRawKeyCallback =
    Option<unsafe extern "C" fn(event: *const OneUiRawKeyEvent, user_data: *mut c_void)>;
pub type OneUiTerminalPointerCallback =
    Option<unsafe extern "C" fn(event: *const OneUiTerminalPointerEvent, user_data: *mut c_void)>;
pub type OneUiTerminalHyperlinkCallback =
    Option<unsafe extern "C" fn(hyperlink_id: c_uint, user_data: *mut c_void)>;
pub type OneUiTerminalViewportCallback =
    Option<unsafe extern "C" fn(rows: c_ushort, columns: c_ushort, user_data: *mut c_void)>;

extern "C" {
    pub fn oneui_utf8_abi_version() -> c_uint;

    pub fn oneui_window_create_utf8(options: *const OneUiWindowOptionsUtf8) -> *mut OneUiWindow;
    pub fn oneui_window_destroy(window: *mut OneUiWindow);
    pub fn oneui_window_initialize(window: *mut OneUiWindow);
    pub fn oneui_window_show(window: *mut OneUiWindow);
    pub fn oneui_window_activate(window: *mut OneUiWindow);
    pub fn oneui_window_file_dialog_utf8(
        window: *mut OneUiWindow,
        options: *const OneUiFileDialogOptionsUtf8,
        buffer: *mut c_char,
        buffer_len: usize,
        required_len: *mut usize,
    ) -> c_int;
    pub fn oneui_window_confirm(
        window: *mut OneUiWindow,
        title: *const u16,
        message: *const u16,
    ) -> c_int;
    pub fn oneui_window_prompt_text(
        window: *mut OneUiWindow,
        title: *const u16,
        message: *const u16,
        initial_value: *const u16,
        placeholder: *const u16,
        password: c_int,
        out: *mut u16,
        out_len: c_int,
    ) -> c_int;
    pub fn oneui_window_run(window: *mut OneUiWindow) -> c_int;
    pub fn oneui_window_close(window: *mut OneUiWindow);
    pub fn oneui_window_request_close(window: *mut OneUiWindow);
    pub fn oneui_window_minimize(window: *mut OneUiWindow);
    pub fn oneui_window_toggle_maximize(window: *mut OneUiWindow);
    pub fn oneui_window_get_placement(
        window: *mut OneUiWindow,
        placement: *mut OneUiWindowPlacement,
    ) -> c_int;
    pub fn oneui_window_set_placement(
        window: *mut OneUiWindow,
        placement: *const OneUiWindowPlacement,
    ) -> c_int;
    pub fn oneui_window_set_borderless(window: *mut OneUiWindow, borderless: c_int);
    pub fn oneui_window_set_title_bar_drag_metrics(
        window: *mut OneUiWindow,
        title_bar_height: f32,
        reserved_button_width: f32,
    );
    pub fn oneui_window_set_corner_radius(window: *mut OneUiWindow, radius: f32);
    pub fn oneui_window_set_title_utf8(window: *mut OneUiWindow, title: OneUiUtf8String);
    pub fn oneui_window_set_content(window: *mut OneUiWindow, widget: *mut OneUiWidget);
    pub fn oneui_window_request_focus(
        window: *mut OneUiWindow,
        widget: *mut OneUiWidget,
        focus_visible: c_int,
    ) -> c_int;
    pub fn oneui_window_set_style_sheet(
        window: *mut OneUiWindow,
        style_sheet: *mut OneUiStyleSheet,
    );
    pub fn oneui_window_post_owned(
        window: *mut OneUiWindow,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
        cleanup: OneUiVoidCallback,
    ) -> c_int;

    pub fn oneui_widget_destroy(widget: *mut OneUiWidget);
    pub fn oneui_widget_set_preferred_size(widget: *mut OneUiWidget, width: f32, height: f32);
    pub fn oneui_widget_set_disabled(widget: *mut OneUiWidget, disabled: c_int);
    pub fn oneui_widget_set_visible(widget: *mut OneUiWidget, visible: c_int);
    pub fn oneui_widget_set_classes(widget: *mut OneUiWidget, classes: *const c_char);
    pub fn oneui_widget_set_style_node(
        widget: *mut OneUiWidget,
        tag: *const c_char,
        classes: *const c_char,
    );
    pub fn oneui_widget_apply_style_sheet(
        widget: *mut OneUiWidget,
        style_sheet: *mut OneUiStyleSheet,
    );

    pub fn oneui_style_sheet_create() -> *mut OneUiStyleSheet;
    pub fn oneui_style_sheet_destroy(style_sheet: *mut OneUiStyleSheet);
    pub fn oneui_style_sheet_set_custom_property(
        style_sheet: *mut OneUiStyleSheet,
        name: *const c_char,
        value: *const c_char,
    );
    pub fn oneui_style_sheet_add_css(
        style_sheet: *mut OneUiStyleSheet,
        css: *const c_char,
        error_buffer: *mut c_char,
        error_buffer_len: c_int,
    ) -> c_int;
    // 0 = column, 1 = row. These values are part of the stable C ABI.
    pub fn oneui_stack_create(direction: c_int) -> *mut OneUiWidget;
    pub fn oneui_stack_add(stack: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_stack_set_gap(stack: *mut OneUiWidget, gap: f32);
    pub fn oneui_stack_set_padding(stack: *mut OneUiWidget, insets: OneUiInsets);
    pub fn oneui_stack_set_align(stack: *mut OneUiWidget, align: c_int);

    // 0 = horizontal, 1 = vertical. These values are part of the stable C ABI.
    pub fn oneui_split_view_create(orientation: c_int) -> *mut OneUiWidget;
    pub fn oneui_split_view_set_first(split_view: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_split_view_set_second(split_view: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_split_view_set_orientation(split_view: *mut OneUiWidget, orientation: c_int);
    pub fn oneui_split_view_set_ratio(split_view: *mut OneUiWidget, ratio: f32);
    pub fn oneui_split_view_ratio(split_view: *mut OneUiWidget) -> f32;
    pub fn oneui_split_view_set_gap(split_view: *mut OneUiWidget, gap: f32);
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
    pub fn oneui_overlay_host_remove_overlay(
        host: *mut OneUiWidget,
        child: *mut OneUiWidget,
    ) -> c_int;

    pub fn oneui_popup_create() -> *mut OneUiWidget;
    pub fn oneui_popup_set_anchor(popup: *mut OneUiWidget, anchor: *mut OneUiWidget);
    pub fn oneui_popup_set_content(popup: *mut OneUiWidget, content: *mut OneUiWidget);
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

    pub fn oneui_dialog_create(title: *const u16, subtitle: *const u16) -> *mut OneUiWidget;
    pub fn oneui_dialog_set_title(dialog: *mut OneUiWidget, title: *const u16);
    pub fn oneui_dialog_set_subtitle(dialog: *mut OneUiWidget, subtitle: *const u16);
    pub fn oneui_dialog_set_icon(dialog: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_dialog_set_close_visible(dialog: *mut OneUiWidget, visible: c_int);
    pub fn oneui_dialog_set_on_close(
        dialog: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_dialog_set_content(dialog: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_dialog_set_actions(dialog: *mut OneUiWidget, child: *mut OneUiWidget);

    pub fn oneui_log_view_create() -> *mut OneUiWidget;
    pub fn oneui_log_view_append_line(
        view: *mut OneUiWidget,
        text: *const u16,
        r: u8,
        g: u8,
        b: u8,
        a: u8,
    );
    pub fn oneui_log_view_clear(view: *mut OneUiWidget);
    pub fn oneui_log_view_content_height(view: *mut OneUiWidget) -> f32;
    pub fn oneui_log_view_set_font_size(view: *mut OneUiWidget, size: f32);
    pub fn oneui_log_view_set_line_height(view: *mut OneUiWidget, height: f32);

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
    pub fn oneui_interactive_surface_set_on_context_menu_requested(
        surface: *mut OneUiWidget,
        callback: OneUiPointerCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_scroll_view_create() -> *mut OneUiWidget;
    pub fn oneui_scroll_view_set_content(view: *mut OneUiWidget, child: *mut OneUiWidget);
    pub fn oneui_scroll_view_set_content_height(view: *mut OneUiWidget, height: f32);
    pub fn oneui_scroll_view_set_wheel_step(view: *mut OneUiWidget, step: f32);
    pub fn oneui_scroll_view_set_chrome_visible(view: *mut OneUiWidget, visible: c_int);
    pub fn oneui_scroll_view_set_scrollbar_style(
        view: *mut OneUiWidget,
        r: u8,
        g: u8,
        b: u8,
        a: u8,
        thickness: f32,
    );
    pub fn oneui_scroll_view_scroll_to_bottom(view: *mut OneUiWidget);
    pub fn oneui_label_create_utf8(text: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_label_set_text_utf8(label: *mut OneUiWidget, text: OneUiUtf8String);
    pub fn oneui_label_set_color(label: *mut OneUiWidget, r: u8, g: u8, b: u8, a: u8);
    pub fn oneui_label_set_font_size(label: *mut OneUiWidget, font_size: f32);
    pub fn oneui_label_set_font_weight(label: *mut OneUiWidget, font_weight: c_int);
    pub fn oneui_label_set_align(label: *mut OneUiWidget, align: c_int);

    pub fn oneui_icon_create(symbol: c_int) -> *mut OneUiWidget;
    pub fn oneui_icon_set_symbol(icon: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_icon_set_color(icon: *mut OneUiWidget, r: u8, g: u8, b: u8, a: u8);

    pub fn oneui_icon_button_create(symbol: c_int) -> *mut OneUiWidget;
    pub fn oneui_icon_button_set_symbol(icon_button: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_icon_button_set_on_click(
        icon_button: *mut OneUiWidget,
        callback: OneUiVoidCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_switch_create(text: *const u16) -> *mut OneUiWidget;
    pub fn oneui_switch_set_text(switch_widget: *mut OneUiWidget, text: *const u16);
    pub fn oneui_switch_set_checked(switch_widget: *mut OneUiWidget, checked: c_int);
    pub fn oneui_switch_checked(switch_widget: *mut OneUiWidget) -> c_int;
    pub fn oneui_switch_set_on_changed(
        switch_widget: *mut OneUiWidget,
        callback: OneUiBoolCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_segmented_control_create() -> *mut OneUiWidget;
    pub fn oneui_segmented_control_set_items(
        segmented_control: *mut OneUiWidget,
        items: *const u16,
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
    pub fn oneui_tabs_set_items_utf8(
        tabs: *mut OneUiWidget,
        items: *const OneUiUtf8String,
        count: usize,
    );
    pub fn oneui_tabs_set_selected_index(tabs: *mut OneUiWidget, index: c_int);
    pub fn oneui_tabs_selected_index(tabs: *mut OneUiWidget) -> c_int;
    pub fn oneui_tabs_set_on_changed(
        tabs: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );

    pub fn oneui_title_bar_create(title: *const u16) -> *mut OneUiWidget;
    pub fn oneui_title_bar_set_title(title_bar: *mut OneUiWidget, title: *const u16);
    pub fn oneui_title_bar_set_icon_symbol(title_bar: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_title_bar_set_variant(title_bar: *mut OneUiWidget, variant: *const c_char);
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
        text: *const u16,
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
    pub fn oneui_button_set_variant(button: *mut OneUiWidget, variant: c_int);
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
        title: *const c_ushort,
        subtitle: *const c_ushort,
    );
    pub fn oneui_menu_add_item(
        menu: *mut OneUiWidget,
        text: *const c_ushort,
        icon_symbol: c_int,
        danger: c_int,
    ) -> c_int;
    pub fn oneui_menu_add_separator(menu: *mut OneUiWidget);
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
    pub fn oneui_text_field_set_read_only(text_field: *mut OneUiWidget, read_only: c_int);
    pub fn oneui_text_field_set_password_mode(text_field: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_text_field_set_password_mask(text_field: *mut OneUiWidget, codepoint: c_uint);
    pub fn oneui_text_field_set_multiline(text_field: *mut OneUiWidget, multiline: c_int);
    pub fn oneui_text_field_set_line_height(text_field: *mut OneUiWidget, line_height: f32);
    pub fn oneui_text_area_create_utf8(placeholder: OneUiUtf8String) -> *mut OneUiWidget;
    pub fn oneui_text_field_set_on_changed_utf8(
        text_field: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_create() -> *mut OneUiWidget;
    pub fn oneui_terminal_view_set_font_size(view: *mut OneUiWidget, size: f32);
    pub fn oneui_terminal_view_set_font_family_utf8(
        view: *mut OneUiWidget,
        family: OneUiUtf8String,
    );
    pub fn oneui_terminal_view_set_line_height(view: *mut OneUiWidget, multiplier: f32);
    pub fn oneui_terminal_view_set_cursor_style(view: *mut OneUiWidget, style: c_int);
    pub fn oneui_terminal_view_set_cursor_blinking(view: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_terminal_view_set_copy_on_select(view: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_terminal_view_set_right_button_action(view: *mut OneUiWidget, action: c_int);
    pub fn oneui_terminal_view_set_middle_button_action(view: *mut OneUiWidget, action: c_int);
    pub fn oneui_terminal_view_set_palette(
        view: *mut OneUiWidget,
        background: OneUiColor,
        foreground: OneUiColor,
        cursor: OneUiColor,
    );
    pub fn oneui_terminal_view_set_grid_utf8(
        view: *mut OneUiWidget,
        rows: c_ushort,
        columns: c_ushort,
        cells: *const OneUiTerminalCellUtf8,
        cell_count: usize,
    );
    pub fn oneui_terminal_view_update_cells_utf8(
        view: *mut OneUiWidget,
        first_cell: usize,
        cells: *const OneUiTerminalCellUtf8,
        cell_count: usize,
    );
    pub fn oneui_terminal_view_set_cursor(
        view: *mut OneUiWidget,
        row: c_ushort,
        column: c_ushort,
        visible: c_int,
    );
    pub fn oneui_terminal_view_select_all(view: *mut OneUiWidget);
    pub fn oneui_terminal_view_copy_selection(view: *mut OneUiWidget) -> c_int;
    pub fn oneui_terminal_view_paste_clipboard(view: *mut OneUiWidget) -> c_int;
    pub fn oneui_terminal_view_set_selection(
        view: *mut OneUiWidget,
        start_row: c_ushort,
        start_column: c_ushort,
        end_row: c_ushort,
        end_column: c_ushort,
    );
    pub fn oneui_terminal_view_clear_selection(view: *mut OneUiWidget);
    pub fn oneui_terminal_view_has_selection(view: *mut OneUiWidget) -> c_int;
    pub fn oneui_terminal_view_get_selected_text_utf8(
        view: *mut OneUiWidget,
        buffer: *mut c_char,
        buffer_len: usize,
    ) -> usize;
    pub fn oneui_terminal_view_set_on_text_input_utf8(
        view: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_paste_utf8(
        view: *mut OneUiWidget,
        callback: OneUiUtf8TextCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_raw_key(
        view: *mut OneUiWidget,
        callback: OneUiRawKeyCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_scroll_rows_per_wheel(view: *mut OneUiWidget, rows: f32);
    pub fn oneui_terminal_view_set_on_scroll(
        view: *mut OneUiWidget,
        callback: OneUiIntCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_mouse_reporting(view: *mut OneUiWidget, enabled: c_int);
    pub fn oneui_terminal_view_set_on_pointer(
        view: *mut OneUiWidget,
        callback: OneUiTerminalPointerCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_hyperlink(
        view: *mut OneUiWidget,
        callback: OneUiTerminalHyperlinkCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_viewport_changed(
        view: *mut OneUiWidget,
        callback: OneUiTerminalViewportCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_terminal_view_set_on_focus_changed(
        view: *mut OneUiWidget,
        callback: OneUiBoolCallback,
        user_data: *mut c_void,
    );
    pub fn oneui_search_box_create_utf8(placeholder: OneUiUtf8String) -> *mut OneUiWidget;
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
    pub fn oneui_state_view_create(
        title: *const c_ushort,
        message: *const c_ushort,
    ) -> *mut OneUiWidget;
    pub fn oneui_state_view_set_title(state_view: *mut OneUiWidget, title: *const c_ushort);
    pub fn oneui_state_view_set_message(state_view: *mut OneUiWidget, message: *const c_ushort);
    pub fn oneui_state_view_set_icon(state_view: *mut OneUiWidget, symbol: c_int);
    pub fn oneui_state_view_set_action(state_view: *mut OneUiWidget, text: *const c_ushort);
    pub fn oneui_state_view_set_on_action(
        state_view: *mut OneUiWidget,
        callback: Option<unsafe extern "C" fn(user_data: *mut c_void)>,
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

    pub fn oneui_clipboard_set_text_utf8(text: OneUiUtf8String) -> c_int;
    pub fn oneui_clipboard_get_text_utf8(buffer: *mut c_char, buffer_len: usize) -> usize;
}
