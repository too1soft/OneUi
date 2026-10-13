//! Visual style bindings.

use super::{sys, Range};

#[derive(Debug, Clone, Copy, Default)]
pub struct Insets {
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub left: f32,
}

/// A rectangle in logical window coordinates.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Rect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Color {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl Color {
    pub const fn rgb(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b, a: 255 }
    }

    pub const fn rgba(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { r, g, b, a }
    }
}

/// A non-overlapping inline style range for [`crate::Label`] text.
///
/// `range` uses UTF-8 byte offsets so callers can style slices without
/// converting to platform-specific UTF-16 indices. Zero/`None` metrics inherit
/// the label's block style; absent or transparent foreground inherits the
/// label's paint-time color.
#[derive(Clone, Debug, PartialEq)]
pub struct LabelTextSpan {
    pub range: Range<usize>,
    pub font_size: Option<f32>,
    pub font_weight: Option<i32>,
    pub italic: bool,
    pub monospace: bool,
    pub underline: bool,
    pub strikethrough: bool,
    pub foreground: Option<Color>,
    pub background: Option<Color>,
}

impl LabelTextSpan {
    pub fn new(range: Range<usize>) -> Self {
        Self {
            range,
            font_size: None,
            font_weight: None,
            italic: false,
            monospace: false,
            underline: false,
            strikethrough: false,
            foreground: None,
            background: None,
        }
    }
}

impl From<Color> for sys::OneUiColor {
    fn from(value: Color) -> Self {
        Self {
            r: value.r,
            g: value.g,
            b: value.b,
            a: value.a,
        }
    }
}

/// The focus treatment used by interactive controls. It is exposed as a
/// first-class value so product shells can remain keyboard accessible while
/// using a custom visual theme.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct FocusRingStyle {
    pub color: Color,
    pub width: f32,
    pub offset: f32,
    pub radius: f32,
    pub visible: bool,
}

impl FocusRingStyle {
    pub const fn hidden() -> Self {
        Self {
            color: Color::rgba(0, 0, 0, 0),
            width: 0.0,
            offset: 0.0,
            radius: 0.0,
            visible: false,
        }
    }
}

impl From<FocusRingStyle> for sys::OneUiFocusRingStyle {
    fn from(value: FocusRingStyle) -> Self {
        Self {
            color: value.color.into(),
            width: value.width,
            offset: value.offset,
            radius: value.radius,
            visible: i32::from(value.visible),
        }
    }
}

/// Colors and geometry for one state in a native button transition.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ButtonStateStyle {
    pub background: Color,
    pub foreground: Color,
    pub border: Color,
    pub border_width: f32,
    pub radius: f32,
    pub focus_ring: FocusRingStyle,
}

impl ButtonStateStyle {
    pub const fn solid(background: Color, foreground: Color, border: Color, radius: f32) -> Self {
        Self {
            background,
            foreground,
            border,
            border_width: 1.0,
            radius,
            focus_ring: FocusRingStyle::hidden(),
        }
    }
}

impl From<ButtonStateStyle> for sys::OneUiButtonStateStyle {
    fn from(value: ButtonStateStyle) -> Self {
        Self {
            background: value.background.into(),
            foreground: value.foreground.into(),
            border: value.border.into(),
            border_width: value.border_width,
            radius: value.radius,
            focus_ring: value.focus_ring.into(),
        }
    }
}

/// A full five-state visual contract for a native [`crate::Button`]. OneUI animates
/// between these values on hover and press; applications only describe the
/// visual states and never hand-roll pointer animations.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ButtonStyle {
    pub normal: ButtonStateStyle,
    pub hovered: ButtonStateStyle,
    pub pressed: ButtonStateStyle,
    pub disabled: ButtonStateStyle,
    pub focus_visible: ButtonStateStyle,
}

impl From<ButtonStyle> for sys::OneUiButtonStyle {
    fn from(value: ButtonStyle) -> Self {
        Self {
            normal: value.normal.into(),
            hovered: value.hovered.into(),
            pressed: value.pressed.into(),
            disabled: value.disabled.into(),
            focus_visible: value.focus_visible.into(),
        }
    }
}

/// The visual state for a content-bearing interactive surface such as a card
/// or list row. It intentionally excludes text colors because the surface may
/// host any arbitrary child composition.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct InteractiveSurfaceStateStyle {
    pub background: Color,
    pub border: Color,
    pub border_width: f32,
    pub radius: f32,
}

impl InteractiveSurfaceStateStyle {
    pub const fn solid(background: Color, border: Color, radius: f32) -> Self {
        Self {
            background,
            border,
            border_width: 1.0,
            radius,
        }
    }
}

impl From<InteractiveSurfaceStateStyle> for sys::OneUiInteractiveSurfaceStateStyle {
    fn from(value: InteractiveSurfaceStateStyle) -> Self {
        Self {
            background: value.background.into(),
            border: value.border.into(),
            border_width: value.border_width,
            radius: value.radius,
        }
    }
}

/// Native hover and press states for an [`crate::InteractiveSurface`]. OneUI owns
/// the animation clock and preserves events for controls nested inside it.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct InteractiveSurfaceStyle {
    pub normal: InteractiveSurfaceStateStyle,
    pub hovered: InteractiveSurfaceStateStyle,
    pub pressed: InteractiveSurfaceStateStyle,
    pub disabled: InteractiveSurfaceStateStyle,
    pub focus_visible: InteractiveSurfaceStateStyle,
}

impl From<InteractiveSurfaceStyle> for sys::OneUiInteractiveSurfaceStyle {
    fn from(value: InteractiveSurfaceStyle) -> Self {
        Self {
            normal: value.normal.into(),
            hovered: value.hovered.into(),
            pressed: value.pressed.into(),
            disabled: value.disabled.into(),
            focus_visible: value.focus_visible.into(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ButtonContentAlign {
    Start = 0,
    Center = 1,
    End = 2,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LabelAlign {
    Start = 0,
    Center = 1,
    End = 2,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ButtonVariant {
    Primary = 0,
    Secondary = 1,
}

impl From<Insets> for sys::OneUiInsets {
    fn from(value: Insets) -> Self {
        Self {
            top: value.top,
            right: value.right,
            bottom: value.bottom,
            left: value.left,
        }
    }
}
