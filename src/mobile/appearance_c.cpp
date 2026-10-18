#include "oneui/mobile/appearance.h"

struct OneUiMobileAppearance {
    oneui::mobile::Appearance value;
};
extern "C" {
OneUiMobileResult oneui_mobile_appearance_create(OneUiMobileAppearance **out) {
    if (!out)
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    *out = nullptr;
    try {
        *out = new OneUiMobileAppearance;
        return ONEUI_MOBILE_OK;
    } catch (...) {
        return ONEUI_MOBILE_FAILED;
    }
}
OneUiMobileResult oneui_mobile_appearance_destroy(OneUiMobileAppearance *a) {
    if (!a)
        return ONEUI_MOBILE_OK;
    const auto result = a->value.checkThread();
    if (result == ONEUI_MOBILE_OK)
        delete a;
    return result;
}
OneUiMobileResult oneui_mobile_appearance_set_options_v1(OneUiMobileAppearance *a,
                                                         const OneUiMobileAppearanceOptionsV1 *o) {
    return a && o ? a->value.setOptions(*o) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_appearance_get_state_v1(OneUiMobileAppearance *a,
                                                       OneUiMobileAppearanceStateV1 *s) {
    return a && s ? a->value.state(*s) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_appearance_observe_v1(OneUiMobileAppearance *a, int32_t theme,
                                                     uint32_t available) {
    return a && available <= 1 ? a->value.observe(theme, available != 0)
                               : ONEUI_MOBILE_INVALID_ARGUMENT;
}
OneUiMobileResult oneui_mobile_appearance_acknowledge_v1(OneUiMobileAppearance *a,
                                                         uint64_t revision) {
    return a ? a->value.acknowledge(revision) : ONEUI_MOBILE_INVALID_ARGUMENT;
}
}
