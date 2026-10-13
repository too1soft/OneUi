//! Window operations bindings.

use super::{
    sys, wide_null_terminated, wide_to_string, Arc, CString, Error, Path, PathBuf, SecretWide,
    UiDispatcher, Widget, WidgetHandle,
};

/// Presentation options for [`UiDispatcher::prompt_blocking`].
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct PromptOptions<'a> {
    pub initial_value: &'a str,
    pub placeholder: &'a str,
    pub password: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FileDialogMode {
    OpenFile,
    SaveFile,
    SelectFolder,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct FileDialogFilter<'a> {
    pub name: &'a str,
    pub pattern: &'a str,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct FileDialogOptions<'a> {
    pub mode: FileDialogMode,
    pub title: &'a str,
    pub initial_directory: &'a Path,
    pub default_name: &'a str,
    pub default_extension: &'a str,
    pub filters: &'a [FileDialogFilter<'a>],
    pub confirm_overwrite: bool,
}

impl<'a> FileDialogOptions<'a> {
    pub fn open(title: &'a str) -> Self {
        Self {
            mode: FileDialogMode::OpenFile,
            title,
            initial_directory: Path::new(""),
            default_name: "",
            default_extension: "",
            filters: &[],
            confirm_overwrite: true,
        }
    }

    pub fn save(title: &'a str) -> Self {
        Self {
            mode: FileDialogMode::SaveFile,
            ..Self::open(title)
        }
    }

    pub fn select_folder(title: &'a str) -> Self {
        Self {
            mode: FileDialogMode::SelectFolder,
            ..Self::open(title)
        }
    }
}

pub(super) fn parse_multiple_file_dialog_output(
    output: &[u8],
    required: usize,
) -> Result<Vec<PathBuf>, Error> {
    if required < 2 || required > output.len() || output[required - 2..required] != [0, 0] {
        return Err(Error::FileDialogFailed);
    }
    let mut paths = Vec::new();
    for bytes in output[..required - 1].split(|byte| *byte == 0) {
        if bytes.is_empty() {
            break;
        }
        let path = std::str::from_utf8(bytes).map_err(|_| Error::FileDialogFailed)?;
        paths.push(PathBuf::from(path));
    }
    if paths.is_empty() {
        Err(Error::FileDialogFailed)
    } else {
        Ok(paths)
    }
}

impl UiDispatcher {
    /// Serializes the mounted widget tree after layout for visual QA and
    /// diagnostics. The snapshot contains geometry and resolved style data,
    /// but deliberately never includes text-field contents.
    pub fn layout_snapshot_json(&self) -> Result<String, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }

        let mut required = 0_usize;
        let query = self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_layout_snapshot_utf8(raw, std::ptr::null_mut(), 0, &mut required)
            })
            .ok_or(Error::WindowClosed)?;
        if query != -2 || required <= 1 {
            return Err(Error::LayoutSnapshotFailed);
        }

        let mut bytes = vec![0_u8; required];
        let result = self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_layout_snapshot_utf8(
                    raw,
                    bytes.as_mut_ptr().cast(),
                    bytes.len(),
                    &mut required,
                )
            })
            .ok_or(Error::WindowClosed)?;
        if result != 1 || required == 0 || required > bytes.len() {
            return Err(Error::LayoutSnapshotFailed);
        }
        bytes.truncate(required - 1);
        String::from_utf8(bytes).map_err(|_| Error::LayoutSnapshotFailed)
    }
}

impl UiDispatcher {
    /// Captures the composed client frame in opt-in QA builds. Production
    /// builds return false and never invoke operating-system screen capture.
    pub fn capture_frame_png(&self, path: &Path) -> bool {
        let Ok(path) = CString::new(path.to_string_lossy().as_bytes()) else {
            return false;
        };
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_capture_frame_png_utf8(raw, path.as_ptr()) != 0
            })
            .unwrap_or(false)
    }
}

