#include "include/core/SkMilestone.h"

#if defined(_M_X64) && SK_MILESTONE == 150
#include "platform/win32/skia_raster_sse2.h"
#include "src/core/SkBlitMask.h"
#include "src/core/SkBlitRow.h"

#include <iostream>
#include <random>
#include <vector>

int main() {
    const auto referenceRow = SkOpts::blit_row_color32;
    const auto referenceMask = SkOpts::blit_mask_d32_a8;
    oneui::win32::installSkiaRasterSse2();
    oneui::win32::installSkiaRasterSse2();
    if (SkOpts::blit_row_color32 == referenceRow || SkOpts::blit_mask_d32_a8 == referenceMask) {
        std::cerr << "SSE2 raster functions were not installed\n";
        return 1;
    }

    constexpr int dstOffset = 1;
    constexpr int maskOffset = 3;
    std::mt19937 random(0x20261122);
    for (int width = 1; width <= 33; ++width) {
        const int height = width % 11 + 3;
        const int dstStride = width + 3;
        const int maskStride = width + 7;
        std::vector<SkPMColor> initial(dstOffset + height * dstStride);
        std::vector<SkAlpha> coverage(maskOffset + height * maskStride);
        for (auto& pixel : initial) pixel = SkPreMultiplyColor(static_cast<SkColor>(random()));
        for (auto& alpha : coverage) alpha = static_cast<SkAlpha>(random());
        coverage[maskOffset] = 0;
        coverage[maskOffset + maskStride] = 255;
        for (int alpha = 0; alpha <= 255; ++alpha) {
            const auto ink = SkColorSetA(static_cast<SkColor>(random()), alpha);
            auto expected = initial;
            auto actual = initial;
            if (alpha > 0 && alpha < 255) {
                const auto premul = SkPreMultiplyColor(ink);
                referenceRow(expected.data() + dstOffset, width, premul);
                SkOpts::blit_row_color32(actual.data() + dstOffset, width, premul);
                if (expected != actual) {
                    std::cerr << "Color row mismatch for width " << width << " at alpha " << alpha << '\n';
                    return 2;
                }
            }
            for (int stride : {0, maskStride}) {
                expected = initial;
                actual = initial;
                referenceMask(expected.data() + dstOffset, dstStride * sizeof(SkPMColor), coverage.data() + maskOffset, stride,
                    ink, width, height);
                SkOpts::blit_mask_d32_a8(actual.data() + dstOffset, dstStride * sizeof(SkPMColor), coverage.data() + maskOffset, stride,
                    ink, width, height);
                if (expected != actual) {
                    std::cerr << "Alpha mask mismatch for width " << width << " at alpha " << alpha
                              << " with mask stride " << stride << '\n';
                    return 3;
                }
            }
        }
    }
    return 0;
}
#else
int main() { return 77; }
#endif
