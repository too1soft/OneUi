#[cfg(target_os = "macos")]
unsafe extern "C" {
    pub fn oneui_macos_set_open_uri_callback(
        callback: Option<
            unsafe extern "C" fn(*const std::ffi::c_char, usize, *mut std::ffi::c_void),
        >,
        data: *mut std::ffi::c_void,
    ) -> i32;
}
