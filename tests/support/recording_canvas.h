#pragma once

#include "oneui/canvas.h"

#include <optional>
#include <string>
#include <vector>

namespace oneui::test_support {

struct StrokeRectCall {
    Rect rect;
    Color color;
    float radius;
    float width;
};

struct FillRectCall {
    Rect rect;
    Color color;
    float radius;
};

struct FillEllipseCall {
    Rect rect;
    Color color;
};

struct StrokeEllipseCall {
    Rect rect;
    Color color;
    float width;
};

struct DrawTextCall {
    std::wstring text;
    Rect rect;
    Color color;
    float size = 0.0f;
    int weight = 400;
    TextAlign align = TextAlign::Center;
};

struct DrawLineCall {
    Point from;
    Point to;
    Color color;
    float width;
};

struct BoxShadowCall {
    Rect rect;
    BoxShadow shadow;
    float radius;
};

class RecordingCanvas final : public Canvas {
public:
    void save() override {
        ++saves;
    }

    void restore() override {
        ++restores;
    }

    void clipRect(Rect rect) override {
        clips.push_back(rect);
    }

    std::optional<Rect> clipBounds() const override {
        return clipOverride;
    }

    std::optional<Rect> viewportBounds() const override {
        return viewportOverride;
    }

    void clear(Color) override {}

    void fillRect(Rect rect, Color color, float radius = 0.0f) override {
        fillRects.push_back(FillRectCall{rect, color, radius});
    }

    void strokeRect(Rect rect, Color color, float radius, float width = 1.0f) override {
        strokeRects.push_back(StrokeRectCall{rect, color, radius, width});
    }

    void fillEllipse(Rect rect, Color color) override {
        fillEllipses.push_back(FillEllipseCall{rect, color});
    }

    void strokeEllipse(Rect rect, Color color, float width = 1.0f) override {
        strokeEllipses.push_back(StrokeEllipseCall{rect, color, width});
    }

    void drawLine(Point from, Point to, Color color, float width = 1.0f) override {
        lines.push_back(DrawLineCall{from, to, color, width});
    }

    void drawBoxShadow(Rect rect, const BoxShadow& shadow, float radius = 0.0f) override {
        boxShadows.push_back(BoxShadowCall{rect, shadow, radius});
    }

    void drawText(
        const std::wstring& text,
        Rect rect,
        Color color,
        float size,
        TextAlign align = TextAlign::Center) override {
        texts.push_back(DrawTextCall{text, rect, color, size, 400, align});
    }

    void drawTextStyled(
        const std::wstring& text,
        Rect rect,
        Color color,
        float size,
        TextAlign align = TextAlign::Center,
        int weight = 400) override {
        texts.push_back(DrawTextCall{text, rect, color, size, weight, align});
    }

    float measureTextWidth(const std::wstring& text, float size, int weight = 400) const override {
        (void)weight;
        ++measureCalls;
        measuredCodeUnits += text.size();
        const float scale = size / 14.0f;
        float width = 0.0f;
        for (wchar_t character : text) {
            switch (character) {
            case L'W':
            case L'M':
            case L'@':
            case L'#':
                width += 10.0f;
                break;
            case L'i':
            case L'l':
            case L'I':
            case L'!':
            case L'|':
            case L' ':
                width += 4.0f;
                break;
            default:
                width += 7.0f;
                break;
            }
        }
        return width * scale;
    }

    std::vector<FillRectCall> fillRects;
    std::vector<FillEllipseCall> fillEllipses;
    std::vector<StrokeRectCall> strokeRects;
    std::vector<StrokeEllipseCall> strokeEllipses;
    std::vector<DrawTextCall> texts;
    std::vector<DrawLineCall> lines;
    std::vector<BoxShadowCall> boxShadows;
    std::vector<Rect> clips;
    std::optional<Rect> clipOverride;
    std::optional<Rect> viewportOverride;
    int saves = 0;
    int restores = 0;
    mutable std::size_t measureCalls = 0;
    mutable std::size_t measuredCodeUnits = 0;
};

} // namespace oneui::test_support
