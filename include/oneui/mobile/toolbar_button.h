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
    void setTitle(std::wstring title){title_=std::move(title);setAccessibleName(title_);invalidate();}
    void setIconOnly(bool value) { iconOnly_=value; invalidate(); }
    bool isFocusable() const override { return !disabled(); }
    AccessibilityInfo accessibilityInfo() const override {
        auto info = Widget::accessibilityInfo();
        info.state.selected = selected_;
        return info;
    }
    void paint(Canvas &canvas) override {
        const auto r = frame();
        auto ink = selected_ ? palette_.tint : palette_.secondary;
        if(disabled())ink.a=110;
        canvas.fillRect(r, pressed_ || hovered_ || selected_ ? palette_.pressed : palette_.surface, 8);
        if(focusVisible())canvas.strokeRect(r,palette_.tint,8,1.5f);
        const float side = 22 * scale_;
        const float line = 17 * scale_;
        const float top = r.y + (r.height - (iconOnly_ ? side : side + 4 * scale_ + line)) / 2;
        const Rect iconRect{r.x + (r.width - side) / 2, top, side, side};
        if (painter_)
            painter_(canvas, iconRect, ink);
        else
            paintIcon(canvas, icon_, iconRect, ink);
        if(!iconOnly_)canvas.drawTextStyledEllipsized(title_, {r.x, top + side + 4 * scale_, r.width, line},
                                        ink, 11 * scale_, TextAlign::Center, 500);
    }
    bool onMouseMove(const MouseEvent &event) override {
        const bool hovered=!disabled()&&frame().contains(event.position);
        if(hovered_!=hovered){hovered_=hovered;invalidate();}
        return hovered;
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
    bool hasInteractionState() const override { return pressed_ || hovered_; }
    void resetInteractionState() override {
        pressed_ = false;
        hovered_ = false;
        invalidate();
    }

  private:
    std::wstring title_;
    IconSymbol icon_;
    bool selected_, pressed_ = false, hovered_ = false, iconOnly_ = false;
    ControlPalette palette_;
    std::function<void()> action_;
    GlyphPainter painter_;
    float scale_;
};
} // namespace oneui::mobile
