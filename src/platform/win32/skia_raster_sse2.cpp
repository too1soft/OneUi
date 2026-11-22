#include "skia_raster_sse2.h"

#include "include/core/SkMilestone.h"

// These are private entry points and blend rules of the pinned Skia milestone.
#if defined(_M_X64) && SK_MILESTONE == 150
#include "include/core/SkGraphics.h"
#include "src/core/SkBlitMask.h"
#include "src/core/SkBlitRow.h"

#include <cstring>
#include <emmintrin.h>
#endif

namespace oneui::win32 {
#if defined(_M_X64) && SK_MILESTONE == 150
namespace {
decltype(SkOpts::blit_row_color32) originalRow;
decltype(SkOpts::blit_mask_d32_a8) originalMask;

// Four packed pixels. Skia's approx_scale is (x * (y + 1)) >> 8.
__m128i approxScale(__m128i x, __m128i y) {
    const auto zero = _mm_setzero_si128();
    const auto one = _mm_set1_epi16(1);
    const auto lo = _mm_mullo_epi16(_mm_unpacklo_epi8(x, zero),
        _mm_add_epi16(_mm_unpacklo_epi8(y, zero), one));
    const auto hi = _mm_mullo_epi16(_mm_unpackhi_epi8(x, zero),
        _mm_add_epi16(_mm_unpackhi_epi8(y, zero), one));
    return _mm_packus_epi16(_mm_srli_epi16(lo, 8), _mm_srli_epi16(hi, 8));
}

void blendColorRow(SkPMColor* dst, int pixels, SkPMColor color) {
    const auto source = _mm_set1_epi32(static_cast<int>(color));
    const auto inverseAlpha = _mm_set1_epi8(static_cast<char>(255 - (color >> 24)));
    for (; pixels >= 4; pixels -= 4, dst += 4) {
        const auto target = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dst));
        const auto result = _mm_add_epi8(source, approxScale(target, inverseAlpha));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst), result);
    }
    if (pixels) originalRow(dst, pixels, color);
}

void blendAlphaMask(SkPMColor* dst, std::size_t dstBytes, const SkAlpha* mask,
        std::size_t maskBytes, SkColor color, int width, int height) {
    const auto source = _mm_set1_epi32(static_cast<int>(SkPreMultiplyColor(color)));
    const auto byteMask = _mm_set1_epi8(-1);
    const auto dstStride = dstBytes / sizeof(*dst);
    for (int row = 0; row < height; ++row) {
        auto* dstRow = dst + row * dstStride;
        const auto* maskRow = mask + row * maskBytes;
        int x = 0;
        for (; width - x >= 4; x += 4) {
            int maskBits;
            std::memcpy(&maskBits, maskRow + x, sizeof(maskBits));
            const auto mask4 = _mm_cvtsi32_si128(maskBits);
            const auto pairs = _mm_unpacklo_epi8(mask4, mask4);
            const auto coverage = _mm_unpacklo_epi16(pairs, pairs);
            const auto left = approxScale(source, coverage);
            // Broadcast each scaled source alpha to the four channels of its pixel.
            auto alpha = _mm_srli_epi32(left, 24);
            alpha = _mm_or_si128(alpha, _mm_slli_epi32(alpha, 8));
            alpha = _mm_or_si128(alpha, _mm_slli_epi32(alpha, 16));
            const auto target = _mm_loadu_si128(reinterpret_cast<const __m128i*>(dstRow + x));
            const auto right = approxScale(target, _mm_xor_si128(alpha, byteMask));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dstRow + x), _mm_add_epi8(left, right));
        }
        if (x < width) originalMask(dstRow + x, dstBytes, maskRow + x, maskBytes, color, width - x, 1);
    }
}
} // namespace
#endif

void installSkiaRasterSse2() {
#if defined(_M_X64) && SK_MILESTONE == 150
    // MSVC lowers the pinned Skia blitters to scalar operations, including
    // their newer-CPU variants. Windows x64 guarantees SSE2 support.
    [[maybe_unused]] static const bool installed = [] {
        SkGraphics::Init();
        originalRow = SkOpts::blit_row_color32;
        originalMask = SkOpts::blit_mask_d32_a8;
        SkOpts::blit_row_color32 = blendColorRow;
        SkOpts::blit_mask_d32_a8 = blendAlphaMask;
        return true;
    }();
#endif
}
} // namespace oneui::win32
