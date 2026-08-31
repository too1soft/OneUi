#pragma once

#include "oneui/color.h"
#include "oneui/export.h"
#include "oneui/geometry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oneui {

enum class TextAlign {
    Left,
    Center,
    Right
};

/// Selects a stable text family for controls with layout-sensitive glyphs.
/// Backends that cannot provide the requested family may fall back to their
/// default face without changing the caller's layout contract.
enum class TextFontFamily {
    Default,
    Monospace
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

enum class CanvasPathVerb {
    MoveTo,
    LineTo,
    CubicTo,
    Close
};

struct CanvasPathCommand {
    CanvasPathVerb verb = CanvasPathVerb::MoveTo;
    Point first{};
    Point second{};
    Point third{};
};

/// Backend-neutral vector path used by charts and other data visualizations.
/// Page code never depends on a renderer-specific path type.
struct CanvasPath {
    std::vector<CanvasPathCommand> commands;

    void moveTo(Point point) {
        commands.push_back(CanvasPathCommand{CanvasPathVerb::MoveTo, point, {}, {}});
    }
    void lineTo(Point point) {
        commands.push_back(CanvasPathCommand{CanvasPathVerb::LineTo, point, {}, {}});
    }
    void cubicTo(Point control1, Point control2, Point end) {
        commands.push_back(CanvasPathCommand{CanvasPathVerb::CubicTo, control1, control2, end});
    }
    void close() {
        commands.push_back(CanvasPathCommand{CanvasPathVerb::Close, {}, {}, {}});
    }
    bool empty() const {
        return commands.empty();
    }
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
    /// Returns the stable logical client-space viewport for this paint pass.
    /// Unlike clipBounds(), this is not narrowed by dirty-region or container
    /// clipping and is therefore suitable for edge-aware popup placement.
    virtual std::optional<Rect> viewportBounds() const {
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
    virtual void strokePath(const CanvasPath& path, Color color, float width = 1.0f, bool rounded = true) {
        (void)rounded;
        Point current{};
        Point start{};
        bool hasCurrent = false;
        for (const auto& command : path.commands) {
            switch (command.verb) {
            case CanvasPathVerb::MoveTo:
                current = command.first;
                start = current;
                hasCurrent = true;
                break;
            case CanvasPathVerb::LineTo:
                if (hasCurrent) {
                    drawLine(current, command.first, color, width);
                }
                current = command.first;
                hasCurrent = true;
                break;
            case CanvasPathVerb::CubicTo: {
                if (!hasCurrent) {
                    current = command.third;
                    hasCurrent = true;
                    break;
                }
                const Point from = current;
                Point previous = from;
                for (int step = 1; step <= 16; ++step) {
                    const float t = static_cast<float>(step) / 16.0f;
                    const float u = 1.0f - t;
                    const Point next{
                        u * u * u * from.x + 3.0f * u * u * t * command.first.x +
                            3.0f * u * t * t * command.second.x + t * t * t * command.third.x,
                        u * u * u * from.y + 3.0f * u * u * t * command.first.y +
                            3.0f * u * t * t * command.second.y + t * t * t * command.third.y};
                    drawLine(previous, next, color, width);
                    previous = next;
                }
                current = command.third;
                break;
            }
            case CanvasPathVerb::Close:
                if (hasCurrent) {
                    drawLine(current, start, color, width);
                    current = start;
                }
                break;
            }
        }
    }
    virtual void fillPathLinearGradient(
        const CanvasPath& path,
        Rect bounds,
        Color start,
        Color end,
        float angleDegrees = 90.0f) {
        (void)path;
        (void)bounds;
        (void)start;
        (void)end;
        (void)angleDegrees;
    }
    virtual void drawText(const std::wstring& text, Rect rect, Color color, float size, TextAlign align = TextAlign::Center) = 0;
    virtual void drawTextStyled(const std::wstring& text, Rect rect, Color color, float size, TextAlign align = TextAlign::Center, int weight = 400) {
        (void)weight;
        drawText(text, rect, color, size, align);
    }
    virtual void drawTextStyledWithFont(
        const std::wstring& text,
        Rect rect,
        Color color,
        float size,
        TextAlign align,
        TextFontFamily family,
        int weight = 400) {
        (void)family;
        drawTextStyled(text, rect, color, size, align, weight);
    }
    virtual void drawTextStyledWithNamedFont(
        const std::wstring& text,
        Rect rect,
        Color color,
        float size,
        TextAlign align,
        const std::wstring& familyName,
        TextFontFamily fallbackFamily,
        int weight = 400) {
        (void)familyName;
        drawTextStyledWithFont(text, rect, color, size, align, fallbackFamily, weight);
    }
    virtual float measureTextWidth(const std::wstring& text, float size, int weight = 400) const {
        (void)weight;
        return static_cast<float>(text.size()) * size * 0.5f;
    }
    /// Measures every UTF-16 caret boundary in one linear pass. Backends may
    /// override this to use native glyph advances; the default deliberately
    /// measures one Unicode scalar at a time so callers never build and
    /// re-measure successively longer prefixes.
    virtual std::vector<float> measureTextPrefixWidths(
        const std::wstring& text,
        float size,
        int weight = 400) const {
        std::vector<float> widths(text.size() + 1, 0.0f);
        float advance = 0.0f;
        for (std::size_t index = 0; index < text.size();) {
            std::size_t length = 1;
            if constexpr (sizeof(wchar_t) == 2) {
                const auto current = static_cast<unsigned int>(text[index]);
                if (current >= 0xD800U && current <= 0xDBFFU && index + 1 < text.size()) {
                    const auto next = static_cast<unsigned int>(text[index + 1]);
                    if (next >= 0xDC00U && next <= 0xDFFFU) {
                        length = 2;
                    }
                }
            }
            advance += measureTextWidth(text.substr(index, length), size, weight);
            for (std::size_t offset = 1; offset <= length; ++offset) {
                widths[index + offset] = offset == length ? advance : widths[index];
            }
            index += length;
        }
        return widths;
    }
    virtual float measureTextWidthWithFont(
        const std::wstring& text,
        float size,
        TextFontFamily family,
        int weight = 400) const {
        (void)family;
        return measureTextWidth(text, size, weight);
    }
    virtual float measureTextWidthWithNamedFont(
        const std::wstring& text,
        float size,
        const std::wstring& familyName,
        TextFontFamily fallbackFamily,
        int weight = 400) const {
        (void)familyName;
        return measureTextWidthWithFont(text, size, fallbackFamily, weight);
    }

    /// Returns a single-line rendering string that fits the requested width.
    /// The returned prefix never splits a UTF-16 surrogate pair on Windows.
    std::wstring ellipsizeText(
        const std::wstring& text,
        float maxWidth,
        float size,
        int weight = 400,
        TextFontFamily family = TextFontFamily::Default) const {
        if (text.empty() || maxWidth <= 0.0f) {
            return {};
        }
        if (measureTextWidthWithFont(text, size, family, weight) <= maxWidth) {
            return text;
        }

        const std::wstring ellipsis = L"\u2026";
        if (measureTextWidthWithFont(ellipsis, size, family, weight) > maxWidth) {
            return {};
        }

        std::vector<std::size_t> boundaries;
        boundaries.reserve(text.size() + 1);
        boundaries.push_back(0);
        for (std::size_t index = 0; index < text.size();) {
            if constexpr (sizeof(wchar_t) == 2) {
                const auto current = static_cast<unsigned int>(text[index]);
                if (current >= 0xD800U && current <= 0xDBFFU && index + 1 < text.size()) {
                    const auto next = static_cast<unsigned int>(text[index + 1]);
                    if (next >= 0xDC00U && next <= 0xDFFFU) {
                        index += 2;
                        boundaries.push_back(index);
                        continue;
                    }
                }
            }
            ++index;
            boundaries.push_back(index);
        }

        std::size_t low = 0;
        std::size_t high = boundaries.size() - 1;
        while (low < high) {
            const std::size_t middle = low + (high - low + 1) / 2;
            const std::wstring candidate = text.substr(0, boundaries[middle]) + ellipsis;
            if (measureTextWidthWithFont(candidate, size, family, weight) <= maxWidth) {
                low = middle;
            } else {
                high = middle - 1;
            }
        }
        return text.substr(0, boundaries[low]) + ellipsis;
    }

    /// Draws a single line with a measured ellipsis instead of clipping glyphs.
    void drawTextStyledEllipsized(
        const std::wstring& text,
        Rect rect,
        Color color,
        float size,
        TextAlign align = TextAlign::Center,
        int weight = 400,
        TextFontFamily family = TextFontFamily::Default) {
        const std::wstring fitted = ellipsizeText(text, rect.width, size, weight, family);
        if (!fitted.empty()) {
            drawTextStyledWithFont(fitted, rect, color, size, align, family, weight);
        }
    }

    void drawTextEllipsized(
        const std::wstring& text,
        Rect rect,
        Color color,
        float size,
        TextAlign align = TextAlign::Center) {
        drawTextStyledEllipsized(text, rect, color, size, align);
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
