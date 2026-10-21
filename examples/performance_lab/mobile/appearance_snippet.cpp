#include <oneui/mobile/appearance.h>

// Owned by the UI thread. Android SystemAppearance forwards configuration
// events; other native adapters must report their actual availability.
void mobileAppearanceExample(oneui::mobile::Appearance &appearance) {
    OneUiMobileAppearanceStateV1 state{};
    appearance.state(state);
    // system_theme remains the system value even when the app overrides it.
    const bool systemDark = state.system_theme == ONEUI_MOBILE_THEME_DARK;
    (void)systemDark;

    auto options = state.options;
    options.theme_mode = ONEUI_MOBILE_THEME_SYSTEM;
    options.custom_bar_colors = 1;
    options.status_argb = 0xff28613d;
    options.navigation_argb = 0xfff5f7f4;
    options.status_icons = ONEUI_MOBILE_ICONS_AUTO;
    options.navigation_icons = ONEUI_MOBILE_ICONS_AUTO;
    appearance.setOptions(options);
    // Accepted request only. The native adapter applies the resolved colors
    // and icons, then acknowledges the exact revision on the UI thread.
}
