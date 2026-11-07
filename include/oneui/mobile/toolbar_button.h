#pragma once
#include "oneui/mobile/presentation.h"

namespace oneui::mobile {
// A compact icon-and-label action with selection and keyboard support.
// Its implementation is header-only and independent of navigation tabs.
class ToolbarButton final : public Widget {
  public:
    using GlyphPainter = std::function<void(Canvas &, Rect, Color)>;
    ToolbarButton(std::wstring title, IconSymbol icon, bool selected, ControlPalette palette,
                  std::function<void()> action, float scale = 1)
        : title_(std::move(title)), icon_(icon), selected_(selected), palette_(palette),
          action_(std::move(action)), scale_(scale) {
        setPreferredSize({0, std::max(64.f, 64 * scale)});
        setAccessibleRole(AccessibilityRole::Button);
        setAccessibleName(title_);
    }
    void setGlyphPainter(GlyphPainter painter) {
        painter_ = std::move(painter);
        invalidate();
    }
    bool isFocusable() const override { return !disabled(); }
    AccessibilityInfo accessibilityInfo() const override {
        auto info = Widget::accessibilityInfo();
        info.state.selected = selected_;
        return info;
    }
    void paint(Canvas &canvas) override {
        const auto r = frame();
        const auto ink = disabled() ? palette_.secondary : selected_ ? palette_.tint : palette_.secondary;
        canvas.fillRect(r, pressed_ || focusVisible() ? palette_.pressed : palette_.surface, 8);
        const float side = 24 * scale_;
        const Rect iconRect{r.x + (r.width - side) / 2, r.y + 9 * scale_, side, side};
        if (painter_)
            painter_(canvas, iconRect, ink);
        else
            paintIcon(canvas, icon_, iconRect, ink);
        canvas.drawTextStyledEllipsized(title_, {r.x, r.y + 36 * scale_, r.width, 19 * scale_},
                                        ink, 11 * scale_, TextAlign::Center, 550);
    }
    bool onMouseDown(const MouseEvent &event) override {
        pressed_ = !disabled() && event.button == MouseButton::Left && frame().contains(event.position);
        invalidate();
        return pressed_;
    }
    bool onMouseUp(const MouseEvent &event) override {
        const bool activate = pressed_ && !disabled() && frame().contains(event.position);
        pressed_ = false;
        invalidate();
        if (activate && action_) action_();
        return activate;
    }
    bool onKeyDown(const KeyEvent &event) override {
        if (disabled() || !event.pressed || (event.key != Key::Enter && event.key != Key::Space))
            return false;
        if (action_) action_();
        return true;
    }

  protected:
    bool hasInteractionState() const override { return pressed_; }
    void resetInteractionState() override {
        pressed_ = false;
        invalidate();
    }

  private:
    std::wstring title_;
    IconSymbol icon_;
    bool selected_, pressed_ = false;
    ControlPalette palette_;
    std::function<void()> action_;
    GlyphPainter painter_;
    float scale_;
};
} // namespace oneui::mobile
