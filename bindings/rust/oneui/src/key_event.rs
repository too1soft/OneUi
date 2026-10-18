//! Key event bindings.

use super::sys;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RawKeyEvent {
    pub virtual_key: u32,
    pub scan_code: u32,
    pub pressed: bool,
    pub repeat: bool,
    pub extended: bool,
    pub alt: bool,
    pub ctrl: bool,
    pub shift: bool,
    pub win: bool,
}

impl RawKeyEvent {
    /// Meta on every backend (Command on macOS); `win` remains for ABI compatibility.
    pub fn meta(&self) -> bool {
        self.win
    }
}

impl From<sys::OneUiRawKeyEvent> for RawKeyEvent {
    fn from(value: sys::OneUiRawKeyEvent) -> Self {
        Self {
            virtual_key: value.virtual_key,
            scan_code: value.scan_code,
            pressed: value.pressed != 0,
            repeat: value.repeat != 0,
            extended: value.extended != 0,
            alt: value.alt != 0,
            ctrl: value.ctrl != 0,
            shift: value.shift != 0,
            win: value.win != 0,
        }
    }
}
