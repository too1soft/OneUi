#pragma once
#include "oneui/mobile/appearance_c.h"
#include <thread>

namespace oneui::mobile {
// UI-thread policy/state shared by native hosts. No global application state,
// polling timer or OS dependency; native configuration events drive observe().
class ONEUI_MOBILE_API Appearance final {
  public:
    Appearance();
    Appearance(const Appearance &) = delete;
    Appearance &operator=(const Appearance &) = delete;
    OneUiMobileResult checkThread() const;
    OneUiMobileResult setOptions(const OneUiMobileAppearanceOptionsV1 &options);
    OneUiMobileResult state(OneUiMobileAppearanceStateV1 &out) const;
    OneUiMobileResult observe(int32_t systemTheme, bool hostAvailable);
    OneUiMobileResult acknowledge(uint64_t revision);

  private:
    std::thread::id thread_;
    OneUiMobileAppearanceStateV1 state_{};
    void resolve();
};
} // namespace oneui::mobile
