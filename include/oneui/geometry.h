#pragma once

namespace oneui {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct Size {
    float width = 0.0f;
    float height = 0.0f;
};

struct Insets {
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;

    constexpr Insets() = default;
    constexpr explicit Insets(float all) : top(all), right(all), bottom(all), left(all) {}
    constexpr Insets(float vertical, float horizontal)
        : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
    constexpr Insets(float topValue, float rightValue, float bottomValue, float leftValue)
        : top(topValue), right(rightValue), bottom(bottomValue), left(leftValue) {}

    constexpr float horizontal() const {
        return left + right;
    }

    constexpr float vertical() const {
        return top + bottom;
    }
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool contains(Point point) const {
        return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height;
    }

    Rect inset(Insets insets) const {
        return Rect{x + insets.left, y + insets.top, width - insets.horizontal(), height - insets.vertical()};
    }
};

} // namespace oneui
