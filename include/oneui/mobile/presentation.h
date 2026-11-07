#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <oneui/controls/button.h>
#include <oneui/controls/checkbox.h>
#include <oneui/controls/label.h>
#include <oneui/controls/text_field.h>
#include <oneui/icon.h>
#include <oneui/layout/panel.h>
#include <oneui/layout/stack.h>

namespace oneui::mobile {
// Additive, header-only presentation primitives. They use the public Widget /
// Canvas ABI and can be shared by all native hosts without product policies.
struct ControlPalette {
    Color page, surface, ink, secondary, tint, separator, pressed, danger;
};
inline ControlPalette controlPalette(bool dark) {
    if (dark)
        return {{20, 20, 22},    {36, 36, 38}, {245, 245, 247}, {176, 176, 184},
                {121, 168, 255}, {59, 59, 64}, {53, 53, 58},    {255, 139, 142}};
    return {{245, 245, 247}, {255, 255, 255}, {29, 29, 31},    {110, 110, 115},
            {23, 96, 237},   {229, 229, 232}, {233, 233, 238}, {193, 46, 50}};
}

inline std::shared_ptr<Label> pageText(std::wstring value, float size, Color color, int weight = 400,
                                       float availableWidth = std::numeric_limits<float>::infinity()) {
    auto label = std::make_shared<Label>(value);
    label->setAccessibleName(std::move(value));
    label->setFontSize(size);
    label->setColor(color);
    label->setFontWeight(weight);
    label->setLineHeight(size * 1.5f);
    label->setTextWrapping(true);
    // Legacy Stack uses preferredSize rather than measure. Reserve measured
    // line boxes explicitly; callers update this width with their viewport.
    label->setPreferredSize({0, std::ceil(label->measure({availableWidth, INFINITY}).height)});
    return label;
}

// Short semantic text blocks keep long native documents readable to platform
// accessibility adapters whose individual labels are deliberately bounded.
inline std::shared_ptr<Stack> readingText(const std::wstring &value, const ControlPalette &palette,
                                          float size, float availableWidth) {
    auto content = std::make_shared<Stack>();
    content->setGap(0);
    for (size_t offset = 0; offset < value.size(); offset += 240)
        content->add(pageText(value.substr(offset, 240), size, palette.ink, 400, availableWidth));
    content->setPreferredSize({0, content->contentHeight()});
    return content;
}

inline std::shared_ptr<Checkbox> checkRow(std::wstring text, std::wstring description,
                                          const ControlPalette &p, float scale = 1) {
    auto control = std::make_shared<Checkbox>(std::move(text));
    control->setAccessibleDescription(std::move(description));
    CheckboxStateStyleOverride normal, selected;
    normal.labelColor = p.secondary;
    normal.boxBackground = p.surface;
    normal.boxBorder = p.secondary;
    selected.boxBackground = p.tint;
    selected.boxBorder = p.tint;
    selected.checkColor = p.surface;
    CheckboxStyleOverride style;
    style.normal = normal;
    style.selected = selected;
    control->setStyleOverride(style);
    control->setPreferredSize({0, std::max(48.f, 52 * scale)});
    return control;
}

enum class ActionKind { Primary, Secondary, Text, Destructive, Navigation };
inline std::shared_ptr<Button> actionButton(std::wstring title, std::function<void()> action,
                                            const ControlPalette &p, ActionKind kind = ActionKind::Primary,
                                            float scale = 1) {
    auto button = std::make_shared<Button>(std::move(title));
    const bool textAction = kind == ActionKind::Text || kind == ActionKind::Navigation;
    ButtonStateStyleOverride normal;
    normal.fontSize = 17 * scale;
    normal.fontWeight = textAction ? 400 : 600;
    normal.radius = 12;
    normal.borderWidth = 0;
    normal.background = kind == ActionKind::Primary       ? p.tint
                        : kind == ActionKind::Destructive ? p.danger
                        : textAction                      ? Color{0, 0, 0, 0}
                                                          : p.surface;
    normal.foreground =
        kind == ActionKind::Primary || kind == ActionKind::Destructive ? Color{255, 255, 255} : p.tint;
    // Dark tints are intentionally lighter: use a dark on-tint foreground.
    if ((kind == ActionKind::Primary || kind == ActionKind::Destructive) && p.ink.r > 200)
        normal.foreground = Color{20, 35, 65};
    if (kind == ActionKind::Navigation) {
        normal.padding = Insets{0};
        normal.iconSize = 20;
        normal.iconGap = 4;
        button->setContentAlign(TextAlign::Left);
    }
    ButtonStyleOverride style;
    style.normal = normal;
    style.hovered = normal;
    auto pressed = normal;
    pressed.background = kind == ActionKind::Primary || kind == ActionKind::Destructive
                             ? Color{static_cast<uint8_t>(normal.background->r * .86f),
                                     static_cast<uint8_t>(normal.background->g * .86f),
                                     static_cast<uint8_t>(normal.background->b * .86f)}
                             : p.pressed;
    style.pressed = pressed;
    auto disabled = normal;
    disabled.background = p.pressed;
    disabled.foreground = p.secondary;
    style.disabled = disabled;
    button->setStyleOverride(style);
    button->setPreferredSize({0, std::max(50.0f, 50 * scale)});
    button->setOnClick(std::move(action));
    return button;
}

inline void styleTextField(TextField &field, const ControlPalette &p, float scale = 1) {
    TextFieldStateStyleOverride normal;
    normal.background = p.surface;
    normal.foreground = p.ink;
    normal.placeholderForeground = p.secondary;
    normal.border = p.separator;
    normal.borderWidth = .5f;
    normal.radius = 12;
    normal.padding = Insets{0, 16};
    normal.caretColor = p.tint;
    normal.selectionBackground = Color{189, 213, 255};
    TextFieldStyleOverride style;
    style.normal = normal;
    style.hovered = normal;
    style.readOnly = normal;
    auto focused = normal;
    focused.border = p.tint;
    style.focusVisible = focused;
    field.setStyleOverride(style);
    field.setFontSize(17 * scale);
    field.setPreferredSize({0, std::max(52.0f, 52 * scale)});
}

inline std::shared_ptr<Stack> labeledInput(std::shared_ptr<TextField> field, std::wstring label,
                                           const ControlPalette &p, float scale = 1,
                                           std::shared_ptr<Widget> trailing = {}) {
    auto group = std::make_shared<Stack>();
    group->setGap(6 * scale);
    const float inputHeight = std::max(52.f, 52 * scale);
    group->setPreferredSize({0, 28 * scale + inputHeight});
    auto title = pageText(std::move(label), 13 * scale, p.secondary);
    title->setPreferredSize({0, 22 * scale});
    group->add(title);
    if (trailing) {
        auto line = std::make_shared<Stack>(StackDirection::Row);
        line->setGap(8);
        line->setPreferredSize({0, inputHeight});
        line->add(field);
        StackFlex fill;
        fill.grow = 1;
        fill.basis = 0;
        line->setFlex(field, fill);
        trailing->setPreferredSize({124 * scale, inputHeight});
        line->add(trailing);
        group->add(line);
    } else
        group->add(field);
    return group;
}

inline std::shared_ptr<Panel> insetGroup(std::shared_ptr<Widget> child, const ControlPalette &p) {
    auto panel = std::make_shared<Panel>();
    panel->setBackground(p.surface);
    panel->setBorderWidth(0);
    panel->setRadius(14);
    panel->setPreferredSize(child->preferredSize());
    panel->setContent(std::move(child));
    return panel;
}

// A two-line, icon-leading row with one semantic action. ScrollView owns drag
// cancellation; a cancelled press never emits the row's click callback.
class ListRow final : public Widget {
  public:
    ListRow(std::wstring title, std::wstring detail, ControlPalette palette, float scale = 1)
        : title_(std::move(title)), detail_(std::move(detail)), palette_(palette), scale_(scale) {
        setPreferredSize({0, (detail_.empty() ? 56 : 68) * scale_});
        setAccessibleRole(AccessibilityRole::ListItem);
        setAccessibleName(title_);
        setAccessibleDescription(detail_);
    }
    void setIcon(IconSymbol icon) {
        icon_ = icon;
        hasIcon_ = true;
        invalidate();
    }
    void setOnClick(std::function<void()> action) {
        action_ = std::move(action);
        setAccessibleRole(action_ ? AccessibilityRole::Button : AccessibilityRole::ListItem);
    }
    void setValue(std::wstring value) {
        value_ = std::move(value);
        invalidate();
    }
    void setSelected(bool selected) {
        selected_ = selected;
        selectable_ = true;
        invalidate();
    }
    void setSeparator(bool separator) {
        separator_ = separator;
        invalidate();
    }
    void setDestructive(bool destructive) {
        destructive_ = destructive;
        invalidate();
    }
    bool isFocusable() const override { return !disabled() && bool(action_); }
    AccessibilityInfo accessibilityInfo() const override {
        auto info = Widget::accessibilityInfo();
        info.value = value_;
        info.state.selected = selected_;
        return info;
    }
    void paint(Canvas &canvas) override {
        const auto r = frame();
        if (pressed_ && !disabled())
            canvas.fillRect(r, palette_.pressed, 12);
        const float left = r.x + 16, iconSize = 24 * scale_;
        const float x = left + (hasIcon_ ? 43 * scale_ : 0);
        const float trailing = selectable_ || action_ ? 30 * scale_ : 0;
        const float valueWidth = value_.empty() ? 0 : std::min(r.width * .42f, 160 * scale_);
        const Color foreground = destructive_ ? palette_.danger : palette_.ink;
        if (hasIcon_)
            paintIcon(canvas, icon_, {left, r.y + (r.height - iconSize) / 2, iconSize, iconSize}, foreground);
        const float titleHeight = 25 * scale_;
        const float y = detail_.empty() ? r.y + (r.height - titleHeight) / 2 : r.y + 9 * scale_;
        canvas.drawTextStyledEllipsized(
            title_, {x, y, std::max(0.f, r.x + r.width - x - 16 - trailing - valueWidth), titleHeight},
            foreground, 17 * scale_, TextAlign::Left, 550);
        if (!detail_.empty())
            canvas.drawTextStyledEllipsized(
                detail_, {x, y + titleHeight, std::max(0.f, r.x + r.width - x - 16 - trailing), 21 * scale_},
                palette_.secondary, 14 * scale_, TextAlign::Left);
        if (!value_.empty())
            canvas.drawTextStyledEllipsized(
                value_, {r.x + r.width - 16 - trailing - valueWidth, r.y, valueWidth, r.height},
                palette_.secondary, 14 * scale_, TextAlign::Right);
        if (selected_)
            paintIcon(canvas, IconSymbol::Check,
                      {r.x + r.width - 40 * scale_, r.y + (r.height - iconSize) / 2, iconSize, iconSize},
                      palette_.tint);
        else if (!selectable_ && action_)
            paintIcon(
                canvas, IconSymbol::ChevronRight,
                {r.x + r.width - 30 * scale_, r.y + (r.height - 18 * scale_) / 2, 18 * scale_, 18 * scale_},
                palette_.secondary);
        if (separator_)
            canvas.drawLine({x, r.y + r.height - .5f}, {r.x + r.width, r.y + r.height - .5f},
                            palette_.separator, .5f);
    }
    bool onMouseDown(const MouseEvent &event) override {
        if (!isFocusable() || event.button != MouseButton::Left || !frame().contains(event.position))
            return false;
        pressed_ = true;
        invalidate();
        return true;
    }
    bool onMouseUp(const MouseEvent &event) override {
        const bool invoke = pressed_ && !disabled() && frame().contains(event.position);
        pressed_ = false;
        invalidate();
        if (invoke && action_)
            action_();
        return invoke;
    }
    bool onMouseMove(const MouseEvent &event) override {
        if (pressed_ && !frame().contains(event.position)) {
            pressed_ = false;
            invalidate();
        }
        return false;
    }
    bool onKeyDown(const KeyEvent &event) override {
        if (!isFocusable() || !event.pressed || (event.key != Key::Enter && event.key != Key::Space))
            return false;
        action_();
        return true;
    }

  protected:
    bool hasInteractionState() const override { return pressed_; }
    void resetInteractionState() override { pressed_ = false; }

  private:
    std::wstring title_, detail_, value_;
    ControlPalette palette_;
    float scale_;
    IconSymbol icon_ = IconSymbol::Device;
    bool hasIcon_ = false, selected_ = false, selectable_ = false, separator_ = false, destructive_ = false,
         pressed_ = false;
    std::function<void()> action_;
};

class Cooldown final {
  public:
    int nextChangeMillis() const {
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_ - std::chrono::steady_clock::now())
                .count();
        return ms > 0 ? static_cast<int>(ms % 1000 + 1) : -1;
    }
    void start(int seconds) {
        end_ = std::chrono::steady_clock::now() + std::chrono::seconds(std::max(0, seconds));
    }
    int remaining() const {
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_ - std::chrono::steady_clock::now())
                .count();
        return ms > 0 ? static_cast<int>((ms + 999) / 1000) : 0;
    }

  private:
    std::chrono::steady_clock::time_point end_{};
};
} // namespace oneui::mobile
