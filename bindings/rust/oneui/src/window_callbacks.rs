use super::*;

impl Window {
    /// Installs a window-level key handler that runs before focused widgets.
    /// Returning `true` consumes the event. Text composition remains routed
    /// through the normal IME/text-input path.
    #[track_caller]
    pub fn set_on_raw_key<F>(&mut self, callback: F)
    where
        F: FnMut(RawKeyEvent) -> bool + 'static,
    {
        let trace = InteractionTrace::at("Window", "raw_key", std::panic::Location::caller());
        self.clear_raw_key_callback();
        self.raw_key_callback = Some(Box::new(WindowRawKeyCallback {
            handler: Box::new(traced_value_result_callback(trace, callback)),
        }));
        let user_data = (self
            .raw_key_callback
            .as_deref_mut()
            .expect("window raw key callback was just installed")
            as *mut WindowRawKeyCallback)
            .cast();
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_raw_key(raw, Some(run_window_raw_key_callback), user_data);
        });
    }

    pub fn clear_raw_key_callback(&mut self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_raw_key(raw, None, std::ptr::null_mut());
        });
        self.raw_key_callback = None;
    }

    /// Installs a coalesced logical client-size callback. The callback runs on
    /// the window UI thread after the backend has committed the latest resize
    /// message for the frame.
    pub fn set_on_client_size_changed<F>(&mut self, callback: F)
    where
        F: FnMut(f32, f32) + 'static,
    {
        self.clear_client_size_changed_callback();
        self.client_size_changed_callback = Some(Box::new(WindowClientSizeChangedCallback {
            handler: Box::new(callback),
        }));
        let user_data = (self
            .client_size_changed_callback
            .as_deref_mut()
            .expect("window client-size callback was just installed")
            as *mut WindowClientSizeChangedCallback)
            .cast();
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_client_size_changed(
                raw,
                Some(run_window_client_size_changed_callback),
                user_data,
            );
        });
    }

    pub fn clear_client_size_changed_callback(&mut self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_client_size_changed(raw, None, std::ptr::null_mut());
        });
        self.client_size_changed_callback = None;
    }

    /// Installs a top-level activation callback. The callback runs on the
    /// owning UI thread when the native window becomes active or inactive,
    /// including application switches and minimization.
    pub fn set_on_activation_changed<F>(&mut self, callback: F)
    where
        F: FnMut(bool) + 'static,
    {
        self.clear_activation_changed_callback();
        self.activation_changed_callback = Some(Box::new(WindowActivationChangedCallback {
            handler: Box::new(callback),
        }));
        let user_data = (self
            .activation_changed_callback
            .as_deref_mut()
            .expect("window activation callback was just installed")
            as *mut WindowActivationChangedCallback)
            .cast();
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_activation_changed(
                raw,
                Some(run_window_activation_changed_callback),
                user_data,
            );
        });
    }

    pub fn clear_activation_changed_callback(&mut self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_activation_changed(raw, None, std::ptr::null_mut());
        });
        self.activation_changed_callback = None;
    }

    /// Installs a native filesystem-drop callback. The event owns every path
    /// and reports its position in logical window coordinates.
    #[track_caller]
    pub fn set_on_file_drop<F>(&mut self, callback: F)
    where
        F: FnMut(FileDropEvent) + 'static,
    {
        self.clear_file_drop_callback();
        self.file_drop_callback = Some(Box::new(WindowFileDropCallback {
            handler: Box::new(callback),
        }));
        let user_data = (self
            .file_drop_callback
            .as_deref_mut()
            .expect("window file-drop callback was just installed")
            as *mut WindowFileDropCallback)
            .cast();
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_file_drop(raw, Some(run_window_file_drop_callback), user_data);
        });
    }

    pub fn clear_file_drop_callback(&mut self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_file_drop(raw, None, std::ptr::null_mut());
        });
        self.file_drop_callback = None;
    }

    /// Installs the policy callback for native close requests such as Alt+F4,
    /// the taskbar close command, and operating-system session shutdown.
    ///
    /// Installing this callback replaces OneUI's default immediate-close
    /// behavior. The application must call [`UiDispatcher::close`] when it has
    /// decided that closing is safe. `force_exit` is true for system shutdown
    /// and explicit tray Exit requests.
    pub fn set_on_close_requested<F>(&mut self, callback: F)
    where
        F: FnMut(bool) + 'static,
    {
        self.clear_close_requested_callback();
        self.close_requested_callback = Some(Box::new(WindowCloseRequestedCallback {
            handler: Box::new(callback),
        }));
        let user_data = (self
            .close_requested_callback
            .as_deref_mut()
            .expect("window close callback was just installed")
            as *mut WindowCloseRequestedCallback)
            .cast();
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_close_requested(
                raw,
                Some(run_window_close_requested_callback),
                user_data,
            );
        });
    }

    pub fn clear_close_requested_callback(&mut self) {
        self.state.with_raw(|raw| unsafe {
            sys::oneui_window_set_on_close_requested(raw, None, std::ptr::null_mut());
        });
        self.close_requested_callback = None;
    }
}
