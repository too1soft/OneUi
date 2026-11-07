#pragma once
#include "oneui/mobile/presentation.h"

namespace oneui::mobile {
class TabButton final : public Widget {
  public:
    TabButton(std::wstring title, IconSymbol icon, bool selected, ControlPalette palette,
              std::function<void()> action, float scale = 1)
        : title_(std::move(title)), icon_(icon), selected_(selected), palette_(palette),
          action_(std::move(action)), scale_(scale) {
        setPreferredSize({0, std::max(64.f, 64 * scale)});
        setAccessibleRole(AccessibilityRole::Tab);
        setAccessibleName(title_);
    }
    bool isFocusable() const override { return !disabled(); }
    AccessibilityInfo accessibilityInfo() const override {
        auto info = Widget::accessibilityInfo();
        info.state.selected = selected_;
        return info;
    }
    void paint(Canvas &canvas) override {
        const auto r = frame();
        const Color ink = selected_ ? palette_.tint : palette_.secondary;
        canvas.fillRect(r, pressed_ || focusVisible() ? palette_.pressed : palette_.surface);
        const float side = 24 * scale_;
        paintIcon(canvas, icon_, {r.x + (r.width - side) / 2, r.y + 9 * scale_, side, side}, ink);
        canvas.drawTextStyledEllipsized(title_, {r.x, r.y + 36 * scale_, r.width, 19 * scale_}, ink,
                                        11 * scale_, TextAlign::Center, 550);
    }
    bool onMouseDown(const MouseEvent &e) override {
        pressed_ = !disabled() && e.button == MouseButton::Left && frame().contains(e.position);
        invalidate();
        return pressed_;
    }
    bool onMouseUp(const MouseEvent &e) override {
        const bool run = pressed_ && !disabled() && frame().contains(e.position);
        pressed_ = false;
        invalidate();
        if (run && action_)
            action_();
        return run;
    }
    bool onKeyDown(const KeyEvent &e) override {
        if (disabled() || !e.pressed || (e.key != Key::Enter && e.key != Key::Space))
            return false;
        if (action_)
            action_();
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
    float scale_;
};

inline std::shared_ptr<Panel> segmentedControl(const std::vector<std::wstring> &titles, int selected,
                                               ControlPalette palette, std::function<void(int)> onSelect,
                                               float scale = 1, bool disabled = false) {
    auto line = std::make_shared<Stack>(StackDirection::Row);
    line->setGap(2 * scale);
    const float targetHeight = std::max(48.f, 48 * scale);
    line->setPreferredSize({0, targetHeight});
    for (size_t i = 0; i < titles.size(); ++i) {
        auto item = actionButton(
            titles[i], [onSelect, i] { onSelect(static_cast<int>(i)); }, palette, ActionKind::Text, scale);
        ButtonStateStyleOverride normal;
        normal.background = static_cast<int>(i) == selected ? palette.surface : Color{0, 0, 0, 0};
        normal.foreground = static_cast<int>(i) == selected ? palette.ink : palette.secondary;
        normal.fontSize = 15 * scale;
        normal.fontWeight = 550;
        normal.radius = 9;
        normal.borderWidth = 0;
        ButtonStyleOverride style;
        style.normal = normal;
        style.hovered = normal;
        auto pressed = normal;
        pressed.background = palette.separator;
        style.pressed = pressed;
        item->setStyleOverride(style);
        item->setDisabled(disabled);
        item->setPreferredSize({0, targetHeight});
        line->add(item);
        StackFlex fill;
        fill.grow = 1;
        fill.basis = 0;
        line->setFlex(item, fill);
    }
    auto panel = std::make_shared<Panel>();
    panel->setBackground(palette.pressed);
    panel->setBorderWidth(0);
    panel->setRadius(12);
    panel->setPadding({2 * scale, 2 * scale});
    panel->setPreferredSize({0, targetHeight + 4 * scale});
    panel->setContent(line);
    return panel;
}

inline std::shared_ptr<Stack> tabBar(const std::vector<std::wstring> &titles,
                                     const std::vector<IconSymbol> &icons, int selected,
                                     ControlPalette palette, std::function<void(int)> onSelect,
                                     float scale = 1) {
    auto bar = std::make_shared<Stack>(StackDirection::Row);
    bar->setGap(0);
    bar->setPreferredSize({0, std::max(64.f, 64 * scale)});
    for (size_t i = 0; i < titles.size(); ++i) {
        auto tab = std::make_shared<TabButton>(
            titles[i], i < icons.size() ? icons[i] : IconSymbol::Device, static_cast<int>(i) == selected,
            palette, [onSelect, i] { onSelect(static_cast<int>(i)); }, scale);
        bar->add(tab);
        StackFlex flex;
        flex.grow = 1;
        flex.basis = 0;
        bar->setFlex(tab, flex);
    }
    return bar;
}
} // namespace oneui::mobile
