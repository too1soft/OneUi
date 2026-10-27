use super::*;

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
        if unsafe { sys::oneui_window_initialize_checked(raw.as_ptr()) } == 0 {
            unsafe { sys::oneui_window_destroy(raw.as_ptr()) };
            return Err(Error::WindowCreationFailed);
        }
        Ok(Self {
            state: Arc::new(WindowState {
                raw: Mutex::new(Some(raw)),
                ui_thread: std::thread::current().id(),
            }),
            raw_key_callback: None,
            client_size_changed_callback: None,
            activation_changed_callback: None,
            file_drop_callback: None,
            close_requested_callback: None,
            tray_action_callback: None,
            menu_action_callback: None,
            _ui_thread: PhantomData,
        })
    }

    pub fn set_title(&self, title: &str) {
        let title = sys::OneUiUtf8String::from_str(title);
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_title_utf8(raw, title);
        });
    }

    pub fn set_borderless(&self, borderless: bool) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_borderless(raw, i32::from(borderless));
        });
    }

    pub fn set_fullscreen(&self, fullscreen: bool) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_fullscreen(raw, i32::from(fullscreen));
        });
    }

    pub fn is_fullscreen(&self) -> bool {
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_is_fullscreen(raw) != 0 })
            .unwrap_or(false)
    }

    pub fn set_minimum_client_size(&self, width: f32, height: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_minimum_client_size(raw, width.max(0.0), height.max(0.0));
        });
    }

    pub fn placement(&self) -> Option<WindowPlacement> {
        self.state.with_raw(|raw| {
            let mut native = sys::OneUiWindowPlacement::default();
            let available = unsafe { sys::oneui_window_get_placement(raw, &mut native) } != 0;
            available.then_some(WindowPlacement {
                x: native.x,
                y: native.y,
                width: native.width,
                height: native.height,
                maximized: native.maximized != 0,
            })
        })?
    }

    pub fn set_placement(&self, placement: WindowPlacement) -> bool {
        let native = sys::OneUiWindowPlacement {
            x: placement.x,
            y: placement.y,
            width: placement.width,
            height: placement.height,
            maximized: i32::from(placement.maximized),
        };
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_set_placement(raw, &native) != 0 })
            .unwrap_or(false)
    }

    /// Sets the logical caption height and trailing client-area width reserved
    /// for custom window buttons on a borderless window.
    pub fn set_title_bar_drag_metrics(&self, title_bar_height: f32, reserved_button_width: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_title_bar_drag_metrics(
                raw,
                title_bar_height,
                reserved_button_width,
            );
        });
    }

    /// Keeps the logical title-bar span between the two insets interactive as
    /// client content instead of treating it as a native drag caption.
    pub fn set_title_bar_interactive_insets(&self, leading_width: f32, trailing_width: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_title_bar_interactive_insets(raw, leading_width, trailing_width);
        });
    }

    /// Restores caption dragging across the title bar except for the reserved
    /// trailing window-button strip.
    pub fn clear_title_bar_interactive_insets(&self) {
        self.set_title_bar_interactive_insets(-1.0, -1.0);
    }

    pub fn set_corner_radius(&self, radius: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_corner_radius(raw, radius);
        });
    }

    /// Chooses whether a normal close request hides this window to its tray
    /// entry instead of destroying it.
    pub fn set_close_to_tray(&self, close_to_tray: bool) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_close_to_tray(raw, i32::from(close_to_tray));
        });
    }

    /// Shows or removes the window-owned system tray entry.
    pub fn set_tray_enabled(&self, enabled: bool) -> bool {
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_set_tray_enabled(raw, i32::from(enabled)) != 0
            })
            .unwrap_or(false)
    }

    pub fn tray_icon_visible(&self) -> bool {
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_tray_icon_visible(raw) != 0 })
            .unwrap_or(false)
    }

    /// Shows a non-sensitive informational notification through the current
    /// tray entry.
    pub fn notify_tray(&self, title: &str, message: &str) -> bool {
        let title = wide_null_terminated(title);
        let message = wide_null_terminated(message);
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_notify_tray(raw, title.as_ptr(), message.as_ptr()) != 0
            })
            .unwrap_or(false)
    }

    pub fn set_content(&self, content: &Widget) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_content(raw, content.as_raw());
        });
    }

    /// Borrows the platform window handle on its owning UI thread.
    /// The callback must not destroy, transfer ownership of, or outlive this window.
    pub unsafe fn with_platform_handle<R>(
        &self,
        callback: impl FnOnce(*mut std::ffi::c_void) -> R,
    ) -> Option<R> {
        self.state
            .with_raw(|raw| callback(sys::oneui_window_native_handle(raw)))
    }

    pub fn request_focus(&self, widget: &Widget, focus_visible: bool) -> bool {
        self.state
            .with_raw(|raw| unsafe {
                sys::oneui_window_request_focus(raw, widget.as_raw(), i32::from(focus_visible)) != 0
            })
            .unwrap_or(false)
    }

    /// Installs the application theme before composing the window tree.
    /// Widgets created afterwards inherit it, and explicit widget styling can
    /// still use [`Widget::apply_style_sheet`] where needed.
    pub fn set_style_sheet(&self, style_sheet: &StyleSheet) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_style_sheet(raw, style_sheet.as_raw());
        });
    }

    /// Sets the font family used by controls that request the standard UI
    /// font. Passing an empty string restores the platform default. This is a
    /// window-wide setting and applies without rebuilding the widget tree.
    pub fn set_default_font_family(&self, family: &str) {
        let family = sys::OneUiUtf8String::from_str(family.trim());
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_default_font_family_utf8(raw, family);
        });
    }

    /// Scales the whole client area on top of the monitor DPI (1.0 = 100%).
    /// Layout sees a correspondingly larger or smaller logical client size;
    /// the native window keeps its size. Win32 accepts 1.0..=2.25, the
    /// macOS and Linux backends 0.25..=4.0; other values are clamped or ignored.
    pub fn set_content_scale(&self, scale: f32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_content_scale(raw, scale);
        });
    }

    pub fn show(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_show(raw);
        });
    }

    pub fn center_on_active_monitor(&self) -> bool {
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_center_on_active_monitor(raw) != 0 })
            .unwrap_or(false)
    }

    pub fn show_with_fade(&self, duration_ms: u32) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_show_with_fade(raw, duration_ms);
        });
    }

    pub fn client_area_animations_enabled(&self) -> bool {
        self.state
            .with_raw(|raw| unsafe { sys::oneui_window_client_area_animations_enabled(raw) != 0 })
            .unwrap_or(true)
    }

    pub fn activate(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_activate(raw);
        });
    }

    pub fn run(&self) -> i32 {
        self.state
            .current_raw()
            .map(|raw| unsafe { sys::oneui_window_run(raw.as_ptr()) })
            .unwrap_or(-1)
    }

    pub fn close(&self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_close(raw);
        });
    }

    pub fn dispatcher(&self) -> UiDispatcher {
        UiDispatcher {
            state: Arc::clone(&self.state),
        }
    }

    pub fn layout_snapshot_json(&self) -> Result<String, Error> {
        self.dispatcher().layout_snapshot_json()
    }

    /// Captures the composed client frame in opt-in QA builds.
    pub fn capture_frame_png(&self, path: &Path) -> bool {
        self.dispatcher().capture_frame_png(path)
    }

    pub fn file_dialog(&self, options: FileDialogOptions<'_>) -> Result<Option<PathBuf>, Error> {
        self.dispatcher().file_dialog(options)
    }

    pub fn file_dialog_multiple(
        &self,
        options: FileDialogOptions<'_>,
    ) -> Result<Vec<PathBuf>, Error> {
        self.dispatcher().file_dialog_multiple(options)
    }

    pub fn confirm(&self, title: &str, message: &str) -> Result<bool, Error> {
        self.dispatcher().confirm(title, message)
    }

    pub fn prompt(
        &self,
        title: &str,
        message: &str,
        options: PromptOptions<'_>,
    ) -> Result<Option<String>, Error> {
        self.dispatcher().prompt(title, message, options)
    }

    /// Returns the only thread-safe update path for a terminal mounted in this
    /// window. The terminal itself remains UI-thread bound.
    pub fn terminal_view_handle(&self, terminal: &TerminalView) -> TerminalViewHandle {
        self.dispatcher().terminal_view_handle(terminal)
    }

    /// Returns a thread-safe coalescing producer for decoded remote frames.
    pub fn realtime_frame_view_handle(
        &self,
        frame_view: &RealtimeFrameView,
    ) -> RealtimeFrameViewHandle {
        self.dispatcher().realtime_frame_view_handle(frame_view)
    }

    /// Returns a thread-safe coalescing producer for remote dimensions and
    /// server cursor presentation. The input region remains UI-thread bound.
    pub fn remote_input_region_handle(
        &self,
        input_region: &RemoteInputRegion,
    ) -> RemoteInputRegionHandle {
        self.dispatcher().remote_input_region_handle(input_region)
    }

    /// Returns a thread-safe adaptive-layout handle for a mounted widget.
    pub fn widget_handle(&self, widget: &Widget) -> WidgetHandle {
        self.dispatcher().widget_handle(widget)
    }

    /// Returns the only thread-safe update path for a label mounted in this
    /// window. Bursts are coalesced to the latest text value.
    pub fn label_handle(&self, label: &Label) -> LabelHandle {
        self.dispatcher().label_handle(label)
    }

    /// Returns the thread-safe update path for a mounted single-line text field.
    pub fn text_field_handle(&self, text_field: &TextField) -> TextFieldHandle {
        self.dispatcher().text_field_handle(text_field)
    }

    /// Returns the thread-safe update path for a mounted multiline editor.
    pub fn text_area_handle(&self, text_area: &TextArea) -> TextAreaHandle {
        self.dispatcher().text_area_handle(text_area)
    }

    /// Returns the thread-safe item producer for a mounted select.
    pub fn select_handle(&self, select: &Select) -> SelectHandle {
        self.dispatcher().select_handle(select)
    }

    /// Returns the thread-safe scroll producer for a mounted scroll view.
    pub fn scroll_view_handle(&self, scroll_view: &ScrollView) -> ScrollViewHandle {
        self.dispatcher().scroll_view_handle(scroll_view)
    }

    /// Returns the only thread-safe update path for a mounted progress bar.
    pub fn progress_bar_handle(&self, progress_bar: &ProgressBar) -> ProgressBarHandle {
        self.dispatcher().progress_bar_handle(progress_bar)
    }

    /// Returns the thread-safe update path for a mounted sparkline.
    pub fn sparkline_handle(&self, sparkline: &Sparkline) -> SparklineHandle {
        self.dispatcher().sparkline_handle(sparkline)
    }

    /// Returns the thread-safe update path for a mounted operational chart.
    pub fn time_series_chart_handle(&self, chart: &TimeSeriesChart) -> TimeSeriesChartHandle {
        self.dispatcher().time_series_chart_handle(chart)
    }

    /// Returns the thread-safe update path for rows in a mounted virtual list.
    pub fn virtual_list_handle(&self, list: &VirtualList) -> VirtualListHandle {
        self.dispatcher().virtual_list_handle(list)
    }

    pub fn table_handle(&self, table: &Table) -> TableHandle {
        self.dispatcher().table_handle(table)
    }

    pub fn dispatch<F>(&self, task: F) -> Result<(), Error>
    where
        F: FnOnce() + Send + 'static,
    {
        self.dispatcher().dispatch(task)
    }
}
