#pragma once
#include "oneui/mobile/presentation.h"
#include "oneui/view.h"

namespace oneui::mobile {
// Adornments share the editor's surface. The retained editor owns focus,
// composition, selection and its border; auxiliary pointer actions preserve it.
class AdornedField final : public View {
  public:
    AdornedField(std::shared_ptr<TextField> editor, ControlPalette palette, float scale = 1,
                  std::shared_ptr<Widget> trailing = {}, float trailingWidth = 48,
                  std::wstring prefix = {})
        : editor_(std::move(editor)), trailing_(std::move(trailing)), palette_(palette),
          prefix_(std::move(prefix)), scale_(scale), trailingWidth_(trailingWidth) {
        const float minimum = std::max(58.f, 58 * scale_);
        setPreferredSize({0, minimum});
        if (!prefix_.empty()) {
            Label prefixLabel(prefix_); prefixLabel.setFontSize(16 * scale_);
            prefixWidth_ = prefixLabel.measure({INFINITY, INFINITY}).width;
        }
        TextFieldStateStyleOverride normal;
        normal.background = palette_.surface;
        normal.foreground = palette_.ink;
        normal.placeholderForeground = palette_.secondary;
        normal.border = Color{0, 0, 0, 0};
        normal.borderWidth = 1;
        normal.radius = 12;
        normal.padding = Insets{0, 16 + (trailing_ ? trailingWidth_ : 0), 0,
                                16 + (prefix_.empty() ? 0 : prefixWidth_ + 21)};
        normal.caretColor = palette_.tint;
        normal.selectionBackground = Color{189, 213, 255};
        TextFieldStyleOverride style;
        style.normal = normal;
        style.hovered = normal;
        style.readOnly = normal;
        auto focused = normal;
        focused.border = palette_.tint;
        style.focusVisible = focused;
        editor_->setStyleOverride(style);
        editor_->setFontSize(16 * scale_);
        add(editor_);
        if (trailing_) {
            trailing_->setFocusOnPointer(false);
            add(trailing_);
        }
    }
    void paint(Canvas &canvas) override {
        View::paint(canvas);
        if (!prefix_.empty()) {
            const auto r = frame();
            canvas.drawTextStyledEllipsized(prefix_, {r.x + 16, r.y, prefixWidth_, r.height},
                                            palette_.ink, 16 * scale_, TextAlign::Left, 400);
            canvas.fillRect({r.x + 28 + prefixWidth_, r.y + (r.height - 22 * scale_) / 2,
                             1, 22 * scale_}, palette_.separator);
        }
    }
  protected:
    void layoutChildren() override {
        const auto r = frame();
        editor_->setFrame(r);
        if (trailing_) {
            const float width = std::min(trailingWidth_, std::max(0.f, r.width - 32));
            trailing_->setFrame({r.x + r.width - width - 8, r.y, width, r.height});
        }
    }
  private:
    std::shared_ptr<TextField> editor_;
    std::shared_ptr<Widget> trailing_;
    ControlPalette palette_;
    std::wstring prefix_;
    float scale_, trailingWidth_, prefixWidth_ = 0;
};

inline std::shared_ptr<Stack> adornedInput(std::shared_ptr<TextField> editor, std::wstring title,
    ControlPalette palette, float scale = 1, std::shared_ptr<Widget> trailing = {},
    float trailingWidth = 48, std::wstring prefix = {}) {
    auto group = std::make_shared<Stack>();
    group->setGap(8);
    auto label = pageText(std::move(title), 14 * scale, palette.secondary);
    group->add(label);
    auto field = std::make_shared<AdornedField>(std::move(editor), palette, scale,
                                               std::move(trailing), trailingWidth, std::move(prefix));
    group->add(field);
    group->setPreferredSize({0, label->preferredSize().height + 8 + field->preferredSize().height});
    return group;
}
} // namespace oneui::mobile
