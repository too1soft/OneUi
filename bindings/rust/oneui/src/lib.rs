//! Safe foundation bindings for OneUI.
//!
//! This crate intentionally starts with window lifecycle and UTF-8 title
//! handling. Higher-level controls and async UI dispatch will be added without
//! changing the raw ABI exposed by `oneui-sys`.

use std::ptr::NonNull;

pub use oneui_sys as sys;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Error {
    AbiVersionMismatch { expected: u32, actual: u32 },
    WindowCreationFailed,
}

#[derive(Debug, Clone)]
pub struct WindowOptions {
    pub title: String,
    pub width: i32,
    pub height: i32,
    pub visible: bool,
    pub borderless: bool,
    pub fullscreen: bool,
    pub topmost: bool,
    pub resizable: bool,
}

impl Default for WindowOptions {
    fn default() -> Self {
        Self {
            title: "OneUI".to_owned(),
            width: 1280,
            height: 800,
            visible: false,
            borderless: false,
            fullscreen: false,
            topmost: false,
            resizable: true,
        }
    }
}

pub struct Window {
    raw: NonNull<sys::OneUiWindow>,
}

impl Window {
    pub fn new(options: &WindowOptions) -> Result<Self, Error> {
        let actual = unsafe { sys::oneui_utf8_abi_version() };
        if actual != sys::UTF8_ABI_VERSION {
            return Err(Error::AbiVersionMismatch {
                expected: sys::UTF8_ABI_VERSION,
                actual,
            });
        }

        let native_options = sys::OneUiWindowOptionsUtf8 {
            title: sys::OneUiUtf8String::from_str(&options.title),
            width: options.width,
            height: options.height,
            visible: i32::from(options.visible),
            borderless: i32::from(options.borderless),
            fullscreen: i32::from(options.fullscreen),
            topmost: i32::from(options.topmost),
            resizable: i32::from(options.resizable),
        };
        let raw = unsafe { sys::oneui_window_create_utf8(&native_options) };
        let raw = NonNull::new(raw).ok_or(Error::WindowCreationFailed)?;
        Ok(Self { raw })
    }

    pub fn set_title(&self, title: &str) {
        let title = sys::OneUiUtf8String::from_str(title);
        unsafe { sys::oneui_window_set_title_utf8(self.raw.as_ptr(), title) };
    }

    pub fn show(&self) {
        unsafe { sys::oneui_window_show(self.raw.as_ptr()) };
    }

    pub fn run(&self) -> i32 {
        unsafe { sys::oneui_window_run(self.raw.as_ptr()) }
    }

    pub fn close(&self) {
        unsafe { sys::oneui_window_close(self.raw.as_ptr()) };
    }
}

impl Drop for Window {
    fn drop(&mut self) {
        unsafe { sys::oneui_window_destroy(self.raw.as_ptr()) };
    }
}

#[cfg(test)]
mod tests {
    use super::{Window, WindowOptions};

    #[test]
    fn creates_hidden_window_through_utf8_abi() {
        let window = Window::new(&WindowOptions {
            title: "iShellPro 麒麟 🚀".to_owned(),
            ..WindowOptions::default()
        })
        .expect("OneUI window should be created through the UTF-8 ABI");
        window.set_title("兴业银行股份有限公司");
    }
}
