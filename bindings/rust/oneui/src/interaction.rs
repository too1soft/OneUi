//! Scoped commands and Unicode paragraph options. All registrations are UI-thread-only.
use super::{run_callback_guarded, sys, Error, Label, TextArea, TextField, Widget, Window};
use std::{cell::RefCell, ffi::c_void, marker::PhantomData, ops::BitOr, ptr::NonNull, rc::Rc};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CommandResult {
    NotFound,
    Disabled,
    Executed,
    Enabled,
}
impl CommandResult {
    fn from_raw(value: u32) -> Self {
        match value {
            1 => Self::Disabled,
            2 => Self::Executed,
            3 => Self::Enabled,
            _ => Self::NotFound,
        }
    }
}
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct KeyModifiers(u32);
impl KeyModifiers {
    pub const NONE: Self = Self(0);
    pub const SHIFT: Self = Self(1);
    pub const CONTROL: Self = Self(2);
    pub const ALT: Self = Self(4);
    pub const META: Self = Self(8);
    /// Command on macOS, Control on Windows/Linux; does not change raw key events.
    pub const PRIMARY: Self = Self(16);
    pub fn bits(self) -> u32 {
        self.0
    }
}
impl BitOr for KeyModifiers {
    type Output = Self;
    fn bitor(self, rhs: Self) -> Self {
        Self(self.0 | rhs.0)
    }
}
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct KeyChord {
    pub key: String,
    pub modifiers: KeyModifiers,
}
impl KeyChord {
    pub fn new(key: impl Into<String>, modifiers: KeyModifiers) -> Self {
        Self {
            key: key.into(),
            modifiers,
        }
    }
    fn raw(&self) -> sys::OneUiKeyChordUtf8 {
        sys::OneUiKeyChordUtf8 {
            key: sys::OneUiUtf8String::from_str(&self.key),
            modifiers: self.modifiers.bits(),
        }
    }
}

