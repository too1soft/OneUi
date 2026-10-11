use super::{check, View, ViewMount, VmResult};
use crate::{sys, ItemDragPhase, Tabs};

impl ViewMount {
    /// Register native shell tabs as a WP4 drop target in this mount's window.
    /// The domain borrows the native widget weakly and retains no Rust callbacks.
    pub fn register_workspace_tabs(&self, domain: &str, id: &str, tabs: &Tabs) -> VmResult {
        check(unsafe {
            tabs.as_widget().with_native_handle(|widget| {
                sys::oneui_view_mount_register_workspace_tabs(
                    self.raw.as_ptr(),
                    sys::OneUiUtf8String::from_str(domain),
                    sys::OneUiUtf8String::from_str(id),
                    widget.cast(),
                )
            })
        })
    }
}
impl View {
    /// Forward a native tab drag into the declarative workspace coordinator.
    /// All preview, final hit testing and stale revision handling remains native.
    pub fn workspace_tab_drag(
        &self,
        domain: &str,
        source: &str,
        phase: ItemDragPhase,
        x: f32,
        y: f32,
    ) -> VmResult {
        let phase = match phase {
            ItemDragPhase::Started => 0,
            ItemDragPhase::Updated => 1,
            ItemDragPhase::Dropped => 2,
            ItemDragPhase::Cancelled => 3,
        };
        check(unsafe {
            sys::oneui_view_workspace_tab_drag(
                self.raw.as_ptr(),
                sys::OneUiUtf8String::from_str(domain),
                sys::OneUiUtf8String::from_str(source),
                phase,
                x,
                y,
            )
        })
    }
}
