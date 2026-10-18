//! Terminal frame, cell, viewport and input protocol types.

use super::sys;

/// RGBA color used by the native terminal grid.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl TerminalColor {
    pub const fn rgb(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b, a: 255 }
    }
}

impl Default for TerminalColor {
    fn default() -> Self {
        Self::rgb(220, 226, 240)
    }
}

impl From<TerminalColor> for sys::OneUiColor {
    fn from(value: TerminalColor) -> Self {
        Self {
            r: value.r,
            g: value.g,
            b: value.b,
            a: value.a,
        }
    }
}

/// Terminal cell style flags mirrored by OneUI's stable C ABI.
pub mod terminal_style {
    pub const BOLD: u32 = 1 << 0;
    pub const DIM: u32 = 1 << 1;
    pub const ITALIC: u32 = 1 << 2;
    pub const UNDERLINE: u32 = 1 << 3;
    pub const INVERSE: u32 = 1 << 4;
    pub const WIDE: u32 = 1 << 5;
    pub const WIDE_CONTINUATION: u32 = 1 << 6;
    pub const BLINK_SLOW: u32 = 1 << 7;
    pub const BLINK_RAPID: u32 = 1 << 8;
    pub const CONCEAL: u32 = 1 << 9;
    pub const STRIKETHROUGH: u32 = 1 << 10;
    pub const OVERLINE: u32 = 1 << 11;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(u32)]
pub enum TerminalUnderlineStyle {
    #[default]
    None = 0,
    Single = 1,
    Double = 2,
    Curly = 3,
    Dotted = 4,
    Dashed = 5,
}

/// One terminal cell. It deliberately holds text and colors separately so a
/// caller never has to encode values into a string boundary.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TerminalCell {
    pub text: String,
    pub foreground: TerminalColor,
    pub background: TerminalColor,
    pub style: u32,
    /// Stable emulator-owned OSC 8 hyperlink identifier. Zero means no link.
    pub hyperlink_id: u32,
    pub underline_style: TerminalUnderlineStyle,
    /// `None` follows the effective foreground, including inverse video.
    pub underline_color: Option<TerminalColor>,
}

impl Default for TerminalCell {
    fn default() -> Self {
        Self {
            text: String::new(),
            foreground: TerminalColor::default(),
            background: TerminalColor::rgb(20, 24, 36),
            style: 0,
            hyperlink_id: 0,
            underline_style: TerminalUnderlineStyle::None,
            underline_color: None,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalCursor {
    pub row: u16,
    pub column: u16,
    pub visible: bool,
}

/// A half-open terminal cell range. The end column is exclusive.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalSelection {
    pub start_row: u16,
    pub start_column: u16,
    pub end_row: u16,
    pub end_column: u16,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum TerminalCursorStyle {
    Block = 0,
    Bar = 1,
    Underline = 2,
}

/// The whole-cell terminal viewport reported by the native renderer.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalViewport {
    pub rows: u16,
    pub columns: u16,
}

/// An owned terminal snapshot that can be submitted from a session worker.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TerminalFrame {
    pub rows: u16,
    pub columns: u16,
    pub cells: Vec<TerminalCell>,
    pub cursor: TerminalCursor,
    pub cursor_style: TerminalCursorStyle,
    pub cursor_blinking: bool,
    /// Applies frame cursor style and blinking only when the terminal
    /// application explicitly requested them. Otherwise the view keeps its
    /// user-configured defaults from `TerminalViewOptions`.
    pub cursor_style_from_application: bool,
    /// Whether terminal applications currently own pointer input. Shift still
    /// bypasses reporting so users can select text locally.
    pub mouse_reporting: bool,
    /// One-based physical row number for the first visible row. Zero hides
    /// numbers for this frame (for example while using an alternate screen).
    pub first_visible_line_number: u64,
    /// Retained history rows above the live viewport, and distance from live output.
    pub history_rows: u64,
    pub scrollback_offset: u64,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TerminalPointerAction {
    Press,
    Release,
    Move,
    Wheel,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TerminalPointerButton {
    None,
    Left,
    Right,
    Middle,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum TerminalAuxiliaryButtonAction {
    Ignore = 0,
    Copy = 1,
    Paste = 2,
    Callback = 3,
}

/// Complete visual and pointer configuration for a terminal view.
///
/// Applying this value through a `TerminalViewHandle` uses one UI-thread
/// dispatch, so a settings change cannot enqueue a burst of independent
/// terminal mutations.
#[derive(Debug, Clone, PartialEq)]
pub struct TerminalViewOptions {
    pub font_family: String,
    pub font_size: f32,
    pub line_height: f32,
    pub letter_spacing: f32,
    pub cursor_style: TerminalCursorStyle,
    pub cursor_blinking: bool,
    /// Whether an application-originated DECSCUSR sequence may override the
    /// user's configured cursor shape. Keeping this explicit prevents frame
    /// updates from silently undoing a preference applied through settings.
    pub honor_application_cursor_style: bool,
    pub copy_on_select: bool,
    pub right_button_action: TerminalAuxiliaryButtonAction,
    pub middle_button_action: TerminalAuxiliaryButtonAction,
    pub scroll_rows_per_wheel: f32,
    pub line_numbers_visible: bool,
    pub background: TerminalColor,
    pub foreground: TerminalColor,
    pub cursor: TerminalColor,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct TerminalPointerEvent {
    pub action: TerminalPointerAction,
    pub button: TerminalPointerButton,
    pub x: f32,
    pub y: f32,
    pub row: u16,
    pub column: u16,
    pub wheel_delta: i32,
    pub shift: bool,
    pub control: bool,
    pub alt: bool,
}

impl From<sys::OneUiTerminalPointerEvent> for TerminalPointerEvent {
    fn from(value: sys::OneUiTerminalPointerEvent) -> Self {
        let action = match value.action {
            0 => TerminalPointerAction::Press,
            1 => TerminalPointerAction::Release,
            3 => TerminalPointerAction::Wheel,
            _ => TerminalPointerAction::Move,
        };
        let button = match value.button {
            1 => TerminalPointerButton::Left,
            2 => TerminalPointerButton::Right,
            3 => TerminalPointerButton::Middle,
            _ => TerminalPointerButton::None,
        };
        Self {
            action,
            button,
            x: value.x,
            y: value.y,
            row: value.row,
            column: value.column,
            wheel_delta: value.wheel_delta,
            shift: value.shift != 0,
            control: value.control != 0,
            alt: value.alt != 0,
        }
    }
}
