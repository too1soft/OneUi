use super::{check, View, ViewError, VmResult};
use crate::{sys, Panel, Widget};

impl View {
    /// Retains an explicitly declared MountPoint. Ordinary controls cannot be
    /// retrieved through this capability. The View still owns its layout.
    pub fn mount_point(&self, name: &str) -> VmResult<Panel> {
        let mut raw = std::ptr::null_mut();
        check(unsafe {
            sys::oneui_view_mount_point(self.raw.as_ptr(),
                sys::OneUiUtf8String::from_str(name), &mut raw)
        })?;
        let widget = Widget::from_raw(raw).map_err(|_| ViewError::FOREIGN_FAILURE)?;
        Ok(Panel { widget })
    }
}
