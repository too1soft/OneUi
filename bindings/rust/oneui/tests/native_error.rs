use oneui::{
    clear_native_error, last_native_error, set_builtin_text, BuiltinText, LocalizationError,
    NativeErrorCode,
};

#[test]
fn native_error_is_owned_sticky_and_thread_local() {
    clear_native_error();
    assert_eq!(last_native_error(), None);
    assert_eq!(
        set_builtin_text(BuiltinText::Accept, Some("")),
        Err(LocalizationError::Rejected)
    );
    let error = last_native_error().expect("native validation reason");
    assert_eq!(error.code, NativeErrorCode::InvalidArgument);
    assert!(error.message.contains("oneui_set_builtin_text_utf8"));
    assert!(error.message.contains("1 to 4096 UTF-8 bytes"));

    // Reading the runtime ABI is a successful operation without modifying text.
    // SAFETY: This no-argument query has no ownership or thread requirements.
    assert_eq!(
        unsafe { oneui::sys::oneui_utf8_abi_version() },
        oneui::sys::UTF8_ABI_VERSION
    );
    assert_eq!(last_native_error(), Some(error.clone()));
    std::thread::spawn(|| {
        assert_eq!(last_native_error(), None);
        assert!(set_builtin_text(BuiltinText::Cancel, Some("")).is_err());
        assert_eq!(
            last_native_error().unwrap().code,
            NativeErrorCode::InvalidArgument
        );
        clear_native_error();
        assert_eq!(last_native_error(), None);
    })
    .join()
    .unwrap();
    assert_eq!(last_native_error(), Some(error.clone()));
    clear_native_error();
    assert_eq!(last_native_error(), None);
    assert!(
        error.message.contains("oneui_set_builtin_text_utf8"),
        "owned copy survives clear"
    );
}
