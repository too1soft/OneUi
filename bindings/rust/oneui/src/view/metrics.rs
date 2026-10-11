use super::{check, View, VmResult};
use crate::{sys, Rect};

/// Read-only native geometry. No widget handle or ownership is exposed.
#[derive(Clone, Copy, Debug)]
pub struct ElementMetrics {
    pub frame: Rect,
    pub scrollable: bool,
    pub scroll_x: f32,
    pub scroll_y: f32,
    pub max_scroll_y: f32,
}
impl View {
    /// Query a unique accessible name, including names bound in repeat rows.
    /// Returns the last committed layout without flushing or calling the VM.
    pub fn element_metrics(&self, name: &str) -> VmResult<ElementMetrics> {
        let mut value: sys::OneUiViewElementMetrics = unsafe { std::mem::zeroed() };
        value.struct_size = std::mem::size_of_val(&value) as u32;
        check(unsafe { sys::oneui_view_element_metrics(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), &mut value) })?;
        Ok(ElementMetrics {
            frame: Rect { x: value.x, y: value.y, width: value.width, height: value.height },
            scrollable: value.scrollable != 0, scroll_x: value.scroll_x,
            scroll_y: value.scroll_y, max_scroll_y: value.max_scroll_y,
        })
    }
}

impl super::ViewMount {
    /// Restore native keyboard focus to a uniquely named element in this mount.
    /// Call outside VM callbacks, after its view has been attached.
    pub fn focus_element(&self, name: &str, focus_visible: bool) -> VmResult<bool> {
        let mut focused = 0;
        check(unsafe { sys::oneui_view_mount_focus_element(self.raw.as_ptr(),
            sys::OneUiUtf8String::from_str(name), i32::from(focus_visible), &mut focused) })?;
        Ok(focused != 0)
    }
}
