#pragma once

#include "include/effects/SkGradient.h"

#include <cstddef>
#include <utility>

// Compatibility surface removed by newer Skia revisions. OneUI only uses the
// Color4f linear and radial factories, so forward those calls to SkShaders.
class SkGradientShader {
public:
    using Interpolation = SkGradient::Interpolation;

    static sk_sp<SkShader> MakeLinear(
        const SkPoint points[2],
        const SkColor4f colors[],
        sk_sp<SkColorSpace> colorSpace,
        const SkScalar positions[],
        int count,
        SkTileMode mode,
        const Interpolation& interpolation,
        const SkMatrix* localMatrix) {
        const SkSpan<const SkColor4f> colorSpan(colors, static_cast<std::size_t>(count));
        const SkSpan<const SkScalar> positionSpan = positions
            ? SkSpan<const SkScalar>(positions, static_cast<std::size_t>(count))
            : SkSpan<const SkScalar>();
        const SkGradient::Colors description(colorSpan, positionSpan, mode, std::move(colorSpace));
        return SkShaders::LinearGradient(points, SkGradient(description, interpolation), localMatrix);
    }

    static sk_sp<SkShader> MakeRadial(
        const SkPoint& center,
        SkScalar radius,
        const SkColor4f colors[],
        sk_sp<SkColorSpace> colorSpace,
        const SkScalar positions[],
        int count,
        SkTileMode mode,
        const Interpolation& interpolation,
        const SkMatrix* localMatrix) {
        const SkSpan<const SkColor4f> colorSpan(colors, static_cast<std::size_t>(count));
        const SkSpan<const SkScalar> positionSpan = positions
            ? SkSpan<const SkScalar>(positions, static_cast<std::size_t>(count))
            : SkSpan<const SkScalar>();
        const SkGradient::Colors description(colorSpan, positionSpan, mode, std::move(colorSpace));
        return SkShaders::RadialGradient(center, radius, SkGradient(description, interpolation), localMatrix);
    }
};
