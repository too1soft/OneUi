#pragma once
#include "oneui/controls/image_view.h"
#include "oneui/mobile/presentation.h"
#include <cmath>

namespace oneui::mobile {
// Presentation only: the user moves a piece; the consumer verifies the released
// source-image x coordinate. No recognition, answer inference or HTTP transport.
class PuzzleSlider final : public Widget {
  public:
    explicit PuzzleSlider(ControlPalette palette, float scale = 1) : palette_(palette), scale_(scale) {
        setDisplayWidth(320);
        setAccessibleRole(AccessibilityRole::Slider);
        setAccessibleName(L"滑块验证");
        setAccessibleDescription(L"拖动滑块，将拼图移到缺口后松开。也可用方向键调整，回车提交。");
    }
    bool setChallenge(const std::string &background, const std::string &piece, int width, int height,
                      int side, int y) {
        if (width < 64 || width > 1024 || height < 32 || height > 512 || side < 8 ||
            side > std::min(width, height) || y < 0 || y + side > height)
            return false;
        if (!background_.setPngDataUri(background) || !piece_.setPngDataUri(piece) ||
            background_.imageWidth() != width || background_.imageHeight() != height ||
            piece_.imageWidth() != side || piece_.imageHeight() != side)
            return false;
        width_ = width;
        height_ = height;
        side_ = side;
        y_ = y;
        setDisplayWidth(displayWidth_);
        position_ = 0;
        invalidate();
        return true;
    }
    void setDisplayWidth(float width) {
        displayWidth_ = std::max(1.f, width);
        setPreferredSize({0, measure({displayWidth_, 0}).height});
    }
    void setPalette(ControlPalette palette, float scale = 1) {
        palette_ = palette;
        scale_ = scale;
        setDisplayWidth(displayWidth_);
        invalidate();
    }
    void setOnReleased(std::function<void(int)> callback) { released_ = std::move(callback); }
    bool isFocusable() const override { return !disabled() && width_ > 0; }
    Size measure(Size available) const override {
        const float width = available.width > 0 ? available.width : 320;
        return {width, width * float(height_) / std::max(1, width_) + 20 * scale_ + targetHeight()};
    }
    AccessibilityInfo accessibilityInfo() const override {
        auto info = Widget::accessibilityInfo();
        info.value = std::to_wstring(sourceX());
        return info;
    }
    void paint(Canvas &canvas) override {
        if (!width_)
            return;
        const auto r = frame();
        const float ratio = r.width / width_;
        background_.setFrame({r.x, r.y, r.width, height_ * ratio});
        background_.setCornerRadius(12);
        background_.paint(canvas);
        piece_.setFrame({r.x + sourceX() * ratio, r.y + y_ * ratio, side_ * ratio, side_ * ratio});
        piece_.paint(canvas);
        const auto track = trackRect();
        canvas.fillRect(track, palette_.pressed, 12);
        canvas.drawTextStyledEllipsized(disabled()  ? L"正在验证…"
                                        : dragging_ ? L"松开验证"
                                                    : L"拖动滑块完成拼图",
                                        track, palette_.secondary, 14 * scale_, TextAlign::Center);
        const auto thumb = thumbRect();
        canvas.fillRect(thumb, disabled() ? palette_.separator : palette_.surface, 11);
        paintIcon(canvas, IconSymbol::ChevronRight,
                  {thumb.x + (thumb.width - 22 * scale_) / 2, thumb.y + (thumb.height - 22 * scale_) / 2,
                   22 * scale_, 22 * scale_},
                  palette_.tint);
    }
    bool onMouseDown(const MouseEvent &e) override {
        if (!isFocusable() || e.button != MouseButton::Left || !thumbRect().contains(e.position))
            return false;
        dragging_ = true;
        offset_ = e.position.x - thumbRect().x;
        invalidate();
        return true;
    }
    bool onMouseMove(const MouseEvent &e) override {
        if (!dragging_)
            return false;
        const auto track = trackRect();
        const float distance = std::max(1.f, track.width - targetHeight());
        position_ = std::clamp((e.position.x - offset_ - track.x) / distance, 0.f, 1.f);
        invalidate();
        return true;
    }
    bool onMouseUp(const MouseEvent &) override {
        if (!dragging_)
            return false;
        dragging_ = false;
        invalidate();
        if (released_)
            released_(sourceX());
        return true;
    }
    bool onKeyDown(const KeyEvent &e) override {
        if (!isFocusable() || !e.pressed)
            return false;
        if (e.key == Key::Left || e.key == Key::Right) {
            const float step = (e.key == Key::Left ? -1.f : 1.f) / std::max(1, width_ - side_);
            position_ = std::clamp(position_ + step, 0.f, 1.f);
            invalidate();
            return true;
        }
        if (e.key == Key::Enter || e.key == Key::Space) {
            if (released_)
                released_(sourceX());
            return true;
        }
        return false;
    }

  protected:
    bool hasInteractionState() const override { return dragging_; }
    void resetInteractionState() override { dragging_ = false; }

  private:
    float targetHeight() const { return std::max(48.f, 48 * scale_); }
    int sourceX() const {
        return std::clamp(int(std::lround(position_ * (width_ - side_))), 0, std::max(0, width_ - side_));
    }
    Rect trackRect() const {
        const auto r = frame();
        return {r.x, r.y + r.width * float(height_) / std::max(1, width_) + 12 * scale_, r.width,
                targetHeight() + 4 * scale_};
    }
    Rect thumbRect() const {
        const auto r = trackRect();
        return {r.x + position_ * std::max(0.f, r.width - targetHeight()), r.y + 2 * scale_, targetHeight(),
                targetHeight()};
    }
    ControlPalette palette_;
    float scale_ = 1, position_ = 0, offset_ = 0, displayWidth_ = 320;
    bool dragging_ = false;
    int width_ = 0, height_ = 0, side_ = 0, y_ = 0;
    ImageView background_, piece_;
    std::function<void(int)> released_;
};
} // namespace oneui::mobile
