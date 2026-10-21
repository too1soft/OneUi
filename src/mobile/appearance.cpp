#include "oneui/mobile/appearance.h"
#include <cmath>

namespace oneui::mobile {
namespace {
int32_t icons(uint32_t argb, int32_t requested) {
    if (requested != ONEUI_MOBILE_ICONS_AUTO)
        return requested;
    auto linear = [](uint32_t channel) {
        const double s = double(channel & 255) / 255;
        return s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
    };
    const double luminance =
        .2126 * linear(argb >> 16) + .7152 * linear(argb >> 8) + .0722 * linear(argb);
    return luminance > .179 ? ONEUI_MOBILE_ICONS_DARK : ONEUI_MOBILE_ICONS_LIGHT;
}
bool equal(const OneUiMobileAppearanceOptionsV1 &a, const OneUiMobileAppearanceOptionsV1 &b) {
    return a.theme_mode == b.theme_mode && a.custom_bar_colors == b.custom_bar_colors &&
           a.status_argb == b.status_argb && a.navigation_argb == b.navigation_argb &&
           a.status_icons == b.status_icons && a.navigation_icons == b.navigation_icons;
}
} // namespace
Appearance::Appearance() : thread_(std::this_thread::get_id()) {
    state_.revision = 1;
    resolve();
}
OneUiMobileResult Appearance::checkThread() const {
    return thread_ == std::this_thread::get_id() ? ONEUI_MOBILE_OK : ONEUI_MOBILE_WRONG_THREAD;
}
void Appearance::resolve() {
    const auto &o = state_.options;
    state_.effective_theme =
        o.theme_mode ? o.theme_mode
                     : (state_.system_theme ? state_.system_theme : ONEUI_MOBILE_THEME_LIGHT);
    const uint32_t background =
        state_.effective_theme == ONEUI_MOBILE_THEME_DARK ? 0xff171e21 : 0xfff5f7f4;
    state_.status_argb = o.custom_bar_colors ? o.status_argb : background;
    state_.navigation_argb = o.custom_bar_colors ? o.navigation_argb : background;
    state_.status_icons = icons(state_.status_argb, o.status_icons);
    state_.navigation_icons = icons(state_.navigation_argb, o.navigation_icons);
}
OneUiMobileResult Appearance::setOptions(const OneUiMobileAppearanceOptionsV1 &o) {
    if (checkThread() != ONEUI_MOBILE_OK)
        return ONEUI_MOBILE_WRONG_THREAD;
    if (o.theme_mode < 0 || o.theme_mode > 2 || o.custom_bar_colors > 1 || o.status_icons < 0 ||
        o.status_icons > 2 || o.navigation_icons < 0 || o.navigation_icons > 2 ||
        (o.custom_bar_colors && ((o.status_argb >> 24) != 255 || (o.navigation_argb >> 24) != 255)))
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    if (!equal(o, state_.options)) {
        state_.options = o;
        ++state_.revision;
        resolve();
    }
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult Appearance::observe(int32_t theme, bool available) {
    if (checkThread() != ONEUI_MOBILE_OK)
        return ONEUI_MOBILE_WRONG_THREAD;
    if (theme < 0 || theme > 2)
        return ONEUI_MOBILE_INVALID_ARGUMENT;
    if (theme != state_.system_theme || available != bool(state_.host_available)) {
        state_.system_theme = theme;
        state_.host_available = available;
        state_.applied_revision = 0;
        ++state_.revision;
        resolve();
    }
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult Appearance::state(OneUiMobileAppearanceStateV1 &out) const {
    if (checkThread() != ONEUI_MOBILE_OK)
        return ONEUI_MOBILE_WRONG_THREAD;
    out = state_;
    return ONEUI_MOBILE_OK;
}
OneUiMobileResult Appearance::acknowledge(uint64_t revision) {
    if (checkThread() != ONEUI_MOBILE_OK)
        return ONEUI_MOBILE_WRONG_THREAD;
    if (!state_.host_available || revision != state_.revision)
        return ONEUI_MOBILE_INVALID_STATE;
    state_.applied_revision = revision;
    return ONEUI_MOBILE_OK;
}
} // namespace oneui::mobile
