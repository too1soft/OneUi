#pragma once

#include <algorithm>

namespace oneui {

// Converts a Windows DPI value into OneUI's logical-to-physical scale. Keeping
// this conversion platform-neutral makes the 100/125/150 percent contracts
// deterministic in tests while the Win32 backend consumes the same function.
inline float scaleFromDpiValue(unsigned int dpi) {
    constexpr float kDefaultDpi = 96.0f;
    if (dpi == 0) {
        return 1.0f;
    }
    return std::max(0.25f, static_cast<float>(dpi) / kDefaultDpi);
}

} // namespace oneui