/// Dropping this token immediately unregisters the command. Callback captures are
/// released after any in-flight callback finishes, even if its scope is destroyed.
pub struct CommandRegistration {
    raw: NonNull<sys::OneUiCommandRegistration>,
    _ui_thread: PhantomData<Rc<()>>,
}
impl Drop for CommandRegistration {
    fn drop(&mut self) {
        unsafe { sys::oneui_command_registration_destroy(self.raw.as_ptr()) }
    }
}
struct Callback {
    execute: RefCell<Box<dyn FnMut()>>,
    enabled: RefCell<Box<dyn FnMut() -> bool>>,
}
unsafe extern "C" fn execute(data: *mut c_void) {
    let _ = run_callback_guarded("command execute", || {
        let callback = unsafe { &*data.cast::<Callback>() };
        if let Ok(mut handler) = callback.execute.try_borrow_mut() {
            handler();
        }
    });
}
unsafe extern "C" fn enabled(data: *mut c_void) -> i32 {
    run_callback_guarded("command enabled", || {
        let callback = unsafe { &*data.cast::<Callback>() };
        // Recursive dispatch of a mutably borrowed handler fails closed.
        let Ok(_execution) = callback.execute.try_borrow_mut() else {
            return false;
        };
        let Ok(mut predicate) = callback.enabled.try_borrow_mut() else {
            return false;
        };
        predicate()
    })
    .unwrap_or(false)
    .into()
}
unsafe extern "C" fn destroy(data: *mut c_void) {
    let _ = run_callback_guarded("command destroy", || {
        drop(unsafe { Box::from_raw(data.cast::<Callback>()) });
    });
}
fn callback(
    execute: impl FnMut() + 'static,
    enabled: impl FnMut() -> bool + 'static,
) -> *mut c_void {
    Box::into_raw(Box::new(Callback {
        execute: RefCell::new(Box::new(execute)),
        enabled: RefCell::new(Box::new(enabled)),
    }))
    .cast()
}
fn registration(raw: *mut sys::OneUiCommandRegistration) -> Result<CommandRegistration, Error> {
    NonNull::new(raw)
        .map(|raw| CommandRegistration {
            raw,
            _ui_thread: PhantomData,
        })
        .ok_or(Error::CommandRegistrationFailed)
}
impl Widget {
    pub fn register_command(
        &self,
        id: &str,
        shortcut: Option<KeyChord>,
        handler: impl FnMut() + 'static,
    ) -> Result<CommandRegistration, Error> {
        self.register_command_when(id, shortcut, handler, || true)
    }
    /// A duplicate ID or shortcut in this scope returns an error. The C boundary
    /// takes ownership of callbacks even when registration fails.
    pub fn register_command_when(
        &self,
        id: &str,
        shortcut: Option<KeyChord>,
        handler: impl FnMut() + 'static,
        predicate: impl FnMut() -> bool + 'static,
    ) -> Result<CommandRegistration, Error> {
        let key = shortcut.as_ref().map(KeyChord::raw);
        registration(unsafe {
            sys::oneui_widget_register_command_utf8(
                self.as_raw(),
                sys::OneUiUtf8String::from_str(id),
                key.as_ref().map_or(std::ptr::null(), |key| key),
                Some(execute),
                Some(enabled),
                callback(handler, predicate),
                Some(destroy),
            )
        })
    }
    pub fn query_command(&self, id: &str) -> CommandResult {
        CommandResult::from_raw(unsafe {
            sys::oneui_widget_query_command_utf8(self.as_raw(), sys::OneUiUtf8String::from_str(id))
        })
    }
    pub fn execute_command(&self, id: &str) -> CommandResult {
        CommandResult::from_raw(unsafe {
            sys::oneui_widget_execute_command_utf8(
                self.as_raw(),
                sys::OneUiUtf8String::from_str(id),
            )
        })
    }
}
impl Window {
    pub fn register_command(
        &self,
        id: &str,
        shortcut: Option<KeyChord>,
        handler: impl FnMut() + 'static,
    ) -> Result<CommandRegistration, Error> {
        self.register_command_when(id, shortcut, handler, || true)
    }
    pub fn register_command_when(
        &self,
        id: &str,
        shortcut: Option<KeyChord>,
        handler: impl FnMut() + 'static,
        predicate: impl FnMut() -> bool + 'static,
    ) -> Result<CommandRegistration, Error> {
        let key = shortcut.as_ref().map(KeyChord::raw);
        self.state
            .with_raw(|raw| {
                registration(unsafe {
                    sys::oneui_window_register_command_utf8(
                        raw,
                        sys::OneUiUtf8String::from_str(id),
                        key.as_ref().map_or(std::ptr::null(), |key| key),
                        Some(execute),
                        Some(enabled),
                        callback(handler, predicate),
                        Some(destroy),
                    )
                })
            })
            .ok_or(Error::WindowClosed)?
    }
    pub fn query_command(&self, id: &str) -> Result<CommandResult, Error> {
        self.state
            .with_raw(|raw| {
                CommandResult::from_raw(unsafe {
                    sys::oneui_window_query_command_utf8(raw, sys::OneUiUtf8String::from_str(id))
                })
            })
            .ok_or(Error::WindowClosed)
    }
    pub fn execute_command(&self, id: &str) -> Result<CommandResult, Error> {
        self.state
            .with_raw(|raw| {
                CommandResult::from_raw(unsafe {
                    sys::oneui_window_execute_command_utf8(raw, sys::OneUiUtf8String::from_str(id))
                })
            })
            .ok_or(Error::WindowClosed)
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
#[repr(u32)]
pub enum TextDirection {
    #[default]
    Auto = 0,
    Ltr = 1,
    Rtl = 2,
}
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
#[repr(u32)]
pub enum TextWrapMode {
    #[default]
    NoWrap = 0,
    WordWrap = 1,
}
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
#[repr(u32)]
pub enum TextAffinity {
    Upstream = 0,
    #[default]
    Downstream = 1,
}
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TextOptions {
    pub direction: TextDirection,
    pub wrap: TextWrapMode,
    pub locale: String,
}
impl Default for TextOptions {
    fn default() -> Self {
        Self {
            direction: TextDirection::Auto,
            wrap: TextWrapMode::NoWrap,
            locale: "und".into(),
        }
    }
}
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct TextPosition {
    pub utf8_offset: usize,
    pub affinity: TextAffinity,
}

fn set_options(widget: &Widget, options: &TextOptions) -> Result<(), Error> {
    let raw = sys::OneUiTextOptionsUtf8 {
        direction: options.direction as u32,
        wrap: options.wrap as u32,
        locale: sys::OneUiUtf8String::from_str(&options.locale),
    };
    if unsafe { sys::oneui_widget_set_text_options_utf8(widget.as_raw(), &raw) } != 0 {
        Ok(())
    } else {
        Err(Error::InvalidTextOptions)
    }
}
macro_rules! text_options {
    ($($control:ty),*) => { $(impl $control {
        pub fn set_text_options(&self, options: &TextOptions) -> Result<(), Error> { set_options(&self.widget, options) }
    })* };
}
text_options!(Label, TextField, TextArea);
macro_rules! text_position {
    ($($control:ty),*) => { $(impl $control {
        /// UTF-8 byte offset, independent of the platform wchar_t width.
        pub fn text_position(&self) -> Result<TextPosition, Error> {
            let mut raw = sys::OneUiTextPositionUtf8::default();
            if unsafe { sys::oneui_text_field_get_position_utf8(self.widget.as_raw(), &mut raw) } == 0 { return Err(Error::InvalidTextPosition); }
            Ok(TextPosition { utf8_offset: raw.utf8_offset, affinity: if raw.affinity == 0 { TextAffinity::Upstream } else { TextAffinity::Downstream } })
        }
        /// Rejects offsets inside a UTF-8 sequence or a grapheme cluster.
        pub fn set_text_position(&self, position: TextPosition) -> Result<(), Error> {
            let raw = sys::OneUiTextPositionUtf8 { utf8_offset: position.utf8_offset, affinity: position.affinity as u32 };
            if unsafe { sys::oneui_text_field_set_position_utf8(self.widget.as_raw(), raw) } != 0 { Ok(()) } else { Err(Error::InvalidTextPosition) }
        }
    })* };
}
text_position!(TextField, TextArea);
