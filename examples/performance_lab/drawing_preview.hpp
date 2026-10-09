#pragma once
#include <algorithm>
#include <oneui/widget.h>

namespace lab {
// A static native painting sample. Parameter changes invalidate once; no timer
// or animation callback runs when this preview is idle or detached.
class DrawingPreview final : public oneui::Widget {
    float curve_ = .5f, opacity_ = 1.f;
    bool radial_ = false, clipped_ = true, dark_ = true;

  public:
    void setCurve(float value) {
        curve_ = std::clamp(value, 0.f, 1.f);
        invalidate();
    }
    void setOpacity(float value) {
        opacity_ = std::clamp(value, 0.f, 1.f);
        invalidate();
    }
    void setRadial(bool value) {
        radial_ = value;
        invalidate();
    }
    void setClipped(bool value) {
        clipped_ = value;
        invalidate();
    }
    void setDark(bool value) {
        dark_ = value;
        invalidate();
    }
    void paint(oneui::Canvas &canvas) override {
        using namespace oneui;
        const auto bounds = frame().inset(Insets{16});
        if (bounds.width <= 0 || bounds.height <= 0)
            return;
        const Color ink = dark_ ? Color{229, 236, 232} : Color{28, 43, 35};
        canvas.save();
        canvas.clipRect(frame());
        canvas.fillRect(frame(), dark_ ? Color{16, 21, 23} : Color{245, 247, 244}, 6);
        canvas.strokeRect(bounds, dark_ ? Color{82, 100, 93} : Color{186, 200, 191});
        canvas.saveOpacity(bounds, opacity_);
        if (clipped_)
            canvas.clipRect(bounds);
        Gradient gradient;
        gradient.stops = {{{40, 97, 61}, 0}, {{97, 201, 187}, .55f}, {{196, 237, 135}, 1}};
        gradient.angleDegrees = 35;
        gradient.radial = radial_;
        gradient.center = {.35f, .45f};
        canvas.fillGradient({bounds.x - 12, bounds.y + 12, bounds.width + 24, 74}, gradient, 12);
        CanvasPath path;
        const float baseline = bounds.y + bounds.height * .73f;
        path.moveTo({bounds.x, baseline});
        path.cubicTo({bounds.x + bounds.width * .32f, baseline - 100 * curve_},
                     {bounds.x + bounds.width * .68f, baseline + 70 * curve_},
                     {(bounds.x + bounds.width), baseline - 18});
        canvas.strokePath(path, ink, 3);
        const Rect circle{bounds.x + bounds.width * .5f - 16, baseline - 16, 32, 32};
        canvas.fillEllipse(circle, {97, 201, 187});
        canvas.strokeEllipse(circle, ink, 2);
        canvas.restore();
        canvas.drawText(L"渐变 · 裁剪 · 曲线 · 透明度",
                        {bounds.x, (bounds.y + bounds.height) - 22, bounds.width, 22}, ink, 13,
                        TextAlign::Left);
        canvas.restore();
    }
};
} // namespace lab
