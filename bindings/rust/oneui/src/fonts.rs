use crate::{sys, Error};
use std::ffi::c_void;
use std::panic::{catch_unwind, AssertUnwindSafe};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FontFamilyInfo {
    /// UTF-8 family name accepted by OneUI's font setters.
    pub family: String,
    /// Fixed pitch as reported by the normal face; not a glyph-coverage guarantee.
    pub monospaced: bool,
}

/// Fresh, sorted, unique installed-font snapshot on Windows, macOS and Linux.
/// Excludes process-local registered fonts. Requires no window or UI thread.
/// OS discovery may block: run on a worker, cache in the application, and call
/// again when a user requests refresh after installing fonts.
pub fn enumerate_system_fonts() -> Result<Vec<FontFamilyInfo>, Error> {
    struct Collection {
        fonts: Vec<FontFamilyInfo>,
        failed: bool,
    }
    unsafe extern "C" fn collect(name: sys::OneUiUtf8String, mono: i32, data: *mut c_void) {
        let state = unsafe { &mut *data.cast::<Collection>() };
        if state.failed {
            return;
        }
        let result = catch_unwind(AssertUnwindSafe(|| {
            if name.data.is_null() || name.length == 0 {
                return None;
            }
            let bytes = unsafe { std::slice::from_raw_parts(name.data.cast::<u8>(), name.length) };
            let family = std::str::from_utf8(bytes).ok()?.to_owned();
            state.fonts.push(FontFamilyInfo {
                family,
                monospaced: mono != 0,
            });
            Some(())
        }));
        state.failed = !matches!(result, Ok(Some(())));
    }
    let mut state = Collection {
        fonts: Vec::new(),
        failed: false,
    };
    let count = unsafe {
        sys::oneui_enumerate_system_fonts(Some(collect), (&mut state as *mut Collection).cast())
    };
    if count < 0 || state.failed || count as usize != state.fonts.len() {
        Err(Error::FontEnumerationFailed)
    } else {
        Ok(state.fonts)
    }
}

/// Widget-free unwrapped text shaping in logical pixels, using Label's shaper.
/// An empty family uses OneUI's default font fallback. Invalid options fail.
pub fn measure_text_width(text: &str, family: &str, size: f32, weight: i32, scale: f32) -> Result<f32, Error> {
    let mut width = 0.0;
    let ok = unsafe { sys::oneui_text_measure_width_utf8(
        sys::OneUiUtf8String::from_str(text), sys::OneUiUtf8String::from_str(family),
        size, weight, scale, &mut width) };
    if ok == 1 { Ok(width) } else { Err(Error::InvalidTextOptions) }
}