impl UiDispatcher {
    /// Writes the developer-tools frame report (slow paints with their hottest
    /// component) as JSON. False unless developer tools are compiled in and
    /// enabled with ONEUI_DEVTOOLS=1.
    pub fn export_developer_report(&self, path: &Path) -> bool {
        let Ok(path) = CString::new(path.to_string_lossy().as_bytes()) else {
            return false;
        };
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_export_developer_report_utf8(raw, path.as_ptr()) != 0
            })
            .unwrap_or(false)
    }
}

impl UiDispatcher {
    /// Re-resolves and reapplies the window style sheet after runtime CSS
    /// custom-property changes.
    pub fn refresh_style_sheet(&self) -> Result<(), Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_refresh_style_sheet(raw);
        });
        Ok(())
    }
}

impl UiDispatcher {
    /// Changes the window-wide default UI font on the owning thread.
    pub fn set_default_font_family(&self, family: &str) -> Result<(), Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let family = sys::OneUiUtf8String::from_str(family.trim());
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_default_font_family_utf8(raw, family);
        });
        Ok(())
    }
}

impl UiDispatcher {
    /// Requests focus for a widget already mounted in this window.
    ///
    /// Focus changes are synchronous and must originate on the window thread.
    pub fn request_focus(&self, widget: &Widget, focus_visible: bool) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        Ok(self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_request_focus(raw, widget.as_raw(), i32::from(focus_visible)) != 0
            })
            .unwrap_or(false))
    }
}

impl UiDispatcher {
    /// Shows a platform-native file dialog owned by this window.
    ///
    /// Native dialogs run a nested platform message loop and therefore must be
    /// opened directly from the owning window thread. Product code should call
    /// this from a command callback; background services must first dispatch a
    /// short UI task and continue their I/O after the selected path is returned.
    pub fn file_dialog(&self, options: FileDialogOptions<'_>) -> Result<Option<PathBuf>, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        if options.filters.len() > 64 {
            return Err(Error::FileDialogFailed);
        }

        let initial_directory = options.initial_directory.to_string_lossy();
        let filters = options
            .filters
            .iter()
            .map(|filter| sys::OneUiFileDialogFilterUtf8 {
                name: sys::OneUiUtf8String::from_str(filter.name),
                pattern: sys::OneUiUtf8String::from_str(filter.pattern),
            })
            .collect::<Vec<_>>();
        let native = sys::OneUiFileDialogOptionsUtf8 {
            mode: match options.mode {
                FileDialogMode::OpenFile => sys::FILE_DIALOG_OPEN_FILE,
                FileDialogMode::SaveFile => sys::FILE_DIALOG_SAVE_FILE,
                FileDialogMode::SelectFolder => sys::FILE_DIALOG_SELECT_FOLDER,
            },
            title: sys::OneUiUtf8String::from_str(options.title),
            initial_directory: sys::OneUiUtf8String::from_str(&initial_directory),
            default_name: sys::OneUiUtf8String::from_str(options.default_name),
            default_extension: sys::OneUiUtf8String::from_str(options.default_extension),
            filters: filters.as_ptr(),
            filter_count: filters.len(),
            confirm_overwrite: i32::from(options.confirm_overwrite),
        };

        // A Windows file-system path is capped at 32,767 UTF-16 code units.
        // Four UTF-8 bytes per code unit plus NUL covers the ABI result without
        // reopening a modal dialog just to resize the caller's buffer.
        const MAX_FILE_DIALOG_UTF8_BYTES: usize = 32_767 * 4 + 1;
        let mut output = vec![0_u8; MAX_FILE_DIALOG_UTF8_BYTES];
        let mut required = 0_usize;
        let result = self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_file_dialog_utf8(
                    raw,
                    &native,
                    output.as_mut_ptr().cast(),
                    output.len(),
                    &mut required,
                )
            })
            .ok_or(Error::WindowClosed)?;
        match result {
            0 => Ok(None),
            1 if required > 0 && required <= output.len() => {
                output.truncate(required.saturating_sub(1));
                String::from_utf8(output)
                    .map(PathBuf::from)
                    .map(Some)
                    .map_err(|_| Error::FileDialogFailed)
            }
            _ => Err(Error::FileDialogFailed),
        }
    }
}

