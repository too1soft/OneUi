use crate::{sys, Window};

/// Double-click arbitration time from Win32, AppKit, or GTK desktop settings.
/// Linux uses 500ms only when no GTK display is available.
pub fn double_click_interval() -> std::time::Duration {
    std::time::Duration::from_millis(unsafe { sys::oneui_double_click_interval_ms() } as u64)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WindowBackend {
    Unknown,
    Win32,
    Cocoa,
    X11,
    Wayland,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct WindowCapabilities(u32);

impl WindowCapabilities {
    pub fn bits(self) -> u32 {
        self.0
    }
    pub fn clipboard(self) -> bool {
        self.0 & sys::WINDOW_CAPABILITY_CLIPBOARD != 0
    }
    pub fn ime(self) -> bool {
        self.0 & sys::WINDOW_CAPABILITY_IME != 0
    }
    pub fn placement(self) -> bool {
        self.0 & sys::WINDOW_CAPABILITY_PLACEMENT != 0
    }
    pub fn activation(self) -> bool {
        self.0 & sys::WINDOW_CAPABILITY_ACTIVATION != 0
    }
    pub fn topmost(self) -> bool {
        self.0 & sys::WINDOW_CAPABILITY_TOPMOST != 0
    }
    pub fn tray(self) -> bool {
        self.0 & sys::WINDOW_CAPABILITY_TRAY != 0
    }
    pub fn native_dialogs(self) -> bool {
        self.0 & sys::WINDOW_CAPABILITY_NATIVE_DIALOGS != 0
    }
    pub fn menu_bar(self) -> bool {
        self.0 & sys::WINDOW_CAPABILITY_MENU_BAR != 0
    }
    pub fn tray_tooltip(self) -> bool {
        self.0 & sys::WINDOW_CAPABILITY_TRAY_TOOLTIP != 0
    }
    pub fn file_drop(self) -> bool {
        self.0 & sys::WINDOW_CAPABILITY_FILE_DROP != 0
    }
}

impl Window {
    pub fn backend(&self) -> WindowBackend {
        match self
            .state
            .with_raw(|raw| unsafe { sys::oneui_window_backend(raw) })
            .unwrap_or(0)
        {
            1 => WindowBackend::Win32,
            2 => WindowBackend::Cocoa,
            3 => WindowBackend::X11,
            4 => WindowBackend::Wayland,
            _ => WindowBackend::Unknown,
        }
    }

    /// Runtime snapshot: Wayland clipboard access can change after user input.
    pub fn capabilities(&self) -> WindowCapabilities {
        WindowCapabilities(
            self.state
                .with_raw(|raw| unsafe { sys::oneui_window_capabilities(raw) })
                .unwrap_or(0),
        )
    }
}
