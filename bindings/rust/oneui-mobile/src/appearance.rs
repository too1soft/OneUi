//! Mobile appearance policy. Platform adapters observe configuration and apply
//! requests. An accepted request is not an OS/compositor acknowledgement.
use crate::{checked, Error, Result};
use std::{ffi::c_void, marker::PhantomData, ptr::NonNull, rc::Rc};

#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum ThemeMode {
    #[default]
    System = 0,
    Light = 1,
    Dark = 2,
}
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SystemTheme {
    Unknown = 0,
    Light = 1,
    Dark = 2,
}
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum BarIcons {
    #[default]
    Auto = 0,
    Light = 1,
    Dark = 2,
}

#[derive(Debug, Clone, Copy, Default)]
pub struct AppearanceOptions {
    pub theme: ThemeMode,
    /// Opaque ARGB colors for status and navigation protection backgrounds.
    pub bar_colors: Option<(u32, u32)>,
    pub status_icons: BarIcons,
    pub navigation_icons: BarIcons,
}
#[derive(Debug, Clone, Copy)]
pub struct AppearanceState {
    pub options: AppearanceOptions,
    pub system_theme: SystemTheme,
    pub effective_theme: SystemTheme,
    pub host_available: bool,
    pub status_argb: u32,
    pub navigation_argb: u32,
    pub status_icons: BarIcons,
    pub navigation_icons: BarIcons,
    pub revision: u64,
    pub applied_revision: u64,
}
#[repr(C)]
#[derive(Default)]
struct Options {
    theme: i32,
    custom: u32,
    status: u32,
    navigation: u32,
    status_icons: i32,
    navigation_icons: i32,
}
#[repr(C)]
#[derive(Default)]
struct State {
    options: Options,
    system: i32,
    effective: i32,
    available: u32,
    status: u32,
    navigation: u32,
    status_icons: i32,
    navigation_icons: i32,
    revision: u64,
    applied_revision: u64,
}
extern "C" {
    fn oneui_mobile_appearance_create(out: *mut *mut c_void) -> i32;
    fn oneui_mobile_appearance_destroy(handle: *mut c_void) -> i32;
    fn oneui_mobile_appearance_set_options_v1(handle: *mut c_void, options: *const Options) -> i32;
    fn oneui_mobile_appearance_get_state_v1(handle: *mut c_void, state: *mut State) -> i32;
    fn oneui_mobile_appearance_observe_v1(handle: *mut c_void, system: i32, available: u32) -> i32;
    fn oneui_mobile_appearance_acknowledge_v1(handle: *mut c_void, revision: u64) -> i32;
}
fn theme(value: i32) -> Result<SystemTheme> {
    match value {
        0 => Ok(SystemTheme::Unknown),
        1 => Ok(SystemTheme::Light),
        2 => Ok(SystemTheme::Dark),
        _ => Err(Error::AbiMismatch),
    }
}
fn icons(value: i32) -> Result<BarIcons> {
    match value {
        0 => Ok(BarIcons::Auto),
        1 => Ok(BarIcons::Light),
        2 => Ok(BarIcons::Dark),
        _ => Err(Error::AbiMismatch),
    }
}
/// UI-thread confined, no implicit Android/iOS/HarmonyOS host.
/// ```compile_fail
/// let appearance = oneui_mobile::appearance::Appearance::new().unwrap();
/// std::thread::spawn(move || drop(appearance));
/// ```
pub struct Appearance {
    handle: NonNull<c_void>,
    _thread: PhantomData<Rc<()>>,
}
impl Appearance {
    pub fn new() -> Result<Self> {
        let mut raw = std::ptr::null_mut();
        // SAFETY: writable pointer; native ownership transferred on success.
        checked(unsafe { oneui_mobile_appearance_create(&mut raw) })?;
        Ok(Self {
            handle: NonNull::new(raw).ok_or(Error::Failed)?,
            _thread: PhantomData,
        })
    }
    pub fn set_options(&mut self, options: AppearanceOptions) -> Result<()> {
        let (status, navigation) = options.bar_colors.unwrap_or((0, 0));
        let raw = Options {
            theme: options.theme as i32,
            custom: options.bar_colors.is_some() as u32,
            status,
            navigation,
            status_icons: options.status_icons as i32,
            navigation_icons: options.navigation_icons as i32,
        };
        // SAFETY: live thread-confined handle and repr(C) input, not retained.
        checked(unsafe { oneui_mobile_appearance_set_options_v1(self.handle.as_ptr(), &raw) })
    }
    pub fn state(&self) -> Result<AppearanceState> {
        let mut raw = State::default();
        // SAFETY: writable repr(C) output and live thread-confined handle.
        checked(unsafe { oneui_mobile_appearance_get_state_v1(self.handle.as_ptr(), &mut raw) })?;
        let mode = match raw.options.theme {
            0 => ThemeMode::System,
            1 => ThemeMode::Light,
            2 => ThemeMode::Dark,
            _ => return Err(Error::AbiMismatch),
        };
        Ok(AppearanceState {
            options: AppearanceOptions {
                theme: mode,
                bar_colors: if raw.options.custom != 0 {
                    Some((raw.options.status, raw.options.navigation))
                } else {
                    None
                },
                status_icons: icons(raw.options.status_icons)?,
                navigation_icons: icons(raw.options.navigation_icons)?,
            },
            system_theme: theme(raw.system)?,
            effective_theme: theme(raw.effective)?,
            host_available: raw.available != 0,
            status_argb: raw.status,
            navigation_argb: raw.navigation,
            status_icons: icons(raw.status_icons)?,
            navigation_icons: icons(raw.navigation_icons)?,
            revision: raw.revision,
            applied_revision: raw.applied_revision,
        })
    }
    /// Native host integration, on initial attachment/configuration/resume.
    pub fn observe(&mut self, system: SystemTheme, available: bool) -> Result<()> {
        // SAFETY: valid enums and live thread-confined handle.
        checked(unsafe {
            oneui_mobile_appearance_observe_v1(
                self.handle.as_ptr(),
                system as i32,
                available as u32,
            )
        })
    }
    /// Native adapter only, after applying this exact revision successfully.
    pub fn acknowledge(&mut self, revision: u64) -> Result<()> {
        // SAFETY: native implementation checks availability and revision.
        checked(unsafe { oneui_mobile_appearance_acknowledge_v1(self.handle.as_ptr(), revision) })
    }
}
impl Drop for Appearance {
    fn drop(&mut self) {
        // SAFETY: unique ownership and !Send/!Sync preserve the creating thread.
        let result = unsafe { oneui_mobile_appearance_destroy(self.handle.as_ptr()) };
        debug_assert_eq!(result, 0);
    }
}
