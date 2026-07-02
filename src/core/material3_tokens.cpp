#include "oneui/material3_tokens.h"

#include <algorithm>
#include <cmath>

namespace oneui {

namespace {

std::uint8_t mixChannel(std::uint8_t base, std::uint8_t overlay, float opacity) {
    const float clamped = std::clamp(opacity, 0.0f, 1.0f);
    const float value = static_cast<float>(base) * (1.0f - clamped) + static_cast<float>(overlay) * clamped;
    return static_cast<std::uint8_t>(std::clamp(std::round(value), 0.0f, 255.0f));
}

float stateOpacity(MaterialState state) {
    constexpr Material3StateOpacity opacities{};
    switch (state) {
    case MaterialState::Hovered:
        return opacities.hover;
    case MaterialState::Focused:
        return opacities.focus;
    case MaterialState::Pressed:
        return opacities.pressed;
    case MaterialState::Disabled:
        return opacities.disabledContainer;
    case MaterialState::Normal:
    default:
        return 0.0f;
    }
}

} // namespace

Color material3Blend(Color base, Color overlay, float opacity) {
    return Color{
        mixChannel(base.r, overlay.r, opacity),
        mixChannel(base.g, overlay.g, opacity),
        mixChannel(base.b, overlay.b, opacity),
        base.a};
}

Color material3StateLayer(Color base, Color stateColor, MaterialState state) {
    return material3Blend(base, stateColor, stateOpacity(state));
}

StyleShadow material3ElevationShadow(MaterialElevationLevel level) {
    StyleShadow shadow;
    shadow.color = Color{0, 0, 0, 0};
    switch (level) {
    case MaterialElevationLevel::Level0:
        shadow.blurRadius = 0.0f;
        shadow.offset = Point{0.0f, 0.0f};
        shadow.color = Color{0, 0, 0, 0};
        break;
    case MaterialElevationLevel::Level1:
        shadow.blurRadius = 3.0f;
        shadow.offset = Point{0.0f, 1.0f};
        shadow.color = Color{0, 0, 0, 64};
        break;
    case MaterialElevationLevel::Level2:
        shadow.blurRadius = 6.0f;
        shadow.offset = Point{0.0f, 2.0f};
        shadow.color = Color{0, 0, 0, 72};
        break;
    case MaterialElevationLevel::Level3:
        shadow.blurRadius = 10.0f;
        shadow.offset = Point{0.0f, 4.0f};
        shadow.color = Color{0, 0, 0, 82};
        break;
    }
    return shadow;
}

} // namespace oneui
