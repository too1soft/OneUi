#pragma once
#include "oneui/mobile/adorned_field.h"
namespace oneui::mobile {
class SearchField final : public View {
  public:
    SearchField(std::shared_ptr<TextField> editor, ControlPalette palette, float scale)
        : editor_(std::move(editor)), palette_(palette) {
        add(editor_); editor_->setFontSize(16 * scale);
        TextFieldStateStyleOverride normal;
        normal.background = palette.surface; normal.foreground = palette.ink;
        normal.placeholderForeground = palette.secondary; normal.radius = 12;
        normal.borderWidth = 1; normal.border = Color{0, 0, 0, 0};
        normal.padding = Insets{0, 16, 0, 52}; normal.caretColor = palette.tint;
        auto focus = normal; focus.border = palette.tint;
        TextFieldStyleOverride style; style.normal = normal; style.focusVisible = focus;
        editor_->setStyleOverride(style);
        setPreferredSize({0, std::max(56.f, 56 * scale)});
    }
    void paint(Canvas &canvas) override {
        View::paint(canvas);
        const auto r = frame(); const float side = 24;
        paintIcon(canvas, IconSymbol::OutlineSearch, {r.x + 16, r.y + (r.height - side) / 2, side, side}, palette_.secondary);
    }
  protected:
    void layoutChildren() override { editor_->setFrame(frame()); }
  private:
    std::shared_ptr<TextField> editor_;
    ControlPalette palette_;
};
}