impl UiDispatcher {
    /// Shows an owner-bound multi-file or multi-folder picker on the window thread.
    ///
    /// The result preserves the native selection order. A canceled dialog
    /// returns an empty vector. Save dialogs are not accepted.
    pub fn file_dialog_multiple(
        &self,
        options: FileDialogOptions<'_>,
    ) -> Result<Vec<PathBuf>, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        if options.mode == FileDialogMode::SaveFile || options.filters.len() > 64 {
            return Err(Error::FileDialogFailed);
        }

        let initial_directory = options.initial_directory.to_string_lossy();
        let filters = options
            .filters
            .iter()
            .map(|filter| sys::OneUiFileDialogFilterUtf8 {
                name: sys::OneUiUtf8String::from_str(filter.name),
                pattern: sys::OneUiUtf8String::from_str(filter.pattern),
            })
            .collect::<Vec<_>>();
        let native = sys::OneUiFileDialogOptionsUtf8 {
            mode: if options.mode == FileDialogMode::SelectFolder {
                sys::FILE_DIALOG_SELECT_FOLDER
            } else {
                sys::FILE_DIALOG_OPEN_FILE
            },
            title: sys::OneUiUtf8String::from_str(options.title),
            initial_directory: sys::OneUiUtf8String::from_str(&initial_directory),
            default_name: sys::OneUiUtf8String::from_str(options.default_name),
            default_extension: sys::OneUiUtf8String::from_str(options.default_extension),
            filters: filters.as_ptr(),
            filter_count: filters.len(),
            confirm_overwrite: i32::from(options.confirm_overwrite),
        };

        // A single call must retain the shell selection while copying it out.
        // 16 MiB covers over one hundred maximum-length Windows paths without
        // reopening the modal dialog or holding native COM objects afterward.
        const MAX_MULTI_FILE_DIALOG_UTF8_BYTES: usize = 16 * 1024 * 1024;
        let mut output = vec![0_u8; MAX_MULTI_FILE_DIALOG_UTF8_BYTES];
        let mut required = 0_usize;
        let result = self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_file_dialog_multiple_utf8(
                    raw,
                    &native,
                    output.as_mut_ptr().cast(),
                    output.len(),
                    &mut required,
                )
            })
            .ok_or(Error::WindowClosed)?;
        match result {
            0 => Ok(Vec::new()),
            1 if required >= 2 && required <= output.len() => {
                output.truncate(required);
                parse_multiple_file_dialog_output(&output, required)
            }
            _ => Err(Error::FileDialogFailed),
        }
    }
}

impl UiDispatcher {
    /// System preference for motion in client-area UI, read on the UI thread.
    pub fn client_area_animations_enabled(&self) -> bool {
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_client_area_animations_enabled(raw) != 0 })
            .unwrap_or(false)
    }
}

impl UiDispatcher {
    /// Displays a platform-native confirmation prompt on the window thread
    /// and waits for the user's decision on the calling worker thread.
    ///
    /// This method is intended for background operations that must pause at a
    /// security or destructive-action boundary. It must not be called from the
    /// window thread because the caller waits until the dispatched UI work has
    /// completed. Closing the window while the request is queued returns
    /// [`Error::WindowClosed`].
    pub fn confirm_blocking(&self, title: &str, message: &str) -> Result<bool, Error> {
        if std::thread::current().id() == self.state.ui_thread {
            return Err(Error::UiThreadBlockingOperation);
        }
        let title = wide_null_terminated(title);
        let message = wide_null_terminated(message);
        let dispatcher = self.clone();
        let (sender, receiver) = std::sync::mpsc::sync_channel(1);
        self.dispatch(move || {
            let accepted = dispatcher.confirm_on_window_thread(&title, &message);
            let _ = sender.send(accepted);
        })?;
        receiver.recv().map_err(|_| Error::WindowClosed)
    }
}

