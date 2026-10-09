use super::snapshot::{rect, rectangle};
use super::*;
use crate::sys;

/// Constrain a floating window without persisting compact-window geometry.
pub fn clamp_floating(
    bounds: Rect,
    workspace: Rect,
    minimum: PaneMinimum,
) -> Result<Rect, DockError> {
    let mut output = rect(empty_rect());
    native::checked(unsafe {
        sys::oneui_dock_clamp_floating(
            rect(bounds),
            rect(workspace),
            sys::OneUiDockSize {
                width: minimum.width,
                height: minimum.height,
            },
            &mut output,
        )
    })?;
    Ok(rectangle(output))
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum FloatResizeEdge {
    North,
    South,
    East,
    West,
    NorthEast,
    NorthWest,
    SouthEast,
    SouthWest,
}
/// Preview from the original rectangle, retaining reversal after hitting a limit.
pub fn resize_floating(
    start: Rect,
    delta: (f32, f32),
    edge: FloatResizeEdge,
    workspace: Rect,
    minimum: PaneMinimum,
) -> Result<Rect, DockError> {
    let mut output = rect(empty_rect());
    native::checked(unsafe {
        sys::oneui_dock_resize_floating(
            rect(start),
            delta.0,
            delta.1,
            edge as i32,
            rect(workspace),
            sys::OneUiDockSize {
                width: minimum.width,
                height: minimum.height,
            },
            &mut output,
        )
    })?;
    Ok(rectangle(output))
}
