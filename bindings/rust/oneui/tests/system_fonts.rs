#[test]
fn installed_fonts_can_be_enumerated_without_a_window_on_a_worker() {
    let fonts = std::thread::spawn(oneui::enumerate_system_fonts)
        .join()
        .unwrap()
        .unwrap();
    assert!(fonts.windows(2).all(|pair| pair[0].family < pair[1].family));
    assert!(fonts
        .iter()
        .all(|font| !font.family.trim().is_empty() && !font.family.contains('\0')));
    let count = unsafe { oneui::sys::oneui_enumerate_system_fonts(None, std::ptr::null_mut()) };
    assert!(count >= 0);
    #[cfg(target_os = "windows")]
    {
        assert!(!fonts.is_empty());
        assert!(fonts.iter().any(|font| font.monospaced));
    }
}