impl UiDispatcher {
    /// Displays a platform-native confirmation prompt owned by this window.
    ///
    /// This synchronous form is intended for command callbacks already running
    /// on the window thread. The platform dialog owns its nested message loop;
    /// long-running work following confirmation must still run on a worker.
    pub fn confirm(&self, title: &str, message: &str) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        Ok(self
            .confirm_on_window_thread(&wide_null_terminated(title), &wide_null_terminated(message)))
    }
}

impl UiDispatcher {
    fn confirm_on_window_thread(&self, title: &[sys::WChar], message: &[sys::WChar]) -> bool {
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_confirm(raw, title.as_ptr(), message.as_ptr()) != 0
            })
            .unwrap_or(false)
    }
}

impl UiDispatcher {
    /// Displays a platform-native text or password prompt and waits on the
    /// calling worker thread. The entered value is never retained by OneUI.
    ///
    /// Like [`Self::confirm_blocking`], this method must not be called from the
    /// window thread. Closing the prompt returns `Ok(None)`; closing the owner
    /// before queued work runs returns [`Error::WindowClosed`].
    pub fn prompt_blocking(
        &self,
        title: &str,
        message: &str,
        options: PromptOptions<'_>,
    ) -> Result<Option<String>, Error> {
        if std::thread::current().id() == self.state.ui_thread {
            return Err(Error::UiThreadBlockingOperation);
        }
        let title = wide_null_terminated(title);
        let message = wide_null_terminated(message);
        let initial_value = SecretWide::new(options.initial_value);
        let placeholder = wide_null_terminated(options.placeholder);
        let password = options.password;
        let dispatcher = self.clone();
        let (sender, receiver) = std::sync::mpsc::sync_channel(1);
        self.dispatch(move || {
            let result = dispatcher.prompt_on_window_thread(
                &title,
                &message,
                initial_value.as_slice(),
                &placeholder,
                password,
            );
            let _ = sender.send(result);
        })?;
        receiver.recv().map_err(|_| Error::WindowClosed)
    }
}

impl UiDispatcher {
    /// Displays a platform-native text or password prompt owned by this window.
    ///
    /// Call this form from a window-thread command callback. The returned value
    /// is not retained by OneUI and the temporary UTF-16 buffer is cleared.
    pub fn prompt(
        &self,
        title: &str,
        message: &str,
        options: PromptOptions<'_>,
    ) -> Result<Option<String>, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let initial_value = SecretWide::new(options.initial_value);
        Ok(self.prompt_on_window_thread(
            &wide_null_terminated(title),
            &wide_null_terminated(message),
            initial_value.as_slice(),
            &wide_null_terminated(options.placeholder),
            options.password,
        ))
    }
}

impl UiDispatcher {
    fn prompt_on_window_thread(
        &self,
        title: &[sys::WChar],
        message: &[sys::WChar],
        initial_value: &[sys::WChar],
        placeholder: &[sys::WChar],
        password: bool,
    ) -> Option<String> {
        const MAX_PROMPT_CODE_UNITS: usize = 4096;
        let mut output = vec![0 as sys::WChar; MAX_PROMPT_CODE_UNITS];
        let accepted = self
            .state
            .with_raw(|raw| unsafe {
                sys::oneui_window_prompt_text(
                    raw,
                    title.as_ptr(),
                    message.as_ptr(),
                    initial_value.as_ptr(),
                    placeholder.as_ptr(),
                    i32::from(password),
                    output.as_mut_ptr(),
                    output.len() as i32,
                ) != 0
            })
            .unwrap_or(false);
        let value = accepted.then(|| {
            let length = output
                .iter()
                .position(|code_unit| *code_unit == 0)
                .unwrap_or(output.len());
            wide_to_string(&output[..length])
        });
        output.fill(0);
        value
    }
}

