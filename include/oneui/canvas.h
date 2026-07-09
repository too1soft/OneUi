#pragma once

#include "oneui/color.h"
#include "oneui/export.h"
#include "oneui/geometry.h"

#include <cstdint>
#include <optional>
#include <string>

namespace oneui {

enum class TextAlign {
    Left,
    Center,
    Right
};

struct BoxShadow {
    Color color{0, 0, 0, 0};
    Point offset{};
    float blurRadius = 0.0f;
    float spreadRadius = 0.0f;
};

enum class CanvasPixelFormat {
    Bgra8888,
    Rgba8888
};

class ONEUI_API Canvas {
public:
    virtual ~Canvas() = default;

    virtual void save() = 0;
    virtual void restore() = 0;
    virtual void clipRect(Rect rect) = 0;
    virtual std::optional<Rect> clipBounds() const {
        return std::nullopt;
    }
    virtual void clear(Color color) = 0;
    virtual void fillRect(Rect rect, Color color, float radius = 0.0f) = 0;
    virtual void fillLinearGradient(Rect rect, Color start, Color end, float angleDegrees, float radius = 0.0f) {
        (void)end;
        (void)angleDegrees;
        fillRect(rect, start, radius);
    }
    // 径向渐变：centerNorm 为相对 rect 的归一化圆心（0-1），radiusNorm 为相对 max(宽,高) 的半径比例。
    // 默认降级为中心色纯色填充（与线性渐变的降级策略一致）。
    virtual void fillRadialGradient(Rect rect, Color center, Color edge, Point centerNorm, float radiusNorm, float radius = 0.0f) {
        (void)edge;
        (void)centerNorm;
        (void)radiusNorm;
        fillRect(rect, center, radius);
    }
    virtual void strokeRect(Rect rect, Color color, float radius = 0.0f, float width = 1.0f) = 0;
    virtual void fillEllipse(Rect rect, Color color) = 0;
    virtual void strokeEllipse(Rect rect, Color color, float width = 1.0f) = 0;
    virtual void drawLine(Point from, Point to, Color color, float width = 1.0f) = 0;
    virtual void drawText(const std::wstring& text, Rect rect, Color color, float size, TextAlign align = TextAlign::Center) = 0;
    virtual void drawTextStyled(const std::wstring& text, Rect rect, Color color, float size, TextAlign align = TextAlign::Center, int weight = 400) {
        (void)weight;
        drawText(text, rect, color, size, align);
    }
    virtual float measureTextWidth(const std::wstring& text, float size, int weight = 400) const {
        (void)weight;
        return static_cast<float>(text.size()) * size * 0.5f;
    }

    // Draws an owned or caller-retained pixel buffer during this paint pass.
    virtual void drawPixels(Rect rect, const std::uint8_t* pixels, int width, int height, int stride, CanvasPixelFormat format) {
        (void)rect;
        (void)pixels;
        (void)width;
        (void)height;
        (void)stride;
        (void)format;
    }

    // Backends without native shadow support may keep the conservative no-op.
    virtual void drawBoxShadow(Rect rect, const BoxShadow& shadow, float radius = 0.0f) {
        (void)rect;
        (void)shadow;
        (void)radius;
    }
};

} // namespace oneui
