#pragma once
#include <oneui/mobile/presentation.h>

namespace oneui::mobile {
// A native two-line file row. Consumers own navigation, selection and metadata.
class FileRow final : public Widget {
  public:
    FileRow(std::wstring title, std::wstring detail, bool directory, ControlPalette palette,
            float scale, std::function<void()> activate)
        : title_(std::move(title)), detail_(std::move(detail)), directory_(directory),
          palette_(palette), scale_(scale), activate_(std::move(activate)) {
        setAccessibleName(title_);
        setAccessibleDescription(detail_);
        setAccessibleRole(AccessibilityRole::Button);
        setPreferredSize({0, std::max(64.f, 72 * scale_)});
    }
    bool isFocusable() const override { return !disabled(); }
    void paint(Canvas &canvas) override {
        const auto r = frame();
        if (pressed_ || hovered_ || focusVisible()) canvas.fillRect(r, palette_.pressed);
        const float icon = 24 * scale_;
        paintIcon(canvas, directory_ ? IconSymbol::Folder : IconSymbol::File,
            {r.x + 4, r.y + (r.height - icon) / 2, icon, icon}, palette_.secondary);
        const float x = r.x + 40 * scale_, width = std::max(0.f, r.width - 48 * scale_ - (directory_ ? 24 * scale_ : 0));
        canvas.drawTextStyledEllipsized(title_, {x, r.y + 10 * scale_, width, 27 * scale_},
            palette_.ink, 16 * scale_, TextAlign::Left, 500);
        canvas.drawTextStyledEllipsized(detail_, {x, r.y + 38 * scale_, width, 21 * scale_},
            palette_.secondary, 12 * scale_, TextAlign::Left, 400);
        if (directory_) paintIcon(canvas, IconSymbol::ChevronRight,
            {r.x + r.width - 22 * scale_, r.y + (r.height - 18 * scale_) / 2, 18 * scale_, 18 * scale_}, palette_.secondary);
    }
    bool onMouseMove(const MouseEvent &event) override {
        const bool value = frame().contains(event.position);
        if (value != hovered_) { hovered_ = value; invalidate(); }
        return pressed_;
    }
    bool onMouseDown(const MouseEvent &event) override {
        pressed_ = !disabled() && event.button == MouseButton::Left && frame().contains(event.position);
        invalidate(); return pressed_;
    }
    bool onMouseUp(const MouseEvent &event) override {
        const bool run = pressed_ && frame().contains(event.position) && !disabled();
        pressed_ = false; invalidate(); if (run && activate_) activate_(); return run;
    }
    bool onKeyDown(const KeyEvent &event) override {
        if (disabled() || !event.pressed || (event.key != Key::Enter && event.key != Key::Space)) return false;
        if (activate_) activate_(); return true;
    }
  protected:
    bool hasInteractionState() const override { return pressed_ || hovered_; }
    void resetInteractionState() override { pressed_ = hovered_ = false; invalidate(); }
  private:
    std::wstring title_, detail_;
    bool directory_, pressed_ = false, hovered_ = false;
    ControlPalette palette_;
    float scale_;
    std::function<void()> activate_;
};
}
