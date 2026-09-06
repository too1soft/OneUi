use crate::{sys, Window};

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
