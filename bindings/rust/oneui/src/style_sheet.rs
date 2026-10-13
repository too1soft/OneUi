//! Style sheet bindings.

use super::{sys, CStr, CString, NonNull, Path};

/// Errors returned while building a native OneUI style sheet from CSS.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum StyleSheetError {
    CreationFailed,
    InteriorNul,
    ParseFailed(String),
    ReadFailed(String),
}

/// A reusable CSS theme for OneUI windows and widgets.
///
/// The style sheet is owned by Rust while OneUI retains a shared native copy
/// after it is installed on a window. Applications should assign semantic
/// classes to widgets instead of reproducing visual tokens in every page.
pub struct StyleSheet {
    raw: NonNull<sys::OneUiStyleSheet>,
}

impl StyleSheet {
    pub fn new() -> Result<Self, StyleSheetError> {
        let raw = NonNull::new(unsafe { sys::oneui_style_sheet_create() })
            .ok_or(StyleSheetError::CreationFailed)?;
        Ok(Self { raw })
    }

    pub fn from_css(css: &str) -> Result<Self, StyleSheetError> {
        let mut style_sheet = Self::new()?;
        style_sheet.add_css(css)?;
        Ok(style_sheet)
    }

    pub fn add_css(&mut self, css: &str) -> Result<(), StyleSheetError> {
        let css = CString::new(css).map_err(|_| StyleSheetError::InteriorNul)?;
        let mut error = [0i8; 1024];
        let applied = unsafe {
            sys::oneui_style_sheet_add_css(
                self.raw.as_ptr(),
                css.as_ptr(),
                error.as_mut_ptr(),
                error.len() as i32,
            )
        };
        if applied != 0 {
            return Ok(());
        }

        let message = unsafe { CStr::from_ptr(error.as_ptr()) }
            .to_string_lossy()
            .into_owned();
        Err(StyleSheetError::ParseFailed(message))
    }

    pub fn load_file(&mut self, path: impl AsRef<Path>) -> Result<(), StyleSheetError> {
        let css = std::fs::read_to_string(path.as_ref())
            .map_err(|error| StyleSheetError::ReadFailed(error.to_string()))?;
        self.add_css(&css)
    }

    /// Atomically replaces CSS while keeping mounted widget bindings valid.
    /// Refresh the owning window after applying any custom properties.
    pub fn replace_css(&mut self, css: &str) -> Result<(), StyleSheetError> {
        let css = CString::new(css).map_err(|_| StyleSheetError::InteriorNul)?;
        let mut error = [0i8; 1024];
        let applied = unsafe {
            sys::oneui_style_sheet_replace_css(
                self.raw.as_ptr(),
                css.as_ptr(),
                error.as_mut_ptr(),
                error.len() as i32,
            )
        };
        if applied != 0 {
            return Ok(());
        }
        let message = unsafe { CStr::from_ptr(error.as_ptr()) }
            .to_string_lossy()
            .into_owned();
        Err(StyleSheetError::ParseFailed(message))
    }

    /// Assigns the palette used by dialogs, popups, menus and select options.
    /// Keep the variant alive, then refresh the window after changing either sheet.
    /// Passing None restores the regular palette for transient surfaces.
    pub fn set_transient_style_sheet(&mut self, transient: Option<&StyleSheet>) {
        unsafe {
            sys::oneui_style_sheet_set_transient_style_sheet(
                self.raw.as_ptr(),
                transient.map_or(std::ptr::null_mut(), |sheet| sheet.as_raw()),
            );
        }
    }

    pub fn set_custom_property(&mut self, name: &str, value: &str) -> Result<(), StyleSheetError> {
        let name = CString::new(name).map_err(|_| StyleSheetError::InteriorNul)?;
        let value = CString::new(value).map_err(|_| StyleSheetError::InteriorNul)?;
        unsafe {
            sys::oneui_style_sheet_set_custom_property(
                self.raw.as_ptr(),
                name.as_ptr(),
                value.as_ptr(),
            )
        };
        Ok(())
    }

    pub(super) fn as_raw(&self) -> *mut sys::OneUiStyleSheet {
        self.raw.as_ptr()
    }
}

impl Drop for StyleSheet {
    fn drop(&mut self) {
        unsafe { sys::oneui_style_sheet_destroy(self.raw.as_ptr()) };
    }
}