impl UiDispatcher {
    pub fn request_close(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_request_close(raw);
        });
    }
}

impl UiDispatcher {
    /// Updates the close-to-tray policy on the owning UI thread.
    pub fn set_close_to_tray(&self, close_to_tray: bool) -> Result<(), Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_set_close_to_tray(raw, i32::from(close_to_tray));
            })
            .ok_or(Error::WindowClosed)
    }
}

impl UiDispatcher {
    /// Shows or removes the window-owned tray entry on the owning UI thread.
    pub fn set_tray_enabled(&self, enabled: bool) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_set_tray_enabled(raw, i32::from(enabled)) != 0
            })
            .ok_or(Error::WindowClosed)
    }
}

impl UiDispatcher {
    pub fn tray_icon_visible(&self) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_tray_icon_visible(raw) != 0 })
            .ok_or(Error::WindowClosed)
    }
}

impl UiDispatcher {
    /// Emits a non-sensitive informational tray notification.
    pub fn notify_tray(&self, title: &str, message: &str) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        let title = wide_null_terminated(title);
        let message = wide_null_terminated(message);
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_notify_tray(raw, title.as_ptr(), message.as_ptr()) != 0
            })
            .ok_or(Error::WindowClosed)
    }
}

impl UiDispatcher {
    /// Closes the native window immediately on its owning UI thread.
    ///
    /// This is primarily used from a window close-policy callback after the
    /// application has decided that it is safe to exit. Calling
    /// [`Self::request_close`] there would emit the policy callback again.
    pub fn close(&self) -> Result<(), Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_close(raw) })
            .ok_or(Error::WindowClosed)
    }
}

impl UiDispatcher {
    /// Creates a thread-safe adaptive-layout producer for a mounted widget.
    pub fn widget_handle(&self, widget: &Widget) -> WidgetHandle {
        WidgetHandle {
            state: Arc::clone(&widget.state),
            dispatcher: self.clone(),
        }
    }
}

impl UiDispatcher {
    pub fn request_activate(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_activate(raw);
        });
    }
}

impl UiDispatcher {
    pub fn minimize(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_minimize(raw);
        });
    }
}

impl UiDispatcher {
    pub fn toggle_maximize(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_toggle_maximize(raw);
        });
    }
}

impl UiDispatcher {
    /// Changes immersive presentation at runtime. Worker-thread callers are
    /// marshalled to the owning UI thread.
    pub fn set_fullscreen(&self, fullscreen: bool) -> Result<(), Error> {
        if std::thread::current().id() == self.state.ui_thread {
            return self
                .state
                .with_raw(|raw| unsafe {
                    sys::oneui_window_set_fullscreen(raw, i32::from(fullscreen));
                })
                .ok_or(Error::WindowClosed);
        }
        let dispatcher = self.clone();
        self.dispatch(move || {
            let _ = dispatcher.set_fullscreen(fullscreen);
        })
    }
}

impl UiDispatcher {
    /// Reads immersive presentation state on the owning UI thread.
    pub fn is_fullscreen(&self) -> Result<bool, Error> {
        if std::thread::current().id() != self.state.ui_thread {
            return Err(Error::WrongThread);
        }
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_is_fullscreen(raw) != 0 })
            .ok_or(Error::WindowClosed)
    }
}

impl UiDispatcher {
    /// Keeps the logical title-bar span between the two insets interactive as
    /// client content instead of treating it as a native drag caption.
    pub fn set_title_bar_interactive_insets(&self, leading_width: f32, trailing_width: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_title_bar_interactive_insets(raw, leading_width, trailing_width);
        });
    }
}

impl UiDispatcher {
    /// Restores caption dragging across the title bar except for the reserved
    /// trailing window-button strip.
    pub fn clear_title_bar_interactive_insets(&self) {
        self.set_title_bar_interactive_insets(-1.0, -1.0);
    }
}
