//! Terminal render bindings.

use super::sys;
use super::terminal_types::{TerminalCell, TerminalColor, TerminalFrame, TerminalViewOptions};

pub(super) fn apply_terminal_frame(
    raw: *mut sys::OneUiWidget,
    frame: &TerminalFrame,
    previous: Option<&TerminalFrame>,
    honor_application_cursor_style: bool,
) {
    unsafe {
        sys::oneui_terminal_view_set_first_visible_line_number(raw, frame.first_visible_line_number)
    };
    unsafe {
        sys::oneui_terminal_view_set_scrollback(raw, frame.history_rows, frame.scrollback_offset);
    }
    let can_update_in_place = previous.is_some_and(|previous| {
        previous.rows == frame.rows
            && previous.columns == frame.columns
            && previous.cells.len() == frame.cells.len()
    });
    if can_update_in_place {
        for range in terminal_dirty_ranges(previous.expect("previous frame checked"), frame) {
            apply_terminal_cell_update(raw, range.start, &frame.cells[range]);
        }
    } else {
        apply_terminal_grid(raw, frame.rows, frame.columns, &frame.cells);
    }
    unsafe {
        sys::oneui_terminal_view_set_mouse_reporting(raw, i32::from(frame.mouse_reporting));
        if should_apply_application_cursor_style(
            honor_application_cursor_style,
            frame.cursor_style_from_application,
        ) {
            sys::oneui_terminal_view_set_cursor_style(raw, frame.cursor_style as i32);
            sys::oneui_terminal_view_set_cursor_blinking(raw, i32::from(frame.cursor_blinking));
        }
        sys::oneui_terminal_view_set_cursor(
            raw,
            frame.cursor.row,
            frame.cursor.column,
            i32::from(frame.cursor.visible),
        )
    };
}

pub(super) fn should_apply_application_cursor_style(
    honor_application_cursor_style: bool,
    cursor_style_from_application: bool,
) -> bool {
    honor_application_cursor_style && cursor_style_from_application
}

pub(super) fn terminal_dirty_ranges(
    previous: &TerminalFrame,
    frame: &TerminalFrame,
) -> Vec<std::ops::Range<usize>> {
    const MAX_SPARSE_RANGES: usize = 32;

    let mut ranges = Vec::with_capacity(4);
    let mut current_start = None;
    let mut first_changed = None;
    let mut last_changed = 0;
    let mut collapsed = false;

    for (index, (previous, current)) in previous.cells.iter().zip(&frame.cells).enumerate() {
        if previous != current {
            first_changed.get_or_insert(index);
            last_changed = index;
            current_start.get_or_insert(index);
        } else if let Some(start) = current_start.take() {
            if !collapsed {
                ranges.push(start..index);
                if ranges.len() > MAX_SPARSE_RANGES {
                    ranges.clear();
                    collapsed = true;
                }
            }
        }
    }
    if let Some(start) = current_start {
        if !collapsed {
            ranges.push(start..last_changed + 1);
            if ranges.len() > MAX_SPARSE_RANGES {
                collapsed = true;
            }
        }
    }

    if collapsed {
        std::iter::once(
            first_changed.expect("collapsed ranges require a changed cell")..last_changed + 1,
        )
        .collect()
    } else {
        ranges
    }
}

pub(super) fn apply_terminal_view_options(
    raw: *mut sys::OneUiWidget,
    options: &TerminalViewOptions,
) {
    unsafe {
        sys::oneui_terminal_view_set_font_family_utf8(
            raw,
            sys::OneUiUtf8String::from_str(&options.font_family),
        );
        sys::oneui_terminal_view_set_font_size(raw, options.font_size);
        sys::oneui_terminal_view_set_line_height(raw, options.line_height);
        sys::oneui_terminal_view_set_letter_spacing(raw, options.letter_spacing);
        sys::oneui_terminal_view_set_cursor_style(raw, options.cursor_style as i32);
        sys::oneui_terminal_view_set_cursor_blinking(raw, i32::from(options.cursor_blinking));
        sys::oneui_terminal_view_set_copy_on_select(raw, i32::from(options.copy_on_select));
        sys::oneui_terminal_view_set_right_button_action(raw, options.right_button_action as i32);
        sys::oneui_terminal_view_set_middle_button_action(raw, options.middle_button_action as i32);
        sys::oneui_terminal_view_set_scroll_rows_per_wheel(raw, options.scroll_rows_per_wheel);
        sys::oneui_terminal_view_set_line_numbers_visible(
            raw,
            i32::from(options.line_numbers_visible),
        );
        sys::oneui_terminal_view_set_palette(
            raw,
            options.background.into(),
            options.foreground.into(),
            options.cursor.into(),
        );
    }
}

pub(super) fn apply_terminal_grid(
    raw: *mut sys::OneUiWidget,
    rows: u16,
    columns: u16,
    cells: &[TerminalCell],
) {
    let native_cells = native_terminal_cells(cells);
    unsafe {
        sys::oneui_terminal_view_set_grid_utf8(
            raw,
            rows,
            columns,
            native_cells.as_ptr(),
            native_cells.len(),
        )
    };
}

fn apply_terminal_cell_update(
    raw: *mut sys::OneUiWidget,
    first_cell: usize,
    cells: &[TerminalCell],
) {
    let native_cells = native_terminal_cells(cells);
    unsafe {
        sys::oneui_terminal_view_update_cells_utf8(
            raw,
            first_cell,
            native_cells.as_ptr(),
            native_cells.len(),
        )
    };
}

pub(super) fn native_terminal_cells(cells: &[TerminalCell]) -> Vec<sys::OneUiTerminalCellUtf8> {
    cells
        .iter()
        .map(|cell| sys::OneUiTerminalCellUtf8 {
            text: sys::OneUiUtf8String::from_str(&cell.text),
            foreground: cell.foreground.into(),
            background: cell.background.into(),
            style: cell.style,
            hyperlink_id: cell.hyperlink_id,
            underline_style: cell.underline_style as u32,
            underline_color: cell
                .underline_color
                .unwrap_or(TerminalColor::rgb(0, 0, 0))
                .into(),
            underline_color_set: i32::from(cell.underline_color.is_some()),
        })
        .collect()
}
