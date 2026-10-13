//! Log view bindings.

use super::visual_style::Color;
use super::{sys, wide_null_terminated, Error, Widget};

/// A single immutable line for the selectable native log view.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LogLine {
    pub text: String,
    pub color: Color,
}

/// A native, selectable, multi-line log surface.
///
/// Use this for command output and diagnostics. It deliberately keeps one
/// structured line per append, rather than asking a label control to emulate
/// terminal layout with embedded newlines.
pub struct LogView {
    widget: Widget,
}

impl LogView {
    pub fn new() -> Result<Self, Error> {
        let widget = Widget::from_raw(unsafe { sys::oneui_log_view_create() })?;
        Ok(Self { widget })
    }

    pub fn append_line(&self, line: &LogLine) {
        let text = wide_null_terminated(&line.text);
        unsafe {
            sys::oneui_log_view_append_line(
                self.widget.as_raw(),
                text.as_ptr(),
                line.color.r,
                line.color.g,
                line.color.b,
                line.color.a,
            )
        };
    }

    pub fn set_lines(&self, lines: &[LogLine]) {
        self.clear();
        for line in lines {
            self.append_line(line);
        }
    }

    pub fn clear(&self) {
        unsafe { sys::oneui_log_view_clear(self.widget.as_raw()) };
    }

    pub fn content_height(&self) -> f32 {
        unsafe { sys::oneui_log_view_content_height(self.widget.as_raw()) }
    }

    pub fn set_font_size(&self, size: f32) {
        unsafe { sys::oneui_log_view_set_font_size(self.widget.as_raw(), size) };
    }

    pub fn set_line_height(&self, height: f32) {
        unsafe { sys::oneui_log_view_set_line_height(self.widget.as_raw(), height) };
    }

    pub fn as_widget(&self) -> &Widget {
        &self.widget
    }
}
