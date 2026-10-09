#pragma once
#include "plots.hpp"
#include <oneui/layout/panel.h>

// App layout uses public View/Panel APIs; input and scrolling remain native
// controls.
class LayoutView : public View {
  public:
    std::function<void(Rect)> arrange;

  protected:
    void layoutChildren() override {
        if (arrange)
            arrange(frame());
    }
};
class Rule final : public Widget {
    Color ink;

  public:
    explicit Rule(Color c = LINE) : ink(c) {}
    void paint(Canvas &c) override { c.fillRect(frame(), ink); }
};
using L = std::shared_ptr<Label>;
using B = std::shared_ptr<Button>;
inline L label(View &parent, std::wstring text, float size = 12, Color ink = MUTED,
               int weight = 400) {
    auto p = std::make_shared<Label>(std::move(text));
    p->setFontSize(size);
    p->setColor(ink);
    p->setFontWeight(weight);
    parent.add(p);
    return p;
}
inline std::wstring number(double value, int precision = 1) {
    std::wostringstream o;
    o << std::fixed << std::setprecision(precision) << value;
    return o.str();
}
inline ButtonStyleOverride buttonStyle(bool active = false, bool nav = false) {
    ButtonStateStyleOverride base;
    base.background =
        active ? (nav ? color(0x2a3b34) : ACCENT) : (nav ? color(0x131a1d) : color(0x222d31));
    base.foreground = active ? (nav ? ACCENT : BG) : TEXT;
    base.borderWidth = 0;
    base.radius = 5;
    base.fontSize = 13;
    base.fontWeight = 400;
    base.shadows = std::vector<ControlShadowStyle>{};
    ButtonStyleOverride s;
    s.normal = base;
    auto hover = base;
    hover.background = active ? color(0xd3f5a7) : color(0x304039);
    if (active)
        hover.foreground = BG;
    s.hovered = hover;
    auto down = base;
    down.background = color(0x66804e);
    down.foreground = TEXT;
    s.pressed = down;
    auto disabled = base;
    disabled.foreground = color(0x75847e);
    disabled.background = color(0x222a27);
    s.disabled = disabled;
    return s;
}
inline B button(View &parent, std::wstring text, std::function<void()> action) {
    auto b = std::make_shared<Button>(std::move(text));
    b->setStyleOverride(buttonStyle());
    b->setOnClick(std::move(action));
    parent.add(b);
    return b;
}
inline std::shared_ptr<Panel> panel(View &parent, const std::shared_ptr<Widget> &content,
                                    Color bg = PANEL) {
    auto p = std::make_shared<Panel>();
    p->setBackground(bg);
    p->setBorderWidth(0);
    p->setRadius(8);
    p->setPadding(Insets{0});
    p->setContent(content);
    parent.add(p);
    return p;
}
