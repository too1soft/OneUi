#include "platform/shared/window_fit.h"

#include <cmath>
#include <cstdio>

namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}
bool near(double a, double b) { return std::abs(a - b) < 0.001; }
} // namespace

int main() {
    using oneui::platform::WindowFitRect;
    using oneui::platform::fitWindowRect;
    using oneui::platform::initialWindowRect;
    using oneui::platform::windowExceedsArea;

    // 1600x1000 default on a 1512x945 point MacBook work area below a 37pt menu bar.
    const WindowFitRect macbook{0, 0, 1512, 945};
    const auto mac = initialWindowRect({0, 0, 1600, 1000}, macbook);
    check(near(mac.width, 1512 * 0.9) && near(mac.height, 945 * 0.9), "oversized extents shrink to 90%");
    check(near(mac.x, (1512 - mac.width) / 2) && near(mac.y, (945 - mac.height) / 2), "fitted frame is centered");

    // Only the overflowing extent shrinks; a width that fits is kept.
    const auto tall = initialWindowRect({100, 100, 1200, 1000}, {0, 40, 1920, 960});
    check(near(tall.width, 1200) && near(tall.height, 864), "fitting width is preserved");
    check(near(tall.y, 40 + 48), "centered inside an offset work area");

    // A frame that already fits is returned unchanged, position included.
    const auto fits = initialWindowRect({10, 20, 1600, 1000}, {0, 0, 2560, 1400});
    check(near(fits.x, 10) && near(fits.y, 20) && near(fits.width, 1600) && near(fits.height, 1000),
          "fitting frame is not moved");
    check(!windowExceedsArea({0, 0, 1600, 1000}, {0, 0, 0, 0}), "empty work area never reports overflow");

    // Restored placements are clamped to the full work area and slid inside it.
    const auto restored = fitWindowRect({-300, 900, 3000, 400}, {0, 0, 1920, 1040});
    check(near(restored.x, 0) && near(restored.width, 1920), "restored width clamps to the work area");
    check(near(restored.y, 640) && near(restored.height, 400), "restored origin slides back inside");
    const auto secondary = fitWindowRect({-1500, 100, 800, 600}, {-1440, 0, 1440, 900});
    check(near(secondary.x, -1440) && near(secondary.y, 100), "negative-origin monitors are supported");

    return failures == 0 ? 0 : 1;
}
