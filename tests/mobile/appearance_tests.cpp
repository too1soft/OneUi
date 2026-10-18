#include "oneui/mobile/appearance.h"
#include <iostream>
#include <stdexcept>
#include <thread>

using oneui::mobile::Appearance;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
OneUiMobileAppearanceStateV1 state(Appearance &a) {
    OneUiMobileAppearanceStateV1 s{};
    require(a.state(s) == ONEUI_MOBILE_OK, "state");
    return s;
}
int main() {
    Appearance a;
    auto s = state(a);
    require(!s.host_available && s.system_theme == 0 && s.effective_theme == 1,
            "honest unknown fallback");
    require(a.acknowledge(s.revision) == ONEUI_MOBILE_INVALID_STATE, "no implicit native adapter");
    a.observe(2, true);
    s = state(a);
    require(s.system_theme == 2 && s.effective_theme == 2 && s.status_icons == 1, "follow dark");
    require(a.acknowledge(s.revision) == ONEUI_MOBILE_OK, "native ack");
    a.observe(2, true);
    require(state(a).revision == s.revision && state(a).applied_revision == s.revision,
            "identical events are idle");
    auto options = s.options;
    options.theme_mode = 1;
    a.setOptions(options);
    s = state(a);
    require(s.system_theme == 2 && s.effective_theme == 1 && s.status_icons == 2,
            "override independent of system");
    require(a.acknowledge(s.revision - 1) == ONEUI_MOBILE_INVALID_STATE, "reject stale ack");
    options.custom_bar_colors = 1;
    options.status_argb = 0xff28613d;
    options.navigation_argb = 0xfff5f7f4;
    a.setOptions(options);
    s = state(a);
    require(s.status_argb == options.status_argb && s.status_icons == 1 && s.navigation_icons == 2,
            "per-bar contrast");
    options.status_argb = 0x0028613d;
    require(a.setOptions(options) == ONEUI_MOBILE_INVALID_ARGUMENT &&
                state(a).revision == s.revision,
            "invalid input atomic");
    options = s.options;
    options.status_icons = 2;
    a.setOptions(options);
    require(state(a).status_icons == 2, "explicit icon choice");
    a.observe(1, false);
    require(state(a).applied_revision == 0, "disconnect invalidates application");
    a.observe(1, true);
    require(state(a).applied_revision == 0, "reconnect requires reapply");
    options = {};
    a.setOptions(options);
    require(state(a).effective_theme == 1, "return to follow system");
    a.observe(2, true);
    require(state(a).effective_theme == 2, "follow resumes");
    bool safe = false;
    std::thread other([&] {
        OneUiMobileAppearanceStateV1 untouched{};
        untouched.revision = 999;
        safe = a.state(untouched) == ONEUI_MOBILE_WRONG_THREAD && untouched.revision == 999 &&
               a.setOptions({}) == ONEUI_MOBILE_WRONG_THREAD &&
               a.observe(1, true) == ONEUI_MOBILE_WRONG_THREAD;
    });
    other.join();
    require(safe, "thread confinement");
    std::cout
        << "Appearance policy, atomic validation, host acknowledgement and confinement passed\n";
}
