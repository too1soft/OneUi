#pragma once
#include <oneui/mobile/presentation.h>
#include <oneui/controls/slider.h>
#include <oneui/controls/switch.h>
#include <cerrno>
#include <cmath>
#include <cwchar>

namespace oneui::mobile {
inline std::shared_ptr<Stack> settingSwitch(std::wstring title, bool checked, ControlPalette palette,
    std::function<void(bool)> changed, float scale, float width) {
    auto row = std::make_shared<Stack>(StackDirection::Row);
    row->setAlign(StackAlign::Center);
    auto label = pageText(title, 16 * scale, palette.ink, 400, std::max(1.f, width - 80 * scale));
    row->setPreferredSize({0, std::max(52 * scale, label->preferredSize().height)});
    row->add(label);
    StackFlex fill;
    fill.grow = 1;
    fill.basis = 0;
    row->setFlex(label, fill);
    auto toggle = std::make_shared<Switch>();
    toggle->setAccessibleName(std::move(title));
    toggle->setPreferredSize({52 * scale, std::max(48.f, 48 * scale)});
    toggle->setChecked(checked);
    SwitchStateStyleOverride normal, selected;
    normal.trackBackground = palette.separator;
    selected.trackBackground = palette.tint;
    SwitchStyleOverride style;
    style.normal = normal;
    style.selected = selected;
    toggle->setStyleOverride(style);
    toggle->setOnChanged(std::move(changed));
    row->add(toggle);
    return row;
}

inline std::optional<int> boundedInteger(const std::wstring &text, int minimum, int maximum) {
    if (text.empty()) return {};
    wchar_t *end = nullptr;
    errno = 0;
    const auto value = std::wcstoll(text.c_str(), &end, 10);
    if (errno == ERANGE || !end || *end || value < minimum || value > maximum) return {};
    return static_cast<int>(value);
}

struct NumberSlider {
    std::shared_ptr<Stack> view;
    std::shared_ptr<TextField> field;
    std::shared_ptr<Slider> slider;
    int minimum = 0, maximum = 100;
    std::optional<int> value() const { return field ? boundedInteger(field->text(), minimum, maximum) : std::nullopt; }
};

// Slider and editable numeric value share one value. Editing never rebuilds
// the owning page; incomplete input stays in the editor until validation.
inline NumberSlider numberSlider(std::wstring title, int value, int minimum, int maximum, int step,
    ControlPalette palette, float scale, std::function<void(int)> changed) {
    NumberSlider result;
    result.minimum = minimum;
    result.maximum = maximum;
    result.view = std::make_shared<Stack>();
    result.view->setGap(2);
    result.field = std::make_shared<TextField>();
    result.field->setAccessibleName(title);
    result.field->setText(std::to_wstring(value));
    styleTextField(*result.field, palette, scale);
    result.field->setPreferredSize({96 * scale, std::max(52.f, 52 * scale)});
    auto line = std::make_shared<Stack>(StackDirection::Row);
    line->setAlign(StackAlign::Center);
    line->setPreferredSize({0, result.field->preferredSize().height});
    auto label = pageText(title, 14 * scale, palette.ink);
    line->add(label);
    StackFlex fill;
    fill.grow = 1;
    fill.basis = 0;
    line->setFlex(label, fill);
    line->add(result.field);
    result.view->add(line);
    result.slider = std::make_shared<Slider>();
    result.slider->setAccessibleName(title);
    result.slider->setRange(minimum, maximum);
    result.slider->setStep(step);
    result.slider->setValue(value);
    result.slider->setPreferredSize({0, std::max(48.f, 48 * scale)});
    SliderStateStyleOverride normal;
    normal.trackBackground = palette.separator;
    normal.trackFill = palette.tint;
    normal.thumbBackground = palette.surface;
    normal.thumbBorder = palette.separator;
    normal.thumbBorderWidth = .5f;
    normal.thumbSize = 22 * scale;
    SliderStyleOverride style;
    style.normal = normal;
    result.slider->setStyleOverride(style);
    std::weak_ptr<TextField> editor = result.field;
    std::weak_ptr<Slider> slider = result.slider;
    result.slider->setOnChanged([editor, changed](double raw) {
        const int next = static_cast<int>(std::lround(raw));
        if (auto field = editor.lock(); field && !field->focused()) field->setText(std::to_wstring(next));
        changed(next);
    });
    result.field->setOnChanged([slider, minimum, maximum](const std::wstring &text) {
        if (const auto next = boundedInteger(text, minimum, maximum))
            if (auto control = slider.lock()) control->setValue(*next);
    });
    result.view->add(result.slider);
    result.view->setPreferredSize({0, result.view->contentHeight()});
    return result;
}
} // namespace oneui::mobile
