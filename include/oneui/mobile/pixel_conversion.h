#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace oneui::mobile {
// Limited-range BT.601 NV12 to owned RGBA. Keep codec/platform details outside
// the renderer; the current shared FrameView accepts BGRA/RGBA, not NV12.
inline bool nv12ToRgba(const uint8_t *source, size_t sourceBytes, int width, int height,
                       int sourceStride, uint8_t *target, size_t targetBytes, int targetStride) {
    if (!source || !target || width < 2 || height < 2 || width > 5120 || height > 2880 ||
        width % 2 || height % 2 || sourceStride < width || sourceStride > 5120 ||
        targetStride < width * 4 || targetStride > 5120 * 4 ||
        size_t(sourceStride) * (height + height / 2) > sourceBytes ||
        size_t(targetStride) * height > targetBytes)
        return false;
    auto byte = [](int value) { return uint8_t(std::clamp(value, 0, 255)); };
    const auto chroma = source + size_t(sourceStride) * height;
    for (int y = 0; y < height; ++y) {
        auto out = target + size_t(targetStride) * y;
        const auto luma = source + size_t(sourceStride) * y;
        const auto uv = chroma + size_t(sourceStride) * (y / 2);
        for (int x = 0; x < width; ++x) {
            const int c = std::max(0, int(luma[x]) - 16), u = int(uv[x & ~1]) - 128,
                      v = int(uv[(x & ~1) + 1]) - 128;
            out[x * 4] = byte((298 * c + 409 * v + 128) >> 8);
            out[x * 4 + 1] = byte((298 * c - 100 * u - 208 * v + 128) >> 8);
            out[x * 4 + 2] = byte((298 * c + 516 * u + 128) >> 8);
            out[x * 4 + 3] = 255;
        }
    }
    return true;
}
} // namespace oneui::mobile
