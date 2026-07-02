#pragma once

#include <cstdint>

namespace oneui {

struct Color {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;

    constexpr Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}
};

namespace colors {
constexpr Color Surface{247, 247, 248};
constexpr Color Panel{255, 255, 255};
constexpr Color Text{32, 33, 36};
constexpr Color TextMuted{102, 106, 112};
constexpr Color Border{216, 219, 224};
constexpr Color Primary{37, 99, 235};
constexpr Color PrimaryHover{29, 78, 216};
constexpr Color PrimaryPressed{30, 64, 175};
constexpr Color White{255, 255, 255};
} // namespace colors

} // namespace oneui
