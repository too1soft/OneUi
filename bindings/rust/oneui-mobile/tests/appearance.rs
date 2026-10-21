use oneui_mobile::{appearance::*, Error};
#[test]
fn follow_override_restore_and_host_ack() {
    let mut appearance = Appearance::new().unwrap();
    assert!(!appearance.state().unwrap().host_available);
    appearance.observe(SystemTheme::Dark, true).unwrap();
    let state = appearance.state().unwrap();
    assert_eq!(state.effective_theme, SystemTheme::Dark);
    appearance.acknowledge(state.revision).unwrap();
    appearance
        .set_options(AppearanceOptions {
            theme: ThemeMode::Light,
            bar_colors: Some((0xff28613d, 0xfff5f7f4)),
            ..Default::default()
        })
        .unwrap();
    let state = appearance.state().unwrap();
    assert_eq!(state.system_theme, SystemTheme::Dark);
    assert_eq!(state.effective_theme, SystemTheme::Light);
    assert_eq!(state.status_icons, BarIcons::Light);
    assert_eq!(state.navigation_icons, BarIcons::Dark);
    assert_ne!(state.revision, state.applied_revision);
    assert_eq!(
        appearance.acknowledge(state.revision - 1),
        Err(Error::InvalidState)
    );
    assert_eq!(
        appearance.set_options(AppearanceOptions {
            bar_colors: Some((0x112233, 0xff445566)),
            ..Default::default()
        }),
        Err(Error::InvalidArgument)
    );
    assert_eq!(appearance.state().unwrap().revision, state.revision);
    appearance.set_options(Default::default()).unwrap();
    assert_eq!(
        appearance.state().unwrap().effective_theme,
        SystemTheme::Dark
    );
    appearance.observe(SystemTheme::Light, false).unwrap();
    assert_eq!(appearance.state().unwrap().applied_revision, 0);
}
