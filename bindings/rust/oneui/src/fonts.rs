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
    enumerate_fonts(false)
}

/// Available font-picker families, including process-private application fonts.
/// Embedded faces take precedence over installed faces with the same name.
pub fn enumerate_available_fonts() -> Result<Vec<FontFamilyInfo>, Error> {
    enumerate_fonts(true)
}

fn enumerate_fonts(available: bool) -> Result<Vec<FontFamilyInfo>, Error> {
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
        let data = (&mut state as *mut Collection).cast();
        if available {
            sys::oneui_enumerate_available_fonts(Some(collect), data)
        } else {
            sys::oneui_enumerate_system_fonts(Some(collect), data)
        }
    };
    if count < 0 || state.failed || count as usize != state.fonts.len() {
        Err(Error::FontEnumerationFailed)
    } else {
        Ok(state.fonts)
    }
}

/// Widget-free unwrapped text shaping in logical pixels, using Label's shaper.
/// An empty family uses OneUI's default font fallback. Invalid options fail.
pub fn measure_text_width(
    text: &str,
    family: &str,
    size: f32,
    weight: i32,
    scale: f32,
) -> Result<f32, Error> {
    let mut width = 0.0;
    let ok = unsafe {
        sys::oneui_text_measure_width_utf8(
            sys::OneUiUtf8String::from_str(text),
            sys::OneUiUtf8String::from_str(family),
            size,
            weight,
            scale,
            &mut width,
        )
    };
    if ok == 1 {
        Ok(width)
    } else {
        Err(Error::InvalidTextOptions)
    }
}

/// Warms layout glyph metrics for characters supported by a registered font.
/// Glyphs are looked up individually. This does not shape the text or select
/// platform fallbacks. This hint can run on a background thread and does not
/// require a window.
pub fn warm_glyph_metrics(text: &str, family: &str, size: f32, weight: i32) -> bool {
    unsafe {
        sys::oneui_font_warm_glyph_metrics_utf8(
            sys::OneUiUtf8String::from_str(text),
            sys::OneUiUtf8String::from_str(family),
            size,
            weight,
        ) == 1
    }
}

/// Registers an application font directly from memory.
///
/// OneUI copies the bytes during this call and never installs the font into the
/// operating system. Register fonts before constructing the first window so
/// every text layout cache sees the same set of families.
pub fn register_font_from_memory(data: &[u8], family_alias: &str) -> Result<(), Error> {
    register_font(data, family_alias, sys::oneui_font_register_memory)
}

/// Registers immutable executable font bytes without copying them.
///
/// The static lifetime is required because native typefaces retain the bytes.
/// ```compile_fail
/// let bytes = vec![0_u8; 32];
/// oneui::register_font_from_static_memory(&bytes, "Temporary").unwrap();
/// ```
pub fn register_font_from_static_memory(
    data: &'static [u8],
    family_alias: &str,
) -> Result<(), Error> {
    register_font(data, family_alias, sys::oneui_font_register_static_memory)
}

fn register_font(
    data: &[u8],
    family_alias: &str,
    register: unsafe extern "C" fn(*const u8, usize, sys::OneUiUtf8String) -> std::ffi::c_int,
) -> Result<(), Error> {
    let family_alias = family_alias.trim();
    if data.is_empty() || family_alias.is_empty() {
        return Err(Error::FontRegistrationFailed);
    }
    let family_alias = sys::OneUiUtf8String::from_str(family_alias);
    let registered = unsafe { register(data.as_ptr(), data.len(), family_alias) };
    if registered == 1 {
        Ok(())
    } else {
        Err(Error::FontRegistrationFailed)
    }
}

/// Family alias for OneUI's optional bundled Simplified Chinese UI font.
#[cfg(feature = "bundled-fonts")]
pub const BUNDLED_UI_FONT_FAMILY: &str = "Noto Sans SC";

/// SIL Open Font License text shipped with the bundled Noto Sans SC binary.
#[cfg(feature = "bundled-fonts")]
pub const BUNDLED_UI_FONT_LICENSE: &str =
    include_str!(concat!(env!("CARGO_MANIFEST_DIR"), "/assets/fonts/OFL.txt"));

/// Registers the unmodified Noto Sans SC variable font embedded in this crate.
#[cfg(feature = "bundled-fonts")]
pub fn register_bundled_fonts() -> Result<(), Error> {
    static FONT: &[u8] = include_bytes!(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/assets/fonts/NotoSansSC-wght.ttf"
    ));
    register_font_from_memory(FONT, BUNDLED_UI_FONT_FAMILY)
}
