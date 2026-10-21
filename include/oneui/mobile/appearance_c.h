#pragma once
#include "oneui/mobile/c_api.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct OneUiMobileAppearance OneUiMobileAppearance;
typedef enum OneUiMobileThemeMode {
    ONEUI_MOBILE_THEME_SYSTEM = 0,
    ONEUI_MOBILE_THEME_LIGHT = 1,
    ONEUI_MOBILE_THEME_DARK = 2
} OneUiMobileThemeMode;
typedef enum OneUiMobileBarIcons {
    ONEUI_MOBILE_ICONS_AUTO = 0,
    ONEUI_MOBILE_ICONS_LIGHT = 1,
    ONEUI_MOBILE_ICONS_DARK = 2
} OneUiMobileBarIcons;
/* Separate additive V1 API; existing mobile and desktop records are unchanged.
 * Zero options = follow system and theme-colored bars with contrasting icons.
 * Colors are opaque 0xAARRGGBB. Transparent/edge-to-edge content is not this API.
 * Native hosts draw bar protection behind transparent OS bars when required. */
typedef struct OneUiMobileAppearanceOptionsV1 {
    int32_t theme_mode;
    uint32_t custom_bar_colors;
    uint32_t status_argb, navigation_argb;
    int32_t status_icons, navigation_icons;
} OneUiMobileAppearanceOptionsV1;
typedef struct OneUiMobileAppearanceStateV1 {
    OneUiMobileAppearanceOptionsV1 options;
    int32_t system_theme;    /* 0 unknown, 1 light, 2 dark; independent of override. */
    int32_t effective_theme; /* Unknown system falls back to light. */
    uint32_t host_available;
    uint32_t status_argb, navigation_argb;
    int32_t status_icons, navigation_icons; /* Resolved, never AUTO. */
    uint64_t revision, applied_revision;
} OneUiMobileAppearanceStateV1;

/* All calls and destruction belong to the creating UI thread. No OS host is
 * installed implicitly. set_options accepts a request; applied_revision only
 * advances when the adapter acknowledges it, not when the compositor presents. */
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_appearance_create(OneUiMobileAppearance **);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_appearance_destroy(OneUiMobileAppearance *);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_appearance_set_options_v1(
    OneUiMobileAppearance *, const OneUiMobileAppearanceOptionsV1 *);
ONEUI_MOBILE_API OneUiMobileResult
oneui_mobile_appearance_get_state_v1(OneUiMobileAppearance *, OneUiMobileAppearanceStateV1 *);
/* Host integration: initial/current system observation, availability and ack.
 * Disconnect clears applied_revision. Reconnect always requires reapplication. */
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_appearance_observe_v1(OneUiMobileAppearance *,
                                                                      int32_t system_theme,
                                                                      uint32_t host_available);
ONEUI_MOBILE_API OneUiMobileResult oneui_mobile_appearance_acknowledge_v1(OneUiMobileAppearance *,
                                                                          uint64_t revision);
#ifdef __cplusplus
}
#endif
