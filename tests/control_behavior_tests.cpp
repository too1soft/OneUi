#include "oneui/controls/button.h"
#include "oneui/controls/button_bridge.h"
#include "oneui/controls/badge.h"
#include "oneui/controls/card.h"
#include "oneui/controls/checkbox.h"
#include "oneui/controls/form_field.h"
#include "oneui/controls/icon_view.h"
#include "oneui/controls/list.h"
#include "oneui/controls/virtual_list.h"
#include "oneui/controls/nav_item.h"
#include "oneui/controls/popup.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/controls/radio_group.h"
#include "oneui/controls/select.h"
#include "oneui/controls/separator.h"
#include "oneui/controls/slider.h"
#include "oneui/controls/status_strip.h"
#include "oneui/controls/state_view.h"
#include "oneui/controls/switch.h"
#include "oneui/controls/tabs.h"
#include "oneui/controls/table.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/text_input_bridge.h"
#include "oneui/controls/tree_view.h"
#include "oneui/controls/tile.h"
#include "oneui/controls/toast.h"
#include "oneui/controls/validation_message.h"
#include "oneui/controls/window_title_bar.h"
#include "oneui/layout/app_shell.h"
#include "oneui/layout/dock_view.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/layout/panel.h"
#include "oneui/layout/product_shell.h"
#include "oneui/layout/reorderable_grid.h"
#include "oneui/layout/scroll_view.h"
#include "oneui/layout/sidebar_nav_bridge.h"
#include "oneui/layout/split_view.h"
#include "oneui/layout/stack.h"
#include "oneui/layout/title_bar_bridge.h"
#include "oneui/layout/wrap.h"
#include "oneui/controls/label.h"
#include "oneui/controls/log_view.h"
#include "oneui/animation.h"
#include "oneui/icon.h"
#include "oneui/material3_tokens.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/style_adapter.h"
#include "oneui/style_sheet.h"
#include "oneui/style_transition.h"
#include "oneui/view.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

int failures = 0;

struct StrokeRectCall {
    oneui::Rect rect;
    oneui::Color color;
    float radius;
    float width;
};

struct FillRectCall {
    oneui::Rect rect;
    oneui::Color color;
    float radius;
};

struct FillEllipseCall {
    oneui::Rect rect;
    oneui::Color color;
};

struct StrokeEllipseCall {
    oneui::Rect rect;
    oneui::Color color;
    float width;
};

struct DrawTextCall {
    std::wstring text;
    oneui::Rect rect;
    oneui::Color color;
    float size = 0.0f;
    int weight = 400;
};

struct DrawLineCall {
    oneui::Point from;
    oneui::Point to;
    oneui::Color color;
    float width;
};

struct BoxShadowCall {
    oneui::Rect rect;
    oneui::BoxShadow shadow;
    float radius;
};

class RecordingCanvas final : public oneui::Canvas {
public:
    void save() override {
        ++saves;
    }

    void restore() override {
        ++restores;
    }

    void clipRect(oneui::Rect rect) override {
        clips.push_back(rect);
    }

    std::optional<oneui::Rect> clipBounds() const override {
        return clipOverride;
    }

    void clear(oneui::Color) override {}

    void fillRect(oneui::Rect rect, oneui::Color color, float radius = 0.0f) override {
        fillRects.push_back(FillRectCall{rect, color, radius});
    }

    void strokeRect(oneui::Rect rect, oneui::Color color, float radius, float width = 1.0f) override {
        strokeRects.push_back(StrokeRectCall{rect, color, radius, width});
    }

    void fillEllipse(oneui::Rect rect, oneui::Color color) override {
        fillEllipses.push_back(FillEllipseCall{rect, color});
    }
    void strokeEllipse(oneui::Rect rect, oneui::Color color, float width = 1.0f) override {
        strokeEllipses.push_back(StrokeEllipseCall{rect, color, width});
    }
    void drawLine(oneui::Point from, oneui::Point to, oneui::Color color, float width = 1.0f) override {
        lines.push_back(DrawLineCall{from, to, color, width});
    }
    void drawBoxShadow(oneui::Rect rect, const oneui::BoxShadow& shadow, float radius = 0.0f) override {
        boxShadows.push_back(BoxShadowCall{rect, shadow, radius});
    }
    void drawText(const std::wstring& text, oneui::Rect rect, oneui::Color color, float size, oneui::TextAlign = oneui::TextAlign::Center) override {
        texts.push_back(DrawTextCall{text, rect, color, size, 400});
    }
    void drawTextStyled(
        const std::wstring& text,
        oneui::Rect rect,
        oneui::Color color,
        float size,
        oneui::TextAlign = oneui::TextAlign::Center,
        int weight = 400) override {
        texts.push_back(DrawTextCall{text, rect, color, size, weight});
    }
    float measureTextWidth(const std::wstring& text, float size, int weight = 400) const override {
        (void)weight;
        const float scale = size / 14.0f;
        float width = 0.0f;
        for (wchar_t character : text) {
            switch (character) {
            case L'W':
            case L'M':
            case L'@':
            case L'#':
                width += 10.0f;
                break;
            case L'i':
            case L'l':
            case L'I':
            case L'!':
            case L'|':
            case L' ':
                width += 4.0f;
                break;
            default:
                width += 7.0f;
                break;
            }
        }
        return width * scale;
    }

    std::vector<FillRectCall> fillRects;
    std::vector<FillEllipseCall> fillEllipses;
    std::vector<StrokeRectCall> strokeRects;
    std::vector<StrokeEllipseCall> strokeEllipses;
    std::vector<DrawTextCall> texts;
    std::vector<DrawLineCall> lines;
    std::vector<BoxShadowCall> boxShadows;
    std::vector<oneui::Rect> clips;
    std::optional<oneui::Rect> clipOverride;
    int saves = 0;
    int restores = 0;
};

class MouseUpProbe final : public oneui::Widget {
public:
    void paint(oneui::Canvas&) override {}

    bool onMouseDown(const oneui::MouseEvent& event) override {
        return contains(event.position);
    }

    bool onMouseUp(const oneui::MouseEvent&) override {
        ++mouseUps;
        return true;
    }

    int mouseUps = 0;
};

class MouseMoveProbe final : public oneui::Widget {
public:
    void paint(oneui::Canvas&) override {}

    bool onMouseMove(const oneui::MouseEvent& event) override {
        const bool next = contains(event.position);
        if (next == hovered) {
            return false;
        }
        hovered = next;
        invalidate();
        return true;
    }

    bool hovered = false;

private:
    bool hasInteractionState() const override {
        return hovered;
    }

    void resetInteractionState() override {
        hovered = false;
    }
};

class LayoutProbe final : public oneui::Widget {
public:
    explicit LayoutProbe(oneui::Size preferred) {
        setPreferredSize(preferred);
    }

    void paint(oneui::Canvas&) override {}
};

class PaintProbe final : public oneui::Widget {
public:
    void paint(oneui::Canvas&) override {
        ++paintCalls;
    }

    void requestInvalidation() {
        invalidate();
    }

    int paintCalls = 0;
};

void expectEqual(const char* name, int actual, int expected) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectWideEqual(const char* name, const std::wstring& actual, const std::wstring& expected) {
    if (actual != expected) {
        std::cerr << name << ": unexpected wide string value\n";
        ++failures;
    }
}

void expectNear(const char* name, float actual, float expected) {
    if (std::fabs(actual - expected) > 0.001f) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectRect(const char* name, oneui::Rect actual, oneui::Rect expected) {
    expectNear((std::string(name) + " x").c_str(), actual.x, expected.x);
    expectNear((std::string(name) + " y").c_str(), actual.y, expected.y);
    expectNear((std::string(name) + " width").c_str(), actual.width, expected.width);
    expectNear((std::string(name) + " height").c_str(), actual.height, expected.height);
}

void testSingleLineTextEllipsizesByMeasuredWidth() {
    RecordingCanvas canvas;
    const std::wstring fitted = canvas.ellipsizeText(L"Cloud synchronization", 52.0f, 14.0f);
    expectEqual("Canvas ellipsis preserves a visible prefix", fitted.size() > 1 ? 1 : 0, 1);
    expectEqual("Canvas ellipsis uses one semantic ellipsis glyph", fitted.back() == L'\u2026' ? 1 : 0, 1);
    expectEqual(
        "Canvas ellipsis respects the measured width",
        canvas.measureTextWidth(fitted, 14.0f) <= 52.0f ? 1 : 0,
        1);

    oneui::Label label(L"A long host name that must not be abruptly clipped");
    label.setFrame(oneui::Rect{0.0f, 0.0f, 72.0f, 22.0f});
    label.paint(canvas);
    expectEqual("Label paints exactly one fitted line", static_cast<int>(canvas.texts.size()), 1);
    expectEqual("Label delegates overflow to the shared ellipsis contract", canvas.texts.back().text.back() == L'\u2026' ? 1 : 0, 1);
}

void testWidgetAccessibilityInfoReflectsSemanticAndDynamicState() {
    LayoutProbe widget(oneui::Size{100.0f, 30.0f});
    widget.setAccessibleRole(oneui::AccessibilityRole::TextBox);
    widget.setAccessibleName(L"Project name");
    widget.setAccessibleDescription(L"Required project display name");
    widget.setAccessibleValue(L"OneUI");

    oneui::AccessibilityState authoredState;
    authoredState.required = true;
    authoredState.invalid = true;
    widget.setAccessibilityState(authoredState);

    widget.onFocusChanged(true);
    widget.setFocusVisible(true);

    const auto focusedInfo = widget.accessibilityInfo();
    expectEqual("Widget accessibility role", static_cast<int>(focusedInfo.role), static_cast<int>(oneui::AccessibilityRole::TextBox));
    expectWideEqual("Widget accessibility name", focusedInfo.name, L"Project name");
    expectWideEqual("Widget accessibility description", focusedInfo.description, L"Required project display name");
    expectWideEqual("Widget accessibility value", focusedInfo.value, L"OneUI");
    expectEqual("Widget accessibility focused", focusedInfo.state.focused ? 1 : 0, 1);
    expectEqual("Widget accessibility focusVisible", focusedInfo.state.focusVisible ? 1 : 0, 1);
    expectEqual("Widget accessibility required", focusedInfo.state.required ? 1 : 0, 1);
    expectEqual("Widget accessibility invalid", focusedInfo.state.invalid ? 1 : 0, 1);

    widget.setDisabled(true);

    const auto disabledInfo = widget.accessibilityInfo();
    expectEqual("Widget accessibility disabled follows runtime state", disabledInfo.state.disabled ? 1 : 0, 1);
    expectEqual("Widget accessibility disabled clears focus", disabledInfo.state.focused ? 1 : 0, 0);
    expectEqual("Widget accessibility disabled clears focusVisible", disabledInfo.state.focusVisible ? 1 : 0, 0);
    expectEqual("Widget accessibility authored state survives dynamic merge", disabledInfo.state.required ? 1 : 0, 1);
}

void testCommonControlsExposeDefaultAccessibilityInfo() {
    oneui::Button button(L"Save");
    button.setFrame(oneui::Rect{0.0f, 0.0f, 96.0f, 36.0f});

    auto buttonInfo = button.accessibilityInfo();
    expectEqual("Button accessibility role", static_cast<int>(buttonInfo.role), static_cast<int>(oneui::AccessibilityRole::Button));
    expectWideEqual("Button accessibility default name", buttonInfo.name, L"Save");

    button.setAccessibleName(L"Save project");
    button.onMouseDown(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});
    buttonInfo = button.accessibilityInfo();
    expectWideEqual("Button accessibility explicit name wins", buttonInfo.name, L"Save project");
    expectEqual("Button accessibility pressed follows runtime state", buttonInfo.state.pressed ? 1 : 0, 1);

    oneui::TextField textField(L"Project name");
    textField.setText(L"OneUI");
    auto textInfo = textField.accessibilityInfo();
    expectEqual("TextField accessibility role", static_cast<int>(textInfo.role), static_cast<int>(oneui::AccessibilityRole::TextBox));
    expectWideEqual("TextField accessibility default name from placeholder", textInfo.name, L"Project name");
    expectWideEqual("TextField accessibility value", textInfo.value, L"OneUI");

    oneui::Checkbox checkbox(L"Enable preview");
    checkbox.setChecked(true);
    auto checkboxInfo = checkbox.accessibilityInfo();
    expectEqual("Checkbox accessibility role", static_cast<int>(checkboxInfo.role), static_cast<int>(oneui::AccessibilityRole::CheckBox));
    expectWideEqual("Checkbox accessibility default name", checkboxInfo.name, L"Enable preview");
    expectWideEqual("Checkbox accessibility value checked", checkboxInfo.value, L"checked");
    expectEqual("Checkbox accessibility checked state", checkboxInfo.state.checked ? 1 : 0, 1);

    oneui::Select select;
    select.setItems({L"Windows", L"Linux", L"macOS"});
    select.setSelectedIndex(1);
    auto selectInfo = select.accessibilityInfo();
    expectEqual("Select accessibility role", static_cast<int>(selectInfo.role), static_cast<int>(oneui::AccessibilityRole::ComboBox));
    expectWideEqual("Select accessibility value", selectInfo.value, L"Linux");
    expectEqual("Select accessibility collapsed state", selectInfo.state.expanded ? 1 : 0, 0);

    select.onKeyDown(oneui::KeyEvent{oneui::Key::Space});
    selectInfo = select.accessibilityInfo();
    expectEqual("Select accessibility expanded state", selectInfo.state.expanded ? 1 : 0, 1);
}

void testSelectionAndDataControlsExposeDefaultAccessibilityInfo() {
    oneui::Slider slider;
    slider.setRange(0.0, 10.0);
    slider.setValue(4.5);
    auto sliderInfo = slider.accessibilityInfo();
    expectEqual("Slider accessibility role", static_cast<int>(sliderInfo.role), static_cast<int>(oneui::AccessibilityRole::Slider));
    expectWideEqual("Slider accessibility value", sliderInfo.value, L"4.500000");

    oneui::RadioGroup radio;
    radio.setItems({L"Compact", L"Balanced", L"Comfortable"});
    radio.setSelectedIndex(2);
    auto radioInfo = radio.accessibilityInfo();
    expectEqual("RadioGroup accessibility role", static_cast<int>(radioInfo.role), static_cast<int>(oneui::AccessibilityRole::RadioGroup));
    expectWideEqual("RadioGroup accessibility value", radioInfo.value, L"Comfortable");
    expectEqual("RadioGroup accessibility selected state", radioInfo.state.selected ? 1 : 0, 1);

    oneui::Tabs tabs;
    tabs.setItems({L"Props", L"State", L"Events"});
    tabs.setSelectedIndex(1);
    auto tabsInfo = tabs.accessibilityInfo();
    expectEqual("Tabs accessibility role", static_cast<int>(tabsInfo.role), static_cast<int>(oneui::AccessibilityRole::TabList));
    expectWideEqual("Tabs accessibility value", tabsInfo.value, L"State");
    expectEqual("Tabs accessibility selected state", tabsInfo.state.selected ? 1 : 0, 1);

    oneui::List list;
    list.setItems({
        oneui::ListItem{L"Acme", L"Live"},
        oneui::ListItem{L"Billing", L"Review"},
    });
    list.setSelectedIndex(1);
    auto listInfo = list.accessibilityInfo();
    expectEqual("List accessibility role", static_cast<int>(listInfo.role), static_cast<int>(oneui::AccessibilityRole::List));
    expectWideEqual("List accessibility value", listInfo.value, L"Billing - Review");
    expectEqual("List accessibility selected state", listInfo.state.selected ? 1 : 0, 1);

    oneui::Table table;
    table.setColumns({oneui::TableColumn{L"Name"}, oneui::TableColumn{L"Status"}});
    table.setRows({{L"Acme", L"Live"}, {L"Billing", L"Review"}});
    auto tableInfo = table.accessibilityInfo();
    expectEqual("Table accessibility role", static_cast<int>(tableInfo.role), static_cast<int>(oneui::AccessibilityRole::Table));
    expectWideEqual("Table accessibility value", tableInfo.value, L"2 rows, 2 columns");
}

bool sameColor(oneui::Color left, oneui::Color right) {
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

int countPrimaryOrThickStrokeRects(const RecordingCanvas& canvas) {
    int count = 0;
    for (const auto& stroke : canvas.strokeRects) {
        if (sameColor(stroke.color, oneui::theme().primary) || stroke.width > 1.0f) {
            ++count;
        }
    }
    return count;
}

int countFillRectsWithColor(const RecordingCanvas& canvas, oneui::Color color) {
    int count = 0;
    for (const auto& fill : canvas.fillRects) {
        if (sameColor(fill.color, color)) {
            ++count;
        }
    }
    return count;
}

int countStrokeRectsWithColor(const RecordingCanvas& canvas, oneui::Color color) {
    int count = 0;
    for (const auto& stroke : canvas.strokeRects) {
        if (sameColor(stroke.color, color)) {
            ++count;
        }
    }
    return count;
}

int countCaretRects(const RecordingCanvas& canvas) {
    int count = 0;
    for (const auto& fill : canvas.fillRects) {
        if (fill.rect.width <= 1.25f && fill.rect.height >= 9.0f && fill.rect.height <= 16.0f) {
            ++count;
        }
    }
    return count;
}

double testSteadyTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

int countFillEllipsesWithColor(const RecordingCanvas& canvas, oneui::Color color) {
    int count = 0;
    for (const auto& fill : canvas.fillEllipses) {
        if (sameColor(fill.color, color)) {
            ++count;
        }
    }
    return count;
}

int countStrokeEllipsesWithColor(const RecordingCanvas& canvas, oneui::Color color) {
    int count = 0;
    for (const auto& stroke : canvas.strokeEllipses) {
        if (sameColor(stroke.color, color)) {
            ++count;
        }
    }
    return count;
}

int countLinesWithColor(const RecordingCanvas& canvas, oneui::Color color) {
    int count = 0;
    for (const auto& line : canvas.lines) {
        if (sameColor(line.color, color)) {
            ++count;
        }
    }
    return count;
}

int countTextsWithColor(const RecordingCanvas& canvas, oneui::Color color) {
    int count = 0;
    for (const auto& text : canvas.texts) {
        if (sameColor(text.color, color)) {
            ++count;
        }
    }
    return count;
}

int countTextsWithTextAndColor(const RecordingCanvas& canvas, const std::wstring& textValue, oneui::Color color) {
    int count = 0;
    for (const auto& text : canvas.texts) {
        if (text.text == textValue && sameColor(text.color, color)) {
            ++count;
        }
    }
    return count;
}

int countTextsWithText(const RecordingCanvas& canvas, const std::wstring& textValue) {
    int count = 0;
    for (const auto& text : canvas.texts) {
        if (text.text == textValue) {
            ++count;
        }
    }
    return count;
}

void testCheckboxNoopOnChanged() {
    oneui::Checkbox checkbox(L"Checked");
    int changes = 0;
    checkbox.setOnChanged([&](bool) {
        ++changes;
    });

    checkbox.setChecked(true);
    changes = 0;
    checkbox.setChecked(true);

    expectEqual("Checkbox same-value setChecked", changes, 0);
}

void testBoundCheckboxNoopOnChanged() {
    oneui::State<bool> checked(true);
    oneui::Checkbox checkbox(L"Bound checked");
    checkbox.bindChecked(checked);

    int changes = 0;
    checkbox.setOnChanged([&](bool) {
        ++changes;
    });

    checkbox.setChecked(true);

    expectEqual("Checkbox bound same-value setChecked", changes, 0);
}

void testSwitchNoopOnChanged() {
    oneui::Switch control(L"Enabled");
    int changes = 0;
    control.setOnChanged([&](bool) {
        ++changes;
    });

    control.setChecked(true);
    changes = 0;
    control.setChecked(true);

    expectEqual("Switch same-value setChecked", changes, 0);
}

void testSliderNoopOnChanged() {
    oneui::Slider slider;
    int changes = 0;
    slider.setOnChanged([&](double) {
        ++changes;
    });

    slider.setValue(0.5);
    changes = 0;
    slider.setValue(0.5);

    expectEqual("Slider same-value setValue", changes, 0);
}

void testTabsNoopOnChanged() {
    oneui::Tabs tabs;
    tabs.setItems({L"One", L"Two", L"Three"});
    int changes = 0;
    tabs.setOnChanged([&](int) {
        ++changes;
    });

    tabs.setSelectedIndex(1);
    changes = 0;
    tabs.setSelectedIndex(1);

    expectEqual("Tabs same-value setSelectedIndex", changes, 0);
}

void testRadioGroupNoopOnChanged() {
    oneui::RadioGroup group;
    group.setItems({L"One", L"Two", L"Three"});
    int changes = 0;
    group.setOnChanged([&](int) {
        ++changes;
    });

    group.setSelectedIndex(2);
    changes = 0;
    group.setSelectedIndex(2);

    expectEqual("RadioGroup same-value setSelectedIndex", changes, 0);
}

void testSelectNoopOnChanged() {
    oneui::Select select;
    select.setItems({L"One", L"Two", L"Three"});
    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    select.setSelectedIndex(1);
    changes = 0;
    select.setSelectedIndex(1);

    expectEqual("Select same-value setSelectedIndex", changes, 0);
}

void testListNoopOnChanged() {
    oneui::List list;
    list.setItems({
        oneui::ListItem{L"One", L"First"},
        oneui::ListItem{L"Two", L"Second"},
        oneui::ListItem{L"Three", L"Third"},
    });
    int changes = 0;
    list.setOnChanged([&](int) {
        ++changes;
    });

    list.setSelectedIndex(1);
    changes = 0;
    list.setSelectedIndex(1);

    expectEqual("List same-value setSelectedIndex", changes, 0);
}

void testListSupportsOptionalSelection() {
    oneui::List list;
    list.setItems({
        oneui::ListItem{L"One", L"First"},
        oneui::ListItem{L"Two", L"Second"},
    });
    list.setSelectionRequired(false);
    list.setSelectedIndex(-1);

    expectEqual("Optional List preserves no selection", list.selectedIndex(), -1);

    list.setSelectionRequired(true);
    expectEqual("Required List restores the first selection", list.selectedIndex(), 0);
}

void testBoundListSelectedIndex() {
    oneui::State<int> selected(0);
    oneui::List list;
    list.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 90.0f});
    list.setItems({
        oneui::ListItem{L"One", L"First"},
        oneui::ListItem{L"Two", L"Second"},
        oneui::ListItem{L"Three", L"Third"},
    });
    list.bindSelectedIndex(selected);

    int changes = 0;
    list.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::MouseEvent thirdRow{oneui::Point{20.0f, 75.0f}};
    list.onMouseDown(thirdRow);
    list.onMouseUp(thirdRow);

    expectEqual("List mouse updates bound selectedIndex", selected.get(), 2);
    expectEqual("List mouse emits onChanged once", changes, 1);

    list.onKeyDown(oneui::KeyEvent{oneui::Key::Up});
    expectEqual("List Up updates bound selectedIndex", selected.get(), 1);
    expectEqual("List Up emits onChanged once", changes, 2);

    list.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    expectEqual("List Down updates bound selectedIndex", selected.get(), 2);
    expectEqual("List Down emits onChanged once", changes, 3);

    list.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    expectEqual("List Down at end keeps bound selectedIndex", selected.get(), 2);
    expectEqual("List Down at end skips duplicate onChanged", changes, 3);
}

void testBoundSelectEffectiveNoopOnChanged() {
    oneui::State<int> selected(99);
    oneui::Select select;
    select.setItems({L"One", L"Two", L"Three"});
    select.bindSelectedIndex(selected);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    select.setSelectedIndex(2);

    expectEqual("Select bound same effective setSelectedIndex", changes, 0);
}

void testSelectSetItemsShrinkClampsAndEmitsOnce() {
    oneui::Select select;
    select.setItems({L"One", L"Two", L"Three"});
    select.setSelectedIndex(2);

    int changes = 0;
    int lastIndex = -1;
    select.setOnChanged([&](int index) {
        ++changes;
        lastIndex = index;
    });

    select.setItems({L"One"});

    expectEqual("Select setItems shrink clamps selectedIndex", select.selectedIndex(), 0);
    expectEqual("Select setItems shrink emits once", changes, 1);
    expectEqual("Select setItems shrink emits clamped index", lastIndex, 0);
}

void testBoundSelectSetItemsShrinkUpdatesStateAndEmitsOnce() {
    oneui::State<int> selected(2);
    oneui::Select select;
    select.setItems({L"One", L"Two", L"Three"});
    select.bindSelectedIndex(selected);

    int changes = 0;
    int lastIndex = -1;
    select.setOnChanged([&](int index) {
        ++changes;
        lastIndex = index;
    });

    select.setItems({L"One"});

    expectEqual("Bound Select setItems shrink updates state", selected.get(), 0);
    expectEqual("Bound Select setItems shrink emits once", changes, 1);
    expectEqual("Bound Select setItems shrink emits clamped index", lastIndex, 0);
}

void testSelectSetItemsSameEffectiveIndexDoesNotEmit() {
    oneui::Select select;
    select.setItems({L"One", L"Two", L"Three"});
    select.setSelectedIndex(1);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    select.setItems({L"Uno", L"Dos", L"Tres"});

    expectEqual("Select setItems same effective index preserves selectedIndex", select.selectedIndex(), 1);
    expectEqual("Select setItems same effective index skips onChanged", changes, 0);
}

void testSelectSetItemsEmptyClosesAndEmitsWhenSelectionChanges() {
    oneui::State<int> selected(2);
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.bindSelectedIndex(selected);

    int changes = 0;
    int lastIndex = -1;
    select.setOnChanged([&](int index) {
        ++changes;
        lastIndex = index;
    });

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    const oneui::Point optionPoint{10.0f, 79.0f};
    select.onMouseDown(fieldClick);
    select.onMouseUp(fieldClick);
    expectEqual("Select setItems empty starts above siblings", select.paintsAboveSiblings() ? 1 : 0, 1);

    select.setItems({});

    expectEqual("Select setItems empty updates state", selected.get(), 0);
    expectEqual("Select setItems empty emits once", changes, 1);
    expectEqual("Select setItems empty emits clamped index", lastIndex, 0);
    expectEqual("Select setItems empty closes popup", select.paintsAboveSiblings() ? 1 : 0, 0);
    expectEqual("Select setItems empty stops hit-testing dropdown", select.hitTest(optionPoint) ? 1 : 0, 0);
}

void testSelectFieldClickOpensWithoutCycling() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.setSelectedIndex(0);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    select.onMouseDown(fieldClick);
    select.onMouseUp(fieldClick);

    expectEqual("Select field click does not cycle selectedIndex", select.selectedIndex(), 0);
    expectEqual("Select field click does not emit onChanged", changes, 0);
}

void testSelectOptionClickSelectsExactlyOnce() {
    oneui::State<int> selected(0);
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.bindSelectedIndex(selected);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    const oneui::MouseEvent optionClick{oneui::Point{10.0f, 79.0f}};

    select.onMouseDown(fieldClick);
    select.onMouseUp(fieldClick);
    select.onMouseDown(optionClick);
    select.onMouseUp(optionClick);

    expectEqual("Select option click updates bound selectedIndex", selected.get(), 1);
    expectEqual("Select option click emits onChanged once", changes, 1);
}

void testSelectSameOptionClickDoesNotEmitOnChanged() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.setSelectedIndex(1);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    const oneui::MouseEvent sameOptionClick{oneui::Point{10.0f, 79.0f}};

    select.onMouseDown(fieldClick);
    select.onMouseUp(fieldClick);
    select.onMouseDown(sameOptionClick);
    select.onMouseUp(sameOptionClick);

    expectEqual("Select same option click preserves selectedIndex", select.selectedIndex(), 1);
    expectEqual("Select same option click does not emit onChanged", changes, 0);
    expectEqual("Select same option click closes popup", select.paintsAboveSiblings() ? 1 : 0, 0);
}

void testSelectSameHighlightedOptionCommitDoesNotEmitOnChanged() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.setSelectedIndex(1);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});

    expectEqual("Select same highlighted commit preserves selectedIndex", select.selectedIndex(), 1);
    expectEqual("Select same highlighted commit does not emit onChanged", changes, 0);
    expectEqual("Select same highlighted commit closes popup", select.paintsAboveSiblings() ? 1 : 0, 0);
}

void testDisabledSelectIgnoresMouseAndKeyboardEvents() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.setSelectedIndex(1);
    select.setDisabled(true);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    const oneui::MouseEvent optionClick{oneui::Point{10.0f, 79.0f}};

    expectEqual("Disabled Select does not hit-test field", select.hitTest(fieldClick.position) ? 1 : 0, 0);
    expectEqual("Disabled Select mouse-down not handled", select.onMouseDown(fieldClick) ? 1 : 0, 0);
    expectEqual("Disabled Select mouse-up not handled", select.onMouseUp(fieldClick) ? 1 : 0, 0);
    expectEqual("Disabled Select Enter not handled", select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter}) ? 1 : 0, 0);
    expectEqual("Disabled Select Down not handled", select.onKeyDown(oneui::KeyEvent{oneui::Key::Down}) ? 1 : 0, 0);

    expectEqual("Disabled Select keeps selectedIndex after events", select.selectedIndex(), 1);
    expectEqual("Disabled Select events do not emit onChanged", changes, 0);
    expectEqual("Disabled Select events do not open popup", select.paintsAboveSiblings() ? 1 : 0, 0);
    expectEqual("Disabled Select does not hit-test dropdown", select.hitTest(optionClick.position) ? 1 : 0, 0);
}

void testBoundDisabledSelectIgnoresMouseAndKeyboardEvents() {
    oneui::State<bool> disabled(true);
    oneui::State<int> selected(1);
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.bindSelectedIndex(selected);
    select.bindDisabled(disabled);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};

    expectEqual("Bound disabled Select mouse-down not handled", select.onMouseDown(fieldClick) ? 1 : 0, 0);
    expectEqual("Bound disabled Select mouse-up not handled", select.onMouseUp(fieldClick) ? 1 : 0, 0);
    expectEqual("Bound disabled Select Space not handled", select.onKeyDown(oneui::KeyEvent{oneui::Key::Space}) ? 1 : 0, 0);
    expectEqual("Bound disabled Select keeps selectedIndex after events", selected.get(), 1);
    expectEqual("Bound disabled Select events do not emit onChanged", changes, 0);
    expectEqual("Bound disabled Select events do not open popup", select.paintsAboveSiblings() ? 1 : 0, 0);
}

void testSelectOptionHitRegionsSelectExpectedItems() {
    oneui::State<int> selected(0);
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.bindSelectedIndex(selected);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    const auto chooseOption = [&](float y) {
        const oneui::MouseEvent optionClick{oneui::Point{10.0f, y}};
        select.onMouseDown(fieldClick);
        select.onMouseUp(fieldClick);
        select.onMouseDown(optionClick);
        select.onMouseUp(optionClick);
    };

    chooseOption(109.0f);
    expectEqual("Select third option hit region selects item", selected.get(), 2);
    expectEqual("Select third option hit region emits once", changes, 1);

    chooseOption(49.0f);
    expectEqual("Select first option hit region selects item", selected.get(), 0);
    expectEqual("Select first option hit region emits once", changes, 2);

    chooseOption(79.0f);
    expectEqual("Select second option hit region selects item", selected.get(), 1);
    expectEqual("Select second option hit region emits once", changes, 3);
}

void testOpenSelectReceivesOptionClickAboveLaterSibling() {
    oneui::State<int> selected(0);
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 140.0f});

    auto select = std::make_shared<oneui::Select>();
    select->setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select->setItems({L"One", L"Two", L"Three"});
    select->bindSelectedIndex(selected);

    auto probe = std::make_shared<MouseUpProbe>();
    probe->setFrame(oneui::Rect{0.0f, 34.0f, 120.0f, 90.0f});

    int changes = 0;
    select->setOnChanged([&](int) {
        ++changes;
    });

    view.add(select);
    view.add(probe);

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    const oneui::MouseEvent optionClick{oneui::Point{10.0f, 79.0f}};

    view.onMouseDown(fieldClick);
    view.onMouseUp(fieldClick);
    view.onMouseDown(optionClick);
    view.onMouseUp(optionClick);

    expectEqual("Open Select option click beats later sibling", selected.get(), 1);
    expectEqual("Open Select option click sibling mouse-up", probe->mouseUps, 0);
    expectEqual("Open Select option click emits once through View", changes, 1);
}

void testOpenSelectClosesOnBlankViewClick() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 260.0f, 140.0f});

    auto select = std::make_shared<oneui::Select>();
    select->setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select->setItems({L"One", L"Two", L"Three"});
    select->setSelectedIndex(1);
    int changes = 0;
    select->setOnChanged([&](int) {
        ++changes;
    });

    view.add(select);

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    const oneui::MouseEvent blankClick{oneui::Point{220.0f, 110.0f}};

    view.onMouseDown(fieldClick);
    view.onMouseUp(fieldClick);
    expectEqual("Open Select blank click starts above siblings", select->paintsAboveSiblings() ? 1 : 0, 1);

    view.onMouseDown(blankClick);

    expectEqual("Open Select blank click closes popup", select->paintsAboveSiblings() ? 1 : 0, 0);
    expectEqual("Open Select blank click preserves selectedIndex", select->selectedIndex(), 1);
    expectEqual("Open Select blank click does not emit onChanged", changes, 0);
}

void testOpenSelectClosesWhenAnotherSelectOpens() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 300.0f, 140.0f});

    auto first = std::make_shared<oneui::Select>();
    first->setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    first->setItems({L"One", L"Two", L"Three"});
    first->setSelectedIndex(1);
    int firstChanges = 0;
    first->setOnChanged([&](int) {
        ++firstChanges;
    });

    auto second = std::make_shared<oneui::Select>();
    second->setFrame(oneui::Rect{150.0f, 0.0f, 120.0f, 30.0f});
    second->setItems({L"Alpha", L"Beta", L"Gamma"});

    view.add(first);
    view.add(second);

    const oneui::MouseEvent firstClick{oneui::Point{10.0f, 10.0f}};
    const oneui::MouseEvent secondClick{oneui::Point{160.0f, 10.0f}};

    view.onMouseDown(firstClick);
    view.onMouseUp(firstClick);
    expectEqual("First Select opens before second click", first->paintsAboveSiblings() ? 1 : 0, 1);

    view.onMouseDown(secondClick);
    view.onMouseUp(secondClick);

    expectEqual("First Select closes when second opens", first->paintsAboveSiblings() ? 1 : 0, 0);
    expectEqual("First Select outside click preserves selectedIndex", first->selectedIndex(), 1);
    expectEqual("First Select outside click does not emit onChanged", firstChanges, 0);
    expectEqual("Second Select opens after same click", second->paintsAboveSiblings() ? 1 : 0, 1);
}

void testOpenSelectOutsideControlClickPassesThrough() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 260.0f, 180.0f});

    auto select = std::make_shared<oneui::Select>();
    select->setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select->setItems({L"One", L"Two", L"Three"});
    select->setSelectedIndex(1);
    int changes = 0;
    select->setOnChanged([&](int) {
        ++changes;
    });

    auto probe = std::make_shared<MouseUpProbe>();
    probe->setFrame(oneui::Rect{160.0f, 80.0f, 80.0f, 40.0f});

    view.add(select);
    view.add(probe);

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    const oneui::MouseEvent probeClick{oneui::Point{180.0f, 100.0f}};

    view.onMouseDown(fieldClick);
    view.onMouseUp(fieldClick);
    expectEqual("Open Select outside control starts above siblings", select->paintsAboveSiblings() ? 1 : 0, 1);

    view.onMouseDown(probeClick);
    view.onMouseUp(probeClick);

    expectEqual("Open Select outside control closes popup", select->paintsAboveSiblings() ? 1 : 0, 0);
    expectEqual("Open Select outside control preserves selectedIndex", select->selectedIndex(), 1);
    expectEqual("Open Select outside control does not emit onChanged", changes, 0);
    expectEqual("Open Select outside control receives mouse-up", probe->mouseUps, 1);
}

void testSelectKeyboardOpenDoesNotChangeSelection() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.setSelectedIndex(1);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    select.onKeyDown(oneui::KeyEvent{oneui::Key::Space});

    expectEqual("Select Space opens without selection change", select.selectedIndex(), 1);
    expectEqual("Select Space opens without onChanged", changes, 0);
}

void testSelectKeyboardHighlightThenCommitOnce() {
    oneui::State<int> selected(0);
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.bindSelectedIndex(selected);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Down});

    expectEqual("Select Down while open does not commit", selected.get(), 0);
    expectEqual("Select Down while open does not emit", changes, 0);

    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});

    expectEqual("Select Enter commits highlighted option", selected.get(), 1);
    expectEqual("Select Enter emits onChanged once", changes, 1);
}

void testSelectEscapeDismissesWithoutChangingSelection() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.setSelectedIndex(1);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::Point optionPoint{10.0f, 79.0f};
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    expectEqual("Select Escape starts above siblings", select.paintsAboveSiblings() ? 1 : 0, 1);
    expectEqual("Select Escape starts hit-testing dropdown", select.hitTest(optionPoint) ? 1 : 0, 1);

    select.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Escape});

    expectEqual("Select Escape preserves selectedIndex", select.selectedIndex(), 1);
    expectEqual("Select Escape does not emit onChanged", changes, 0);
    expectEqual("Select Escape stops painting above siblings", select.paintsAboveSiblings() ? 1 : 0, 0);
    expectEqual("Select Escape stops hit-testing dropdown", select.hitTest(optionPoint) ? 1 : 0, 0);
}

void testBoundSelectEscapeDismissesWithoutChangingSelection() {
    oneui::State<int> selected(1);
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.bindSelectedIndex(selected);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::Point optionPoint{10.0f, 79.0f};
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    expectEqual("Bound Select Escape starts above siblings", select.paintsAboveSiblings() ? 1 : 0, 1);
    expectEqual("Bound Select Escape starts hit-testing dropdown", select.hitTest(optionPoint) ? 1 : 0, 1);

    select.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Escape});

    expectEqual("Bound Select Escape preserves selectedIndex", selected.get(), 1);
    expectEqual("Bound Select Escape does not emit onChanged", changes, 0);
    expectEqual("Bound Select Escape stops painting above siblings", select.paintsAboveSiblings() ? 1 : 0, 0);
    expectEqual("Bound Select Escape stops hit-testing dropdown", select.hitTest(optionPoint) ? 1 : 0, 0);
}

void testSelectKeyboardOpenClosesWhenFocusLost() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.setSelectedIndex(1);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::Point optionPoint{10.0f, 79.0f};
    select.onFocusChanged(true);
    select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    expectEqual("Select focus loss starts above siblings", select.paintsAboveSiblings() ? 1 : 0, 1);
    expectEqual("Select focus loss starts hit-testing dropdown", select.hitTest(optionPoint) ? 1 : 0, 1);

    select.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    select.onFocusChanged(false);

    expectEqual("Select focus loss preserves selectedIndex", select.selectedIndex(), 1);
    expectEqual("Select focus loss does not emit onChanged", changes, 0);
    expectEqual("Select focus loss stops painting above siblings", select.paintsAboveSiblings() ? 1 : 0, 0);
    expectEqual("Select focus loss stops hit-testing dropdown", select.hitTest(optionPoint) ? 1 : 0, 0);
}

void testSelectLightDismissModelClosesWithoutCommit() {
    const auto expectDismiss = [](const std::string& name, const std::function<bool(oneui::Select&)>& dismiss) {
        oneui::Select select;
        select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
        select.setItems({L"One", L"Two", L"Three"});
        select.setSelectedIndex(1);

        int changes = 0;
        select.setOnChanged([&](int) {
            ++changes;
        });

        select.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
        select.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
        expectEqual((name + " starts open").c_str(), select.paintsAboveSiblings() ? 1 : 0, 1);

        const bool handled = dismiss(select);

        expectEqual((name + " handled flag").c_str(), handled ? 1 : 0, name == "Select outside light-dismiss" ? 0 : 1);
        expectEqual((name + " preserves selection").c_str(), select.selectedIndex(), 1);
        expectEqual((name + " skips onChanged").c_str(), changes, 0);
        expectEqual((name + " closes popup").c_str(), select.paintsAboveSiblings() ? 1 : 0, 0);
        expectEqual((name + " stops option hit-test").c_str(), select.hitTest(oneui::Point{10.0f, 79.0f}) ? 1 : 0, 0);
    };

    expectDismiss("Select outside light-dismiss", [](oneui::Select& select) {
        return select.onMouseDown(oneui::MouseEvent{oneui::Point{200.0f, 100.0f}});
    });
    expectDismiss("Select Escape light-dismiss", [](oneui::Select& select) {
        return select.onKeyDown(oneui::KeyEvent{oneui::Key::Escape});
    });
    expectDismiss("Select focus-loss light-dismiss", [](oneui::Select& select) {
        return select.onFocusChanged(false);
    });
}

void expectOpenSelectCleanup(
    const std::string& name,
    const std::function<void(oneui::Select&)>& setup,
    const std::function<void(oneui::Select&)>& cleanup) {
    oneui::State<int> selected(0);
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.bindSelectedIndex(selected);
    setup(select);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    const oneui::MouseEvent optionClick{oneui::Point{10.0f, 79.0f}};

    select.onMouseDown(fieldClick);
    select.onMouseUp(fieldClick);
    expectEqual((name + " starts above siblings").c_str(), select.paintsAboveSiblings() ? 1 : 0, 1);
    expectEqual((name + " starts hit-testing dropdown").c_str(), select.hitTest(optionClick.position) ? 1 : 0, 1);

    cleanup(select);

    expectEqual((name + " stops painting above siblings").c_str(), select.paintsAboveSiblings() ? 1 : 0, 0);
    expectEqual((name + " stops hit-testing dropdown").c_str(), select.hitTest(optionClick.position) ? 1 : 0, 0);

    select.onMouseDown(optionClick);
    select.onMouseUp(optionClick);

    expectEqual((name + " former dropdown click does not select").c_str(), selected.get(), 0);
    expectEqual((name + " former dropdown click does not emit").c_str(), changes, 0);
}

void testSelectClosesWhenDisabled() {
    expectOpenSelectCleanup(
        "Select setDisabled(true)",
        [](oneui::Select&) {},
        [](oneui::Select& select) {
            select.setDisabled(true);
        });
}

void testSelectDisabledDuringOptionPressClearsPendingCommit() {
    oneui::State<int> selected(0);
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 30.0f});
    select.setItems({L"One", L"Two", L"Three"});
    select.bindSelectedIndex(selected);

    int changes = 0;
    select.setOnChanged([&](int) {
        ++changes;
    });

    const oneui::MouseEvent fieldClick{oneui::Point{10.0f, 10.0f}};
    const oneui::MouseEvent optionClick{oneui::Point{10.0f, 79.0f}};

    select.onMouseDown(fieldClick);
    select.onMouseUp(fieldClick);
    select.onMouseDown(optionClick);
    expectEqual("Select disabled during option press starts above siblings", select.paintsAboveSiblings() ? 1 : 0, 1);

    select.setDisabled(true);
    select.onMouseUp(optionClick);

    expectEqual("Select disabled during option press keeps selection", selected.get(), 0);
    expectEqual("Select disabled during option press skips onChanged", changes, 0);
    expectEqual("Select disabled during option press closes popup", select.paintsAboveSiblings() ? 1 : 0, 0);
    expectEqual("Select disabled during option press stops hit-testing dropdown", select.hitTest(optionClick.position) ? 1 : 0, 0);
}

void testSelectClosesWhenBoundDisabled() {
    oneui::State<bool> disabled(false);
    expectOpenSelectCleanup(
        "Select bound disabled true",
        [&](oneui::Select& select) {
            select.bindDisabled(disabled);
        },
        [&](oneui::Select&) {
            disabled.set(true);
        });
}

void testSelectClosesWhenHidden() {
    expectOpenSelectCleanup(
        "Select setVisible(false)",
        [](oneui::Select&) {},
        [](oneui::Select& select) {
            select.setVisible(false);
        });
}

void testSelectClosesWhenBoundHidden() {
    oneui::State<bool> visible(true);
    expectOpenSelectCleanup(
        "Select bound visible false",
        [&](oneui::Select& select) {
            select.bindVisible(visible);
        },
        [&](oneui::Select&) {
            visible.set(false);
        });
}

void testSelectClosesWhenFocusLost() {
    expectOpenSelectCleanup(
        "Select focus lost",
        [](oneui::Select& select) {
            select.onFocusChanged(true);
        },
        [](oneui::Select& select) {
            select.onFocusChanged(false);
        });
}

void testSelectClosesWhenItemsEmptied() {
    expectOpenSelectCleanup(
        "Select setItems empty",
        [](oneui::Select&) {},
        [](oneui::Select& select) {
            select.setItems({});
        });
}

void testPressedButtonDisabledBeforeMouseUpDoesNotClick() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 80.0f});

    auto button = std::make_shared<oneui::Button>(L"Save");
    button->setFrame(oneui::Rect{10.0f, 10.0f, 100.0f, 36.0f});

    int clicks = 0;
    button->setOnClick([&] {
        ++clicks;
    });

    view.add(button);
    const oneui::MouseEvent event{oneui::Point{20.0f, 20.0f}};

    view.onMouseDown(event);
    button->setDisabled(true);
    view.onMouseUp(event);

    expectEqual("Pressed Button disabled before mouse-up", clicks, 0);
}

void testPressedButtonHiddenBeforeMouseUpDoesNotClick() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 80.0f});

    auto button = std::make_shared<oneui::Button>(L"Save");
    button->setFrame(oneui::Rect{10.0f, 10.0f, 100.0f, 36.0f});

    int clicks = 0;
    button->setOnClick([&] {
        ++clicks;
    });

    view.add(button);
    const oneui::MouseEvent event{oneui::Point{20.0f, 20.0f}};

    view.onMouseDown(event);
    button->setVisible(false);
    view.onMouseUp(event);

    expectEqual("Pressed Button hidden before mouse-up", clicks, 0);
}

void testDisabledPressedChildDoesNotReceiveMouseUp() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 80.0f});

    auto probe = std::make_shared<MouseUpProbe>();
    probe->setFrame(oneui::Rect{10.0f, 10.0f, 100.0f, 36.0f});

    view.add(probe);
    const oneui::MouseEvent event{oneui::Point{20.0f, 20.0f}};

    view.onMouseDown(event);
    probe->setDisabled(true);
    view.onMouseUp(event);

    expectEqual("Disabled pressed child mouse-up dispatch", probe->mouseUps, 0);
}

void testHiddenPressedChildDoesNotReceiveMouseUp() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 80.0f});

    auto probe = std::make_shared<MouseUpProbe>();
    probe->setFrame(oneui::Rect{10.0f, 10.0f, 100.0f, 36.0f});

    view.add(probe);
    const oneui::MouseEvent event{oneui::Point{20.0f, 20.0f}};

    view.onMouseDown(event);
    probe->setVisible(false);
    view.onMouseUp(event);

    expectEqual("Hidden pressed child mouse-up dispatch", probe->mouseUps, 0);
}

void testViewClearChildrenClearsFocusedAndPressedChild() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 96.0f});

    auto button = std::make_shared<oneui::Button>(L"Save");
    button->setFrame(oneui::Rect{10.0f, 10.0f, 96.0f, 36.0f});
    auto probe = std::make_shared<MouseUpProbe>();
    probe->setFrame(oneui::Rect{120.0f, 10.0f, 96.0f, 36.0f});

    view.add(button);
    view.add(probe);
    view.onFocusChanged(true);
    button->setFocusVisible(true);

    const oneui::MouseEvent event{oneui::Point{130.0f, 20.0f}};
    view.onMouseDown(event);
    view.clearChildren();

    expectEqual("View clearChildren removes children", static_cast<int>(view.children().size()), 0);
    expectEqual("View clearChildren clears child focus", button->focused() ? 1 : 0, 0);
    expectEqual("View clearChildren clears child focus-visible", button->focusVisible() ? 1 : 0, 0);
    expectEqual("View clearChildren mouse-up ignored", view.onMouseUp(event) ? 1 : 0, 0);
    expectEqual("View clearChildren pressed child detached", probe->mouseUps, 0);
}

void testViewCanRequestFocusForNestedDescendant() {
    auto root = std::make_shared<oneui::View>();
    auto branch = std::make_shared<oneui::View>();
    auto field = std::make_shared<oneui::TextField>(L"Search");
    branch->add(field);
    root->add(branch);

    expectEqual("View requestFocus finds nested field", root->requestFocus(field.get(), true) ? 1 : 0, 1);
    expectEqual("View requestFocus focuses nested field", field->focused() ? 1 : 0, 1);
    expectEqual("View requestFocus keeps focus visible", field->focusVisible() ? 1 : 0, 1);
    expectEqual("View requestFocus routes text input", root->onTextInput(L'x') ? 1 : 0, 1);
    expectWideEqual("View requestFocus edits nested field", field->text(), L"x");
}

void testViewMouseMoveDoesNotInvalidateSiblingsWhenHoverUnchanged() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 96.0f});

    auto first = std::make_shared<MouseMoveProbe>();
    first->setFrame(oneui::Rect{10.0f, 10.0f, 80.0f, 36.0f});
    auto second = std::make_shared<MouseMoveProbe>();
    second->setFrame(oneui::Rect{110.0f, 10.0f, 80.0f, 36.0f});

    view.add(first);
    view.add(second);

    int invalidations = 0;
    view.setInvalidator([&] {
        ++invalidations;
    });

    const oneui::MouseEvent event{oneui::Point{20.0f, 20.0f}};
    expectEqual("View first hover move is handled", view.onMouseMove(event) ? 1 : 0, 1);
    expectEqual("View first hover move invalidates hovered child", invalidations > 0 ? 1 : 0, 1);

    invalidations = 0;
    expectEqual("View repeated hover move is unchanged", view.onMouseMove(event) ? 1 : 0, 0);
    expectEqual("View repeated hover move does not invalidate siblings", invalidations, 0);
}

void testViewMouseMoveKeepsSingleHoveredChild() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 260.0f, 96.0f});

    auto first = std::make_shared<MouseMoveProbe>();
    first->setFrame(oneui::Rect{10.0f, 10.0f, 80.0f, 36.0f});
    auto second = std::make_shared<MouseMoveProbe>();
    second->setFrame(oneui::Rect{100.0f, 10.0f, 80.0f, 36.0f});
    auto third = std::make_shared<MouseMoveProbe>();
    third->setFrame(oneui::Rect{190.0f, 10.0f, 60.0f, 36.0f});

    view.add(first);
    view.add(second);
    view.add(third);

    view.onMouseMove(oneui::MouseEvent{oneui::Point{20.0f, 20.0f}});
    expectEqual("Only first child hovered initially", first->hovered ? 1 : 0, 1);
    expectEqual("Second child not hovered initially", second->hovered ? 1 : 0, 0);
    expectEqual("Third child not hovered initially", third->hovered ? 1 : 0, 0);

    view.onMouseMove(oneui::MouseEvent{oneui::Point{110.0f, 20.0f}});
    expectEqual("First child hover cleared", first->hovered ? 1 : 0, 0);
    expectEqual("Only second child hovered", second->hovered ? 1 : 0, 1);
    expectEqual("Third child still not hovered", third->hovered ? 1 : 0, 0);

    view.onMouseMove(oneui::MouseEvent{oneui::Point{200.0f, 20.0f}});
    expectEqual("Second child hover cleared", second->hovered ? 1 : 0, 0);
    expectEqual("Only third child hovered", third->hovered ? 1 : 0, 1);
}

void testViewMouseMoveSweepInvalidatesOnlyExitAndEnter() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 96.0f});

    for (int index = 0; index < 4; ++index) {
        auto probe = std::make_shared<MouseMoveProbe>();
        probe->setFrame(oneui::Rect{10.0f + static_cast<float>(index) * 76.0f, 10.0f, 64.0f, 36.0f});
        view.add(probe);
    }

    int invalidations = 0;
    view.setInvalidator([&] {
        ++invalidations;
    });

    view.onMouseMove(oneui::MouseEvent{oneui::Point{20.0f, 20.0f}});
    expectEqual("Sweep first enter invalidations", invalidations, 1);

    invalidations = 0;
    view.onMouseMove(oneui::MouseEvent{oneui::Point{96.0f, 20.0f}});
    expectEqual("Sweep second enter invalidates exit and enter only", invalidations, 2);

    invalidations = 0;
    view.onMouseMove(oneui::MouseEvent{oneui::Point{172.0f, 20.0f}});
    expectEqual("Sweep third enter invalidates exit and enter only", invalidations, 2);

    invalidations = 0;
    expectEqual("Sweep leaving controls reports hover exit", view.onMouseMove(oneui::MouseEvent{oneui::Point{300.0f, 80.0f}}) ? 1 : 0, 1);
    expectEqual("Sweep leaving controls invalidates previous only", invalidations, 1);

    invalidations = 0;
    expectEqual("Sweep repeated blank move is unchanged", view.onMouseMove(oneui::MouseEvent{oneui::Point{300.0f, 80.0f}}) ? 1 : 0, 0);
    expectEqual("Sweep repeated blank move does not invalidate", invalidations, 0);
}

void testViewPropagatesChildDirtyRectWithoutFullInvalidation() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 120.0f});

    auto probe = std::make_shared<PaintProbe>();
    probe->setFrame(oneui::Rect{24.0f, 32.0f, 88.0f, 40.0f});
    view.add(probe);

    int fullInvalidations = 0;
    std::vector<oneui::Rect> dirtyRects;
    view.setInvalidator([&] {
        ++fullInvalidations;
    });
    view.setRectInvalidator([&](oneui::Rect rect) {
        dirtyRects.push_back(rect);
    });

    probe->requestInvalidation();

    expectEqual("Child dirty rect avoids full invalidation", fullInvalidations, 0);
    expectEqual("Child dirty rect propagated once", static_cast<int>(dirtyRects.size()), 1);
    if (!dirtyRects.empty()) {
        expectRect("Child dirty rect value", dirtyRects.front(), oneui::Rect{24.0f, 32.0f, 88.0f, 40.0f});
    }
}

void testViewPaintSkipsChildrenOutsideClipBounds() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 160.0f});

    auto first = std::make_shared<PaintProbe>();
    first->setFrame(oneui::Rect{10.0f, 10.0f, 80.0f, 40.0f});
    auto second = std::make_shared<PaintProbe>();
    second->setFrame(oneui::Rect{120.0f, 10.0f, 80.0f, 40.0f});
    auto third = std::make_shared<PaintProbe>();
    third->setFrame(oneui::Rect{230.0f, 10.0f, 80.0f, 40.0f});

    view.add(first);
    view.add(second);
    view.add(third);

    RecordingCanvas canvas;
    canvas.clipOverride = oneui::Rect{116.0f, 6.0f, 92.0f, 52.0f};
    view.paint(canvas);

    expectEqual("Clip culling skips first sibling", first->paintCalls, 0);
    expectEqual("Clip culling paints intersecting sibling", second->paintCalls, 1);
    expectEqual("Clip culling skips third sibling", third->paintCalls, 0);
}

void testViewCursorDelegatesToTopmostInteractiveChild() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 120.0f});

    auto textField = std::make_shared<oneui::TextField>(L"Name");
    textField->setFrame(oneui::Rect{10.0f, 10.0f, 120.0f, 36.0f});
    auto button = std::make_shared<oneui::Button>(L"Connect");
    button->setFrame(oneui::Rect{150.0f, 10.0f, 96.0f, 36.0f});

    view.add(textField);
    view.add(button);

    expectEqual("View text field cursor", static_cast<int>(view.cursor(oneui::Point{20.0f, 20.0f})), static_cast<int>(oneui::CursorKind::Text));
    expectEqual("View button cursor", static_cast<int>(view.cursor(oneui::Point{160.0f, 20.0f})), static_cast<int>(oneui::CursorKind::Pointer));
    expectEqual("View empty cursor", static_cast<int>(view.cursor(oneui::Point{280.0f, 80.0f})), static_cast<int>(oneui::CursorKind::Default));

    textField->setReadOnly(true);
    expectEqual("Read-only text field cursor", static_cast<int>(view.cursor(oneui::Point{20.0f, 20.0f})), static_cast<int>(oneui::CursorKind::Default));
}

void testFormFieldCursorDelegatesToEditableChild() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 140.0f});

    auto field = std::make_shared<oneui::FormField>();
    field->setLabel(L"Device code");
    field->setFrame(oneui::Rect{10.0f, 10.0f, 180.0f, 70.0f});
    auto textField = std::make_shared<oneui::TextField>(L"Enter code");
    field->setChild(textField);
    view.add(field);

    expectEqual("FormField label cursor remains default", static_cast<int>(view.cursor(oneui::Point{18.0f, 16.0f})), static_cast<int>(oneui::CursorKind::Default));
    expectEqual("FormField editable child cursor", static_cast<int>(view.cursor(oneui::Point{18.0f, 42.0f})), static_cast<int>(oneui::CursorKind::Text));

    textField->setReadOnly(true);
    expectEqual("FormField read-only child cursor", static_cast<int>(view.cursor(oneui::Point{18.0f, 42.0f})), static_cast<int>(oneui::CursorKind::Default));
}

void testOverlayHostCursorDelegatesToContentAndOverlay() {
    oneui::OverlayHost host;
    host.setFrame(oneui::Rect{0.0f, 0.0f, 420.0f, 180.0f});

    auto content = std::make_shared<oneui::View>();
    content->setFrame(oneui::Rect{0.0f, 0.0f, 420.0f, 180.0f});
    auto textField = std::make_shared<oneui::TextField>(L"Device code");
    textField->setFrame(oneui::Rect{24.0f, 24.0f, 220.0f, 36.0f});
    auto button = std::make_shared<oneui::Button>(L"Connect");
    button->setFrame(oneui::Rect{260.0f, 24.0f, 96.0f, 36.0f});
    content->add(textField);
    content->add(button);
    host.setContent(content);

    expectEqual("OverlayHost content text field cursor", static_cast<int>(host.cursor(oneui::Point{32.0f, 36.0f})), static_cast<int>(oneui::CursorKind::Text));
    expectEqual("OverlayHost content button cursor", static_cast<int>(host.cursor(oneui::Point{280.0f, 36.0f})), static_cast<int>(oneui::CursorKind::Pointer));

    auto overlay = std::make_shared<oneui::Button>(L"Overlay");
    overlay->setFrame(oneui::Rect{20.0f, 100.0f, 120.0f, 36.0f});
    host.addOverlay(overlay, 1);
    expectEqual("OverlayHost overlay cursor", static_cast<int>(host.cursor(oneui::Point{32.0f, 112.0f})), static_cast<int>(oneui::CursorKind::Pointer));
}

void testClearInteractionStateSkipsIdleInteractiveControls() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 120.0f});

    auto first = std::make_shared<oneui::Button>(L"First");
    first->setFrame(oneui::Rect{10.0f, 10.0f, 96.0f, 36.0f});
    auto second = std::make_shared<oneui::Button>(L"Second");
    second->setFrame(oneui::Rect{120.0f, 10.0f, 96.0f, 36.0f});
    view.add(first);
    view.add(second);

    int invalidations = 0;
    view.setInvalidator([&] {
        ++invalidations;
    });

    first->clearInteractionState();
    expectEqual("Idle button clearInteractionState skips invalidation", invalidations, 0);

    view.onMouseMove(oneui::MouseEvent{oneui::Point{20.0f, 20.0f}});
    invalidations = 0;
    first->clearInteractionState();
    expectEqual("Hovered button clearInteractionState invalidates once", invalidations, 1);
}

void testViewPropagatesAnimationSchedulerToInteractiveChildren() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 96.0f});

    int scheduledFrames = 0;
    view.setAnimationScheduler([&] {
        ++scheduledFrames;
    });

    auto button = std::make_shared<oneui::Button>(L"Save");
    button->setFrame(oneui::Rect{10.0f, 10.0f, 96.0f, 36.0f});
    view.add(button);

    expectEqual("View animation scheduler initially idle", scheduledFrames, 0);
    expectEqual("Button hover starts animation through View", view.onMouseMove(oneui::MouseEvent{oneui::Point{20.0f, 20.0f}}) ? 1 : 0, 1);
    expectEqual("View animation scheduler reached child", scheduledFrames > 0 ? 1 : 0, 1);
}

void testCompositeControlsUpdateBaseFocusState() {
    oneui::Tile tile(L"Recent", L"123");
    tile.onFocusChanged(true);
    expectEqual("Tile focus changed sets base focus", tile.focused() ? 1 : 0, 1);
    tile.onFocusChanged(false);
    expectEqual("Tile focus changed clears base focus", tile.focused() ? 1 : 0, 0);

    oneui::Toast toast(L"Notice", L"Message");
    toast.onFocusChanged(true);
    expectEqual("Toast focus changed sets base focus", toast.focused() ? 1 : 0, 1);
    toast.onFocusChanged(false);
    expectEqual("Toast focus changed clears base focus", toast.focused() ? 1 : 0, 0);

    oneui::StatusStrip strip(L"Status", L"Ready");
    strip.onFocusChanged(true);
    expectEqual("StatusStrip focus changed sets base focus", strip.focused() ? 1 : 0, 1);
    strip.onFocusChanged(false);
    expectEqual("StatusStrip focus changed clears base focus", strip.focused() ? 1 : 0, 0);
}

void testButtonMouseFocusIsNotFocusVisible() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 80.0f});

    auto button = std::make_shared<oneui::Button>(L"Save");
    button->setFrame(oneui::Rect{10.0f, 10.0f, 100.0f, 36.0f});
    view.add(button);

    const oneui::MouseEvent event{oneui::Point{20.0f, 20.0f}};
    view.onMouseDown(event);

    expectEqual("Button mouse focus focused", button->focused() ? 1 : 0, 1);
    expectEqual("Button mouse focus is not focus-visible", button->focusVisible() ? 1 : 0, 0);
}

void testButtonKeyboardFocusVisibleAndActivates() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 260.0f, 80.0f});

    auto first = std::make_shared<oneui::Button>(L"First");
    first->setFrame(oneui::Rect{10.0f, 10.0f, 100.0f, 36.0f});
    auto second = std::make_shared<oneui::Button>(L"Second");
    second->setFrame(oneui::Rect{120.0f, 10.0f, 100.0f, 36.0f});

    int clicks = 0;
    second->setOnClick([&] {
        ++clicks;
    });

    view.add(first);
    view.add(second);
    view.onFocusChanged(true);

    expectEqual("Initial window focus is not focus-visible", first->focusVisible() ? 1 : 0, 0);

    view.onKeyDown(oneui::KeyEvent{oneui::Key::Tab});

    expectEqual("Button Tab focus focused", second->focused() ? 1 : 0, 1);
    expectEqual("Button Tab focus is focus-visible", second->focusVisible() ? 1 : 0, 1);

    view.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});

    expectEqual("Keyboard focused Button Enter click", clicks, 1);

    view.onFocusChanged(false);

    expectEqual("Button focus lost clears focused", second->focused() ? 1 : 0, 0);
    expectEqual("Button focus lost clears focus-visible", second->focusVisible() ? 1 : 0, 0);
}

void testButtonStyleOverridePaintsCustomColors() {
    oneui::Button button(L"Save");
    button.setFrame(oneui::Rect{0.0f, 0.0f, 96.0f, 36.0f});

    const oneui::Color background{12, 34, 56};
    const oneui::Color foreground{230, 240, 250};
    const oneui::Color border{98, 76, 54};
    oneui::ButtonStyleOverride style;
    oneui::ButtonStateStyleOverride normal;
    normal.background = background;
    normal.foreground = foreground;
    normal.border = border;
    normal.borderWidth = 3.0f;
    normal.shadows = std::vector<oneui::ControlShadowStyle>{
        oneui::ControlShadowStyle{oneui::Color{0, 0, 0, 72}, oneui::Point{0.0f, 4.0f}, 12.0f, 0.0f, false}};
    style.normal = normal;
    button.setStyleOverride(style);

    RecordingCanvas canvas;
    button.paint(canvas);

    expectEqual("Button style override background", countFillRectsWithColor(canvas, background), 1);
    expectEqual("Button style override border", countStrokeRectsWithColor(canvas, border), 1);
    expectEqual("Button style override shadow", static_cast<int>(canvas.boxShadows.size()), 1);
    expectEqual("Button style override text", countTextsWithColor(canvas, foreground), 1);
}

void testButtonEmptyStyleOverrideKeepsDefaultPaint() {
    oneui::Button button(L"Save");
    button.setFrame(oneui::Rect{0.0f, 0.0f, 96.0f, 36.0f});
    button.setStyleOverride(oneui::ButtonStyleOverride{});

    RecordingCanvas canvas;
    button.paint(canvas);

    expectEqual("Button empty style override keeps primary background", countFillRectsWithColor(canvas, oneui::colors::Primary), 1);
    expectEqual("Button empty style override keeps white text", countTextsWithColor(canvas, oneui::colors::White), 1);
}

void testButtonStyleOverrideCanHideFocusRing() {
    oneui::Button button(L"Save");
    button.setFrame(oneui::Rect{0.0f, 0.0f, 96.0f, 36.0f});
    button.onFocusChanged(true);
    button.setFocusVisible(true);

    oneui::ButtonStyleOverride style;
    oneui::ButtonStateStyleOverride focus;
    oneui::FocusRingStyleOverride ring;
    ring.visible = false;
    focus.focusRing = ring;
    style.focusVisible = focus;
    button.setStyleOverride(style);

    RecordingCanvas canvas;
    button.paint(canvas);

    expectEqual("Button style override hides focus ring", countPrimaryOrThickStrokeRects(canvas), 1);
}

void testCheckboxMouseFocusIsNotFocusVisible() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 80.0f});

    auto checkbox = std::make_shared<oneui::Checkbox>(L"Receive updates");
    checkbox->setFrame(oneui::Rect{10.0f, 10.0f, 180.0f, 28.0f});
    view.add(checkbox);

    view.onMouseDown(oneui::MouseEvent{oneui::Point{20.0f, 20.0f}});

    expectEqual("Checkbox mouse focus focused", checkbox->focused() ? 1 : 0, 1);
    expectEqual("Checkbox mouse focus is not focus-visible", checkbox->focusVisible() ? 1 : 0, 0);
}

void testCheckboxKeyboardFocusVisible() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 260.0f, 100.0f});

    auto first = std::make_shared<oneui::Checkbox>(L"First");
    first->setFrame(oneui::Rect{10.0f, 10.0f, 180.0f, 28.0f});
    auto second = std::make_shared<oneui::Checkbox>(L"Second");
    second->setFrame(oneui::Rect{10.0f, 48.0f, 180.0f, 28.0f});

    view.add(first);
    view.add(second);
    view.onFocusChanged(true);

    view.onKeyDown(oneui::KeyEvent{oneui::Key::Tab});

    expectEqual("Checkbox Tab focus focused", second->focused() ? 1 : 0, 1);
    expectEqual("Checkbox Tab focus is focus-visible", second->focusVisible() ? 1 : 0, 1);
}

void testCheckboxStyleOverridePaintsCustomColors() {
    oneui::Checkbox checkbox(L"Agree");
    checkbox.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 28.0f});
    checkbox.setChecked(true);

    const oneui::Color background{44, 88, 132};
    const oneui::Color border{155, 99, 11};
    const oneui::Color label{18, 52, 86};
    oneui::CheckboxStyleOverride style;
    oneui::CheckboxStateStyleOverride selected;
    selected.boxBackground = background;
    selected.boxBorder = border;
    selected.labelColor = label;
    style.selected = selected;
    checkbox.setStyleOverride(style);

    RecordingCanvas canvas;
    checkbox.paint(canvas);

    expectEqual("Checkbox selected style background", countFillRectsWithColor(canvas, background), 1);
    expectEqual("Checkbox selected style border", countStrokeRectsWithColor(canvas, border), 1);
    expectEqual("Checkbox selected style label", countTextsWithColor(canvas, label), 1);
}

void testCheckboxStyleOverrideCanHideFocusRing() {
    oneui::Checkbox checkbox(L"Agree");
    checkbox.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 28.0f});
    checkbox.onFocusChanged(true);
    checkbox.setFocusVisible(true);

    oneui::CheckboxStyleOverride style;
    oneui::CheckboxStateStyleOverride focus;
    oneui::FocusRingStyleOverride ring;
    ring.visible = false;
    focus.focusRing = ring;
    style.focusVisible = focus;
    checkbox.setStyleOverride(style);

    RecordingCanvas canvas;
    checkbox.paint(canvas);

    expectEqual("Checkbox style override hides focus ring", countPrimaryOrThickStrokeRects(canvas), 0);
}

void testRadioGroupStyleOverridePaintsCustomColorsAndGeometry() {
    oneui::RadioGroup group;
    group.setItems({L"Compact", L"Balanced"});
    group.setSelectedIndex(1);
    group.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 56.0f});

    const oneui::Color hoverBackground{241, 245, 249};
    const oneui::Color border{91, 33, 182};
    const oneui::Color fill{16, 185, 129};
    const oneui::Color label{51, 65, 85};
    const oneui::Color selectedLabel{15, 23, 42};
    oneui::RadioGroupStyleOverride style;
    oneui::RadioGroupStateStyleOverride normal;
    normal.labelColor = label;
    normal.indicatorSize = 18.0f;
    normal.indicatorDotSize = 6.0f;
    normal.indicatorInset = 12.0f;
    normal.labelGap = 8.0f;
    style.normal = normal;
    oneui::RadioGroupStateStyleOverride hovered;
    hovered.itemBackground = hoverBackground;
    style.hovered = hovered;
    oneui::RadioGroupStateStyleOverride selected;
    selected.indicatorBorder = border;
    selected.indicatorFill = fill;
    selected.selectedLabelColor = selectedLabel;
    style.selected = selected;
    group.setStyleOverride(style);

    group.onMouseMove(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});

    RecordingCanvas canvas;
    group.paint(canvas);

    expectEqual("RadioGroup hovered item background", countFillRectsWithColor(canvas, hoverBackground), 1);
    expectEqual("RadioGroup selected indicator border", countStrokeEllipsesWithColor(canvas, border), 1);
    expectEqual("RadioGroup selected indicator fill", countFillEllipsesWithColor(canvas, fill), 1);
    expectEqual("RadioGroup normal label color", countTextsWithTextAndColor(canvas, L"Compact", label), 1);
    expectEqual("RadioGroup selected label color", countTextsWithTextAndColor(canvas, L"Balanced", selectedLabel), 1);
    if (canvas.strokeEllipses.size() >= 2 && canvas.texts.size() >= 2) {
        expectRect("RadioGroup indicator geometry follows style", canvas.strokeEllipses[1].rect, oneui::Rect{12.0f, 33.0f, 18.0f, 18.0f});
        expectRect("RadioGroup label geometry follows style", canvas.texts[1].rect, oneui::Rect{38.0f, 28.0f, 142.0f, 28.0f});
    }
}

void testRadioGroupHorizontalOrientationLaysOutColumns() {
    oneui::RadioGroup group;
    group.setItems({L"Remote desktop", L"Remote file"});
    group.setOrientation(oneui::RadioGroup::Orientation::Horizontal);
    group.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 32.0f});

    group.onMouseDown(oneui::MouseEvent{oneui::Point{150.0f, 16.0f}});
    group.onMouseUp(oneui::MouseEvent{oneui::Point{150.0f, 16.0f}});

    expectEqual("RadioGroup horizontal hit second column", group.selectedIndex(), 1);
}

void testRadioGroupEmptyStyleOverrideKeepsDefaultPaint() {
    oneui::RadioGroup group;
    group.setItems({L"Compact", L"Balanced"});
    group.setSelectedIndex(1);
    group.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 56.0f});
    group.setStyleOverride(oneui::RadioGroupStyleOverride{});

    RecordingCanvas canvas;
    group.paint(canvas);

    expectEqual("RadioGroup empty style override keeps selected border", countStrokeEllipsesWithColor(canvas, oneui::theme().primary), 1);
    expectEqual("RadioGroup empty style override keeps selected fill", countFillEllipsesWithColor(canvas, oneui::theme().primary), 1);
    expectEqual("RadioGroup empty style override keeps label color", countTextsWithColor(canvas, oneui::theme().textMuted), 2);
}

void testRadioGroupStyleOverrideCanHideFocusRingAndStylePressed() {
    oneui::RadioGroup group;
    group.setItems({L"Compact", L"Balanced"});
    group.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 56.0f});
    group.onFocusChanged(true);
    group.setFocusVisible(true);

    const oneui::Color pressedBackground{224, 231, 255};
    oneui::RadioGroupStyleOverride style;
    oneui::RadioGroupStateStyleOverride pressed;
    pressed.itemBackground = pressedBackground;
    style.pressed = pressed;
    oneui::RadioGroupStateStyleOverride focus;
    oneui::FocusRingStyleOverride ring;
    ring.visible = false;
    focus.focusRing = ring;
    style.focusVisible = focus;
    group.setStyleOverride(style);

    group.onMouseDown(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});

    RecordingCanvas canvas;
    group.paint(canvas);

    expectEqual("RadioGroup pressed style background", countFillRectsWithColor(canvas, pressedBackground), 1);
    expectEqual("RadioGroup style override hides focus ring", countPrimaryOrThickStrokeRects(canvas), 0);
}

void testRadioGroupDisabledStyleOverrideWinsAndClearRestoresDefault() {
    oneui::RadioGroup group;
    group.setItems({L"Compact", L"Balanced"});
    group.setSelectedIndex(1);
    group.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 56.0f});

    const oneui::Color hoveredBackground{219, 234, 254};
    const oneui::Color disabledBorder{12, 34, 56};
    const oneui::Color disabledLabel{100, 116, 139};
    oneui::RadioGroupStyleOverride style;
    oneui::RadioGroupStateStyleOverride hovered;
    hovered.itemBackground = hoveredBackground;
    style.hovered = hovered;
    oneui::RadioGroupStateStyleOverride disabled;
    disabled.indicatorBorder = disabledBorder;
    disabled.labelColor = disabledLabel;
    disabled.selectedLabelColor = disabledLabel;
    style.disabled = disabled;
    group.setStyleOverride(style);

    group.onMouseMove(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});
    group.setDisabled(true);

    RecordingCanvas disabledCanvas;
    group.paint(disabledCanvas);

    expectEqual("RadioGroup disabled style border wins", countStrokeEllipsesWithColor(disabledCanvas, disabledBorder), 2);
    expectEqual("RadioGroup hovered style ignored while disabled", countFillRectsWithColor(disabledCanvas, hoveredBackground), 0);
    expectEqual("RadioGroup disabled label wins", countTextsWithColor(disabledCanvas, disabledLabel), 2);

    group.setDisabled(false);
    group.clearStyleOverride();
    RecordingCanvas defaultCanvas;
    group.paint(defaultCanvas);

    expectEqual("RadioGroup clearStyleOverride restores default selected fill", countFillEllipsesWithColor(defaultCanvas, oneui::theme().primary), 1);
    expectEqual("RadioGroup clearStyleOverride removes disabled border", countStrokeEllipsesWithColor(defaultCanvas, disabledBorder), 0);
}

void testTabsStyleOverridePaintsCustomColorsAndGeometry() {
    oneui::Tabs tabs;
    tabs.setItems({L"Props", L"State", L"Events"});
    tabs.setSelectedIndex(1);
    tabs.setFrame(oneui::Rect{0.0f, 0.0f, 300.0f, 30.0f});

    const oneui::Color background{15, 23, 42};
    const oneui::Color border{71, 85, 105};
    const oneui::Color normalText{203, 213, 225};
    const oneui::Color hoverBackground{30, 41, 59};
    const oneui::Color selectedBackground{219, 234, 254};
    const oneui::Color selectedForeground{37, 99, 235};
    const oneui::Color selectedBorder{96, 165, 250};

    oneui::TabsStyleOverride style;
    oneui::TabsStateStyleOverride normal;
    normal.background = background;
    normal.border = border;
    normal.itemForeground = normalText;
    normal.itemInset = oneui::Insets{4.0f};
    style.normal = normal;
    oneui::TabsStateStyleOverride hovered;
    hovered.itemBackground = hoverBackground;
    style.hovered = hovered;
    oneui::TabsStateStyleOverride selected;
    selected.selectedItemBackground = selectedBackground;
    selected.selectedItemForeground = selectedForeground;
    selected.selectedItemBorder = selectedBorder;
    style.selected = selected;
    tabs.setStyleOverride(style);

    tabs.onMouseMove(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});

    RecordingCanvas canvas;
    tabs.paint(canvas);

    expectEqual("Tabs style override container background", countFillRectsWithColor(canvas, background), 1);
    expectEqual("Tabs style override container border", countStrokeRectsWithColor(canvas, border), 1);
    expectEqual("Tabs style override hovered item", countFillRectsWithColor(canvas, hoverBackground), 1);
    expectEqual("Tabs style override selected item", countFillRectsWithColor(canvas, selectedBackground), 1);
    expectEqual("Tabs style override selected border", countStrokeRectsWithColor(canvas, selectedBorder), 1);
    expectEqual("Tabs style override normal text", countTextsWithTextAndColor(canvas, L"Props", normalText), 1);
    expectEqual("Tabs style override selected text", countTextsWithTextAndColor(canvas, L"State", selectedForeground), 1);
    if (canvas.fillRects.size() >= 3) {
        expectRect("Tabs selected item geometry follows itemInset", canvas.fillRects[2].rect, oneui::Rect{104.0f, 4.0f, 92.0f, 22.0f});
    }
}

void testTabsEmptyStyleOverrideKeepsDefaultPaint() {
    oneui::Tabs tabs;
    tabs.setItems({L"Props", L"State"});
    tabs.setSelectedIndex(1);
    tabs.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 30.0f});
    tabs.setStyleOverride(oneui::TabsStyleOverride{});

    RecordingCanvas canvas;
    tabs.paint(canvas);

    expectEqual("Tabs empty style override keeps container background", countFillRectsWithColor(canvas, oneui::Color{241, 245, 249}), 1);
    expectEqual("Tabs empty style override keeps selected background", countFillRectsWithColor(canvas, oneui::theme().surface), 1);
    expectEqual("Tabs empty style override keeps selected text", countTextsWithTextAndColor(canvas, L"State", oneui::theme().primary), 1);
}

void testTabsStyleOverrideCanHideFocusRingAndStylePressed() {
    oneui::Tabs tabs;
    tabs.setItems({L"Props", L"State"});
    tabs.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 30.0f});
    tabs.onFocusChanged(true);
    tabs.setFocusVisible(true);

    const oneui::Color pressedBackground{49, 46, 129};
    oneui::TabsStyleOverride style;
    oneui::TabsStateStyleOverride pressed;
    pressed.itemBackground = pressedBackground;
    style.pressed = pressed;
    oneui::TabsStateStyleOverride focus;
    oneui::FocusRingStyleOverride ring;
    ring.visible = false;
    focus.focusRing = ring;
    style.focusVisible = focus;
    tabs.setStyleOverride(style);

    tabs.onMouseDown(oneui::MouseEvent{oneui::Point{150.0f, 10.0f}});

    RecordingCanvas canvas;
    tabs.paint(canvas);

    expectEqual("Tabs pressed style background", countFillRectsWithColor(canvas, pressedBackground), 1);
    expectEqual("Tabs style override hides focus ring", countPrimaryOrThickStrokeRects(canvas), 0);
}

void testTabsDisabledStyleOverrideWinsAndClearRestoresDefault() {
    oneui::Tabs tabs;
    tabs.setItems({L"Props", L"State"});
    tabs.setSelectedIndex(1);
    tabs.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 30.0f});

    const oneui::Color hoveredBackground{224, 231, 255};
    const oneui::Color disabledBackground{15, 23, 42};
    const oneui::Color disabledText{148, 163, 184};
    oneui::TabsStyleOverride style;
    oneui::TabsStateStyleOverride hovered;
    hovered.itemBackground = hoveredBackground;
    style.hovered = hovered;
    oneui::TabsStateStyleOverride disabled;
    disabled.background = disabledBackground;
    disabled.itemBackground = disabledBackground;
    disabled.itemForeground = disabledText;
    disabled.selectedItemForeground = disabledText;
    style.disabled = disabled;
    tabs.setStyleOverride(style);

    tabs.onMouseMove(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});
    tabs.setDisabled(true);

    RecordingCanvas disabledCanvas;
    tabs.paint(disabledCanvas);

    expectEqual("Tabs disabled style background wins", countFillRectsWithColor(disabledCanvas, disabledBackground), 2);
    expectEqual("Tabs hovered style ignored while disabled", countFillRectsWithColor(disabledCanvas, hoveredBackground), 0);
    expectEqual("Tabs disabled style foreground wins", countTextsWithColor(disabledCanvas, disabledText), 2);

    tabs.setDisabled(false);
    tabs.clearStyleOverride();
    RecordingCanvas defaultCanvas;
    tabs.paint(defaultCanvas);

    expectEqual("Tabs clearStyleOverride restores selected background", countFillRectsWithColor(defaultCanvas, oneui::theme().surface), 1);
    expectEqual("Tabs clearStyleOverride removes disabled background", countFillRectsWithColor(defaultCanvas, disabledBackground), 0);
}

void testListStyleOverridePaintsCustomColorsAndGeometry() {
    oneui::List list;
    list.setItems({
        oneui::ListItem{L"Acme", L"Live"},
        oneui::ListItem{L"Billing", L"Review"},
    });
    list.setSelectedIndex(1);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 88.0f});

    const oneui::Color background{15, 23, 42};
    const oneui::Color border{71, 85, 105};
    const oneui::Color separator{51, 65, 85};
    const oneui::Color title{203, 213, 225};
    const oneui::Color detail{148, 163, 184};
    const oneui::Color hoverBackground{30, 41, 59};
    const oneui::Color selectedBackground{219, 234, 254};
    const oneui::Color selectedTitle{37, 99, 235};
    const oneui::Color selectedDetail{22, 101, 52};

    oneui::ListStyleOverride style;
    oneui::ListStateStyleOverride normal;
    normal.background = background;
    normal.border = border;
    normal.separator = separator;
    normal.titleColor = title;
    normal.detailColor = detail;
    normal.rowInset = oneui::Insets{4.0f, 6.0f};
    normal.textInset = 14.0f;
    normal.titleOffsetY = 8.0f;
    normal.detailOffsetY = 28.0f;
    style.normal = normal;
    oneui::ListStateStyleOverride hovered;
    hovered.rowBackground = hoverBackground;
    style.hovered = hovered;
    oneui::ListStateStyleOverride selected;
    selected.selectedRowBackground = selectedBackground;
    selected.selectedTitleColor = selectedTitle;
    selected.selectedDetailColor = selectedDetail;
    style.selected = selected;
    list.setStyleOverride(style);

    list.onMouseMove(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});

    RecordingCanvas canvas;
    list.paint(canvas);

    expectEqual("List style override container background", countFillRectsWithColor(canvas, background), 1);
    expectEqual("List style override container border", countStrokeRectsWithColor(canvas, border), 1);
    expectEqual("List style override separator", countLinesWithColor(canvas, separator), 1);
    expectEqual("List style override hovered row", countFillRectsWithColor(canvas, hoverBackground), 1);
    expectEqual("List style override selected row", countFillRectsWithColor(canvas, selectedBackground), 1);
    expectEqual("List style override normal title", countTextsWithTextAndColor(canvas, L"Acme", title), 1);
    expectEqual("List style override normal detail", countTextsWithTextAndColor(canvas, L"Live", detail), 1);
    expectEqual("List style override selected title", countTextsWithTextAndColor(canvas, L"Billing", selectedTitle), 1);
    expectEqual("List style override selected detail", countTextsWithTextAndColor(canvas, L"Review", selectedDetail), 1);
    if (canvas.fillRects.size() >= 3 && canvas.texts.size() >= 4) {
        expectRect("List selected row geometry follows rowInset", canvas.fillRects[2].rect, oneui::Rect{6.0f, 48.0f, 148.0f, 36.0f});
        expectRect("List title geometry follows textInset", canvas.texts[2].rect, oneui::Rect{14.0f, 52.0f, 132.0f, 18.0f});
    }
}

void testListEmptyStyleOverrideKeepsDefaultPaint() {
    oneui::List list;
    list.setItems({
        oneui::ListItem{L"Acme", L"Live"},
        oneui::ListItem{L"Billing", L"Review"},
    });
    list.setSelectedIndex(1);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 88.0f});
    list.setStyleOverride(oneui::ListStyleOverride{});

    RecordingCanvas canvas;
    list.paint(canvas);

    expectEqual("List empty style override keeps container background", countFillRectsWithColor(canvas, oneui::theme().surface), 1);
    expectEqual("List empty style override keeps selected row", countFillRectsWithColor(canvas, oneui::theme().primarySoft), 1);
    expectEqual("List empty style override keeps selected title", countTextsWithTextAndColor(canvas, L"Billing", oneui::theme().primary), 1);
}

void testListStyleOverrideCanHideFocusRingAndStylePressed() {
    oneui::List list;
    list.setItems({
        oneui::ListItem{L"Acme", L"Live"},
        oneui::ListItem{L"Billing", L"Review"},
    });
    list.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 88.0f});
    list.onFocusChanged(true);
    list.setFocusVisible(true);

    const oneui::Color pressedBackground{49, 46, 129};
    oneui::ListStyleOverride style;
    oneui::ListStateStyleOverride pressed;
    pressed.rowBackground = pressedBackground;
    style.pressed = pressed;
    oneui::ListStateStyleOverride focus;
    oneui::FocusRingStyleOverride ring;
    ring.visible = false;
    focus.focusRing = ring;
    style.focusVisible = focus;
    list.setStyleOverride(style);

    list.onMouseDown(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});

    RecordingCanvas canvas;
    list.paint(canvas);

    expectEqual("List pressed style background", countFillRectsWithColor(canvas, pressedBackground), 1);
    expectEqual("List style override hides focus ring", countPrimaryOrThickStrokeRects(canvas), 0);
}

void testListDisabledStyleOverrideWinsAndClearRestoresDefault() {
    oneui::List list;
    list.setItems({
        oneui::ListItem{L"Acme", L"Live"},
        oneui::ListItem{L"Billing", L"Review"},
    });
    list.setSelectedIndex(1);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 88.0f});

    const oneui::Color hoveredBackground{224, 231, 255};
    const oneui::Color disabledBackground{15, 23, 42};
    const oneui::Color disabledText{148, 163, 184};
    oneui::ListStyleOverride style;
    oneui::ListStateStyleOverride hovered;
    hovered.rowBackground = hoveredBackground;
    style.hovered = hovered;
    oneui::ListStateStyleOverride disabled;
    disabled.background = disabledBackground;
    disabled.rowBackground = disabledBackground;
    disabled.titleColor = disabledText;
    disabled.detailColor = disabledText;
    disabled.selectedTitleColor = disabledText;
    disabled.selectedDetailColor = disabledText;
    style.disabled = disabled;
    list.setStyleOverride(style);

    list.onMouseMove(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});
    list.setDisabled(true);

    RecordingCanvas disabledCanvas;
    list.paint(disabledCanvas);

    expectEqual("List disabled style background wins", countFillRectsWithColor(disabledCanvas, disabledBackground), 3);
    expectEqual("List hovered style ignored while disabled", countFillRectsWithColor(disabledCanvas, hoveredBackground), 0);
    expectEqual("List disabled style text wins", countTextsWithColor(disabledCanvas, disabledText), 4);

    list.setDisabled(false);
    list.clearStyleOverride();
    RecordingCanvas defaultCanvas;
    list.paint(defaultCanvas);

    expectEqual("List clearStyleOverride restores selected row", countFillRectsWithColor(defaultCanvas, oneui::theme().primarySoft), 1);
    expectEqual("List clearStyleOverride removes disabled background", countFillRectsWithColor(defaultCanvas, disabledBackground), 0);
}

void testVirtualListPaintsOnlyViewportRowsAndMaintainsScrollSelection() {
    std::vector<oneui::ListItem> items;
    items.reserve(5000);
    for (int index = 0; index < 5000; ++index) {
        items.push_back(oneui::ListItem{L"Row " + std::to_wstring(index), L""});
    }

    oneui::VirtualList list;
    list.setItems(std::move(items));
    expectNear(
        "VirtualList does not scroll before it has a viewport",
        list.scrollOffset(),
        0.0f);
    list.setRowHeight(40.0f);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 240.0f});
    list.setScrollOffset(100000.0f);

    RecordingCanvas canvas;
    list.paint(canvas);

    expectEqual("VirtualList clips the viewport exactly once", static_cast<int>(canvas.clips.size()), 1);
    expectEqual("VirtualList paints a bounded number of visible rows", static_cast<int>(canvas.texts.size()) <= 8 ? 1 : 0, 1);
    expectEqual("VirtualList paints the row at the scroll offset", countTextsWithText(canvas, L"Row 2500"), 1);

    const float offsetBeforeUpdate = list.scrollOffset();
    const int selectionBeforeUpdate = list.selectedIndex();
    expectEqual(
        "VirtualList updates one row in place",
        list.updateItem(2500, oneui::ListItem{L"Updated 2500", L"Ready"}) ? 1 : 0,
        1);
    expectNear("VirtualList row update preserves scroll offset", list.scrollOffset(), offsetBeforeUpdate);
    expectEqual(
        "VirtualList row update preserves selection",
        list.selectedIndex(),
        selectionBeforeUpdate);
    RecordingCanvas updatedCanvas;
    list.paint(updatedCanvas);
    expectEqual(
        "VirtualList paints the updated row",
        countTextsWithText(updatedCanvas, L"Updated 2500"),
        1);
    expectEqual(
        "VirtualList rejects an out-of-range row update",
        list.updateItem(5000, oneui::ListItem{L"Out of range", L""}) ? 1 : 0,
        0);

    list.setSelectedIndex(4999);
    expectEqual("VirtualList selected index updates", list.selectedIndex(), 4999);
    expectNear("VirtualList keeps keyboard selection visible", list.scrollOffset(), list.maxScrollOffset());

    const float offsetBeforeWheel = list.scrollOffset();
    const bool wheelHandled = list.onMouseWheel(oneui::MouseWheelEvent{
        oneui::Point{10.0f, 10.0f}, 2.0f, false, false, false, 1000.0});
    expectEqual("VirtualList handles wheel events when scrollable", wheelHandled ? 1 : 0, 1);
    expectNear("VirtualList defers motion until the display frame", list.scrollOffset(), offsetBeforeWheel);
    list.tickAnimations(1005.0);
    expectEqual(
        "VirtualList first display frame advances without jumping to target",
        list.scrollOffset() < offsetBeforeWheel && list.scrollOffset() > offsetBeforeWheel - 96.0f ? 1 : 0,
        1);
    list.onMouseWheel(oneui::MouseWheelEvent{
        oneui::Point{10.0f, 10.0f}, 6.0f, false, false, false, 1258.0});
    list.tickAnimations(1263.0);
    expectEqual(
        "VirtualList retargets the same continuous wheel gesture",
        list.scrollOffset() < offsetBeforeWheel - 96.0f ? 1 : 0,
        1);
    list.tickAnimations(2000.0);
    expectNear(
        "VirtualList settles at the combined wheel distance",
        list.scrollOffset(),
        offsetBeforeWheel - 384.0f);

    list.setSelectedIndex(-1);
    expectEqual("VirtualList supports an explicit empty selection", list.selectedIndex(), -1);
    expectEqual(
        "VirtualList empty selection is reflected in accessibility state",
        list.accessibilityInfo().state.selected ? 1 : 0,
        0);
    list.onKeyDown(oneui::KeyEvent{oneui::Key::Down});
    expectEqual("VirtualList keyboard navigation resumes from the first row", list.selectedIndex(), 0);
}

void testVirtualListUsesStandardMultipleSelectionSemantics() {
    std::vector<oneui::ListItem> items;
    for (int index = 0; index < 8; ++index) {
        items.push_back(oneui::ListItem{L"Row " + std::to_wstring(index), L""});
    }

    oneui::VirtualList list;
    list.setItems(std::move(items));
    list.setRowHeight(40.0f);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 320.0f});
    list.setSelectionMode(oneui::SelectionMode::Multiple);
    list.setSelectedIndex(1);

    int selectionChanges = 0;
    list.setOnSelectionChanged([&selectionChanges](const std::vector<int>&) {
        ++selectionChanges;
    });

    const oneui::MouseEvent ctrlRow3{
        oneui::Point{20.0f, 140.0f}, oneui::MouseButton::Left, false, true, false};
    list.onMouseDown(ctrlRow3);
    list.onMouseUp(ctrlRow3);
    expectEqual("VirtualList Ctrl click keeps prior selection", list.selectedIndices().size(), 2);
    expectEqual("VirtualList Ctrl click selects target", list.selectedIndices().back(), 3);

    const oneui::MouseEvent shiftRow5{
        oneui::Point{20.0f, 220.0f}, oneui::MouseButton::Left, true, false, false};
    list.onMouseDown(shiftRow5);
    list.onMouseUp(shiftRow5);
    expectEqual("VirtualList Shift click creates anchored range", list.selectedIndices().size(), 3);
    expectEqual("VirtualList Shift range starts at anchor", list.selectedIndices().front(), 3);
    expectEqual("VirtualList Shift range ends at target", list.selectedIndices().back(), 5);

    list.onKeyDown(oneui::KeyEvent{oneui::Key::A, false, true});
    expectEqual("VirtualList Ctrl+A selects every row", list.selectedIndices().size(), 8);
    expectEqual("VirtualList emits each selection collection change", selectionChanges, 3);

    list.onKeyDown(oneui::KeyEvent{oneui::Key::Down, false, true});
    expectEqual("VirtualList Ctrl+Down preserves selected rows", list.selectedIndices().size(), 8);
}

void testVirtualListExposesStandardRowCommands() {
    std::vector<oneui::ListItem> items;
    for (int index = 0; index < 6; ++index) {
        items.push_back(oneui::ListItem{L"Row " + std::to_wstring(index), L""});
    }

    oneui::VirtualList list;
    list.setItems(std::move(items));
    list.setRowHeight(40.0f);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 240.0f});
    list.setSelectionMode(oneui::SelectionMode::Multiple);
    list.setSelectedIndices({1, 2});

    int activated = -1;
    int editRequested = -1;
    int contextIndex = -1;
    oneui::Point contextPoint{};
    list.setOnActivated([&activated](int index) { activated = index; });
    list.setOnEditRequested([&editRequested](int index) { editRequested = index; });
    list.setOnContextMenuRequested([&contextIndex, &contextPoint](int index, oneui::Point point) {
        contextIndex = index;
        contextPoint = point;
    });

    const oneui::MouseEvent selectedContext{
        oneui::Point{20.0f, 100.0f}, oneui::MouseButton::Right};
    list.onMouseDown(selectedContext);
    list.onMouseUp(selectedContext);
    expectEqual("VirtualList context click preserves an existing selection", list.selectedIndices().size(), 2);
    expectEqual("VirtualList reports the context row", contextIndex, 2);
    expectNear("VirtualList reports the context x coordinate", contextPoint.x, 20.0f);

    const oneui::MouseEvent unselectedContext{
        oneui::Point{30.0f, 180.0f}, oneui::MouseButton::Right};
    list.onMouseDown(unselectedContext);
    list.onMouseUp(unselectedContext);
    expectEqual("VirtualList context click selects an unselected row", list.selectedIndices().size(), 1);
    expectEqual("VirtualList context click makes its row active", list.selectedIndex(), 4);

    list.onKeyDown(oneui::KeyEvent{oneui::Key::Enter});
    list.onKeyDown(oneui::KeyEvent{oneui::Key::F2});
    expectEqual("VirtualList Enter activates the active row", activated, 4);
    expectEqual("VirtualList F2 requests editing for the active row", editRequested, 4);
}

void testVirtualListReportsReorderRequestsWithoutMutatingSelection() {
    oneui::VirtualList list;
    list.setItems({
        {L"Alpha", L""},
        {L"Beta", L""},
        {L"Gamma", L""},
        {L"Delta", L""},
    });
    list.setRowHeight(40.0f);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 160.0f});
    list.setSelectedIndex(0);
    list.setReorderEnabled(true);

    int reorderCount = 0;
    int sourceIndex = -1;
    int targetIndex = -1;
    list.setOnReorderRequested([&](int source, int target) {
        ++reorderCount;
        sourceIndex = source;
        targetIndex = target;
    });

    const oneui::MouseEvent betaDown{
        oneui::Point{40.0f, 60.0f}, oneui::MouseButton::Left};
    list.onMouseDown(betaDown);
    list.onMouseMove(oneui::MouseEvent{oneui::Point{42.0f, 61.0f}});
    list.onMouseUp(oneui::MouseEvent{
        oneui::Point{42.0f, 61.0f}, oneui::MouseButton::Left});
    expectEqual("VirtualList movement below drag threshold does not reorder", reorderCount, 0);
    expectEqual("VirtualList normal click still selects the row", list.selectedIndex(), 1);

    list.setSelectedIndex(0);
    list.onMouseDown(betaDown);
    list.onMouseMove(oneui::MouseEvent{oneui::Point{40.0f, 142.0f}});
    list.onMouseUp(oneui::MouseEvent{
        oneui::Point{40.0f, 142.0f}, oneui::MouseButton::Left});
    expectEqual("VirtualList drag emits exactly one reorder request", reorderCount, 1);
    expectEqual("VirtualList reorder reports the source index", sourceIndex, 1);
    expectEqual("VirtualList reorder reports the final target index", targetIndex, 3);
    expectEqual("VirtualList reorder does not mutate selection", list.selectedIndex(), 0);

    list.setSelectedIndex(2);
    oneui::KeyEvent altUp{oneui::Key::Up};
    altUp.alt = true;
    list.onKeyDown(altUp);
    expectEqual("VirtualList Alt+Up emits a reorder request", reorderCount, 2);
    expectEqual("VirtualList keyboard reorder reports source", sourceIndex, 2);
    expectEqual("VirtualList keyboard reorder reports target", targetIndex, 1);
    expectEqual("VirtualList keyboard reorder preserves selection", list.selectedIndex(), 2);
}

void testVirtualListEmitsStableExternalItemDragWithoutBreakingReorder() {
    oneui::VirtualList list;
    list.setItems({
        {L"Alpha", L""},
        {L"Beta", L""},
        {L"Gamma", L""},
    });
    list.setRowHeight(40.0f);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 120.0f});
    expectEqual(
        "VirtualList rejects mismatched external drag IDs",
        list.setItemDragIds({L"alpha", L"beta"}) ? 1 : 0,
        0);
    expectEqual(
        "VirtualList rejects duplicate external drag IDs",
        list.setItemDragIds({L"alpha", L"alpha", L"gamma"}) ? 1 : 0,
        0);
    expectEqual(
        "VirtualList accepts stable external drag IDs",
        list.setItemDragIds({L"alpha", L"beta", L"gamma"}) ? 1 : 0,
        1);
    list.setItemDragEnabled(true);
    list.setReorderEnabled(true);

    int reorderCount = 0;
    list.setOnReorderRequested([&](int, int) { ++reorderCount; });
    std::vector<oneui::ItemDragEvent> events;
    list.setOnItemDrag([&](const oneui::ItemDragEvent& event) {
        events.push_back(event);
    });

    const oneui::MouseEvent betaDown{
        oneui::Point{40.0f, 60.0f}, oneui::MouseButton::Left};
    list.onMouseDown(betaDown);
    list.onMouseMove(oneui::MouseEvent{oneui::Point{230.0f, 60.0f}});
    list.onMouseMove(oneui::MouseEvent{oneui::Point{250.0f, 66.0f}});
    list.onMouseUp(oneui::MouseEvent{
        oneui::Point{250.0f, 66.0f}, oneui::MouseButton::Left});

    expectEqual("VirtualList external drag emits three phases", static_cast<int>(events.size()), 3);
    if (events.size() == 3) {
        expectEqual("VirtualList external drag starts once", events[0].phase == oneui::ItemDragPhase::Started ? 1 : 0, 1);
        expectEqual("VirtualList external drag updates once", events[1].phase == oneui::ItemDragPhase::Updated ? 1 : 0, 1);
        expectEqual("VirtualList external drag drops once", events[2].phase == oneui::ItemDragPhase::Dropped ? 1 : 0, 1);
        expectEqual("VirtualList external drag keeps a stable ID", events[2].sourceId == L"beta" ? 1 : 0, 1);
        expectNear("VirtualList external drag reports client x", events[2].position.x, 250.0f);
        expectNear("VirtualList external drag reports client y", events[2].position.y, 66.0f);
    }
    expectEqual("VirtualList external drag does not request an internal reorder", reorderCount, 0);

    list.onMouseDown(betaDown);
    list.onMouseMove(oneui::MouseEvent{oneui::Point{40.0f, 105.0f}});
    list.onMouseUp(oneui::MouseEvent{
        oneui::Point{40.0f, 105.0f}, oneui::MouseButton::Left});
    expectEqual("VirtualList internal drag still requests one reorder", reorderCount, 1);
    expectEqual(
        "VirtualList internal reorder does not leak external drag phases",
        static_cast<int>(events.size()),
        3);

    list.onMouseDown(betaDown);
    list.onMouseMove(oneui::MouseEvent{oneui::Point{230.0f, 60.0f}});
    list.setItems({{L"Replacement", L""}});
    expectEqual(
        "VirtualList model reset cancels the active external drag",
        static_cast<int>(events.size()),
        5);
    if (events.size() == 5) {
        expectEqual(
            "VirtualList model reset emits cancellation after start",
            events[4].phase == oneui::ItemDragPhase::Cancelled ? 1 : 0,
            1);
        expectEqual(
            "VirtualList model reset cancellation retains the old stable ID",
            events[4].sourceId == L"beta" ? 1 : 0,
            1);
    }
    expectEqual(
        "VirtualList item reset clears stale external drag IDs",
        list.setItemDragIds({L"replacement"}) ? 1 : 0,
        1);
}

void testVirtualListReorderIndicatorUsesCssFocusStyle() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool parsed = sheet.addRulesFromCss(R"css(
        virtual-list.reorderable {
            outline-color: #38bdf8;
            outline-width: 3px;
            text-inset: 11px;
        }
    )css", &error);
    expectEqual("VirtualList reorder CSS parses", parsed ? 1 : 0, 1);

    oneui::VirtualList list;
    list.setItems({{L"Alpha", L""}, {L"Beta", L""}, {L"Gamma", L""}});
    list.setRowHeight(40.0f);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 120.0f});
    list.setStyleOverride(oneui::listStyleOverrideFromStyleSheet(
        sheet,
        oneui::StyleNode{"virtual-list", {"reorderable"}, oneui::StyleStateNone}));
    list.setReorderEnabled(true);
    list.onMouseDown(oneui::MouseEvent{
        oneui::Point{40.0f, 20.0f}, oneui::MouseButton::Left});
    list.onMouseMove(oneui::MouseEvent{oneui::Point{40.0f, 100.0f}});

    RecordingCanvas canvas;
    list.paint(canvas);
    const auto indicator = std::find_if(canvas.lines.begin(), canvas.lines.end(), [](const DrawLineCall& line) {
        return sameColor(line.color, oneui::Color{56, 189, 248});
    });
    expectEqual("VirtualList paints a CSS-colored reorder indicator", indicator != canvas.lines.end() ? 1 : 0, 1);
    if (indicator != canvas.lines.end()) {
        expectNear("VirtualList paints the CSS reorder indicator width", indicator->width, 3.0f);
        expectNear("VirtualList applies CSS inset to the reorder indicator", indicator->from.x, 11.0f);
    }
}

void testReorderableGridOwnsLayoutGestureAndCssIndicator() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool parsed = sheet.addRulesFromCss(R"css(
        reorderable-grid.hosts {
            padding: 2px 4px 6px 4px;
            gap: 12px;
            height: 80px;
            grid-min-column-width: 96px;
            outline-color: #22d3ee;
            outline-width: 3px;
        }
    )css", &error);
    expectEqual("ReorderableGrid CSS parses", parsed ? 1 : 0, 1);

    oneui::ReorderableGrid grid;
    grid.setColumnCount(2);
    grid.setStyleBox(sheet.resolve(
        oneui::StyleNode{"reorderable-grid", {"hosts"}, oneui::StyleStateNone}));
    grid.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 180.0f});
    grid.setReorderEnabled(true);

    int activationCount = 0;
    std::vector<std::shared_ptr<oneui::InteractiveSurface>> surfaces;
    for (const wchar_t* id : {L"alpha", L"beta", L"gamma", L"delta"}) {
        auto surface = std::make_shared<oneui::InteractiveSurface>();
        surface->setOnPointerActivated([&](const oneui::MouseEvent&) {
            ++activationCount;
        });
        grid.addItem(oneui::ReorderableGridItem{id, surface});
        surfaces.push_back(std::move(surface));
    }
    grid.addItem(oneui::ReorderableGridItem{
        L"alpha", std::make_shared<oneui::InteractiveSurface>()});
    grid.addItem(oneui::ReorderableGridItem{
        L"", std::make_shared<oneui::InteractiveSurface>()});
    expectEqual("ReorderableGrid rejects ambiguous item IDs", grid.itemCount(), 4);

    RecordingCanvas initialCanvas;
    grid.paint(initialCanvas);
    expectNear("ReorderableGrid lays out first column from CSS padding", surfaces[0]->frame().x, 4.0f);
    expectNear("ReorderableGrid lays out equal card widths", surfaces[0]->frame().width, 100.0f);
    expectNear("ReorderableGrid computes reusable content height", grid.contentHeight(), 180.0f);

    grid.setFrame(oneui::Rect{0.0f, 0.0f, 150.0f, 380.0f});
    RecordingCanvas narrowCanvas;
    grid.paint(narrowCanvas);
    expectNear("ReorderableGrid drops a column below its CSS minimum", surfaces[0]->frame().width, 142.0f);
    expectNear("ReorderableGrid recomputes responsive content height", grid.contentHeight(), 364.0f);
    grid.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 180.0f});
    RecordingCanvas restoredCanvas;
    grid.paint(restoredCanvas);

    int reorderCount = 0;
    std::wstring sourceId;
    int targetIndex = -1;
    grid.setOnReorderRequested([&](const std::wstring& source, int target) {
        ++reorderCount;
        sourceId = source;
        targetIndex = target;
    });
    int externalDragEventCount = 0;
    grid.setItemDragEnabled(true);
    grid.setOnItemDrag([&](const oneui::ItemDragEvent&) {
        ++externalDragEventCount;
    });

    const oneui::MouseEvent betaDown{
        oneui::Point{160.0f, 40.0f}, oneui::MouseButton::Left};
    grid.onMouseDown(betaDown);
    grid.onMouseMove(oneui::MouseEvent{oneui::Point{162.0f, 41.0f}});
    grid.onMouseUp(oneui::MouseEvent{
        oneui::Point{162.0f, 41.0f}, oneui::MouseButton::Left});
    expectEqual("ReorderableGrid movement below threshold remains a click", reorderCount, 0);
    expectEqual("ReorderableGrid preserves child activation below threshold", activationCount, 1);

    grid.onMouseDown(oneui::MouseEvent{
        oneui::Point{40.0f, 40.0f}, oneui::MouseButton::Left});
    grid.onMouseMove(oneui::MouseEvent{oneui::Point{10.0f, 175.0f}});
    grid.onMouseUp(oneui::MouseEvent{
        oneui::Point{10.0f, 175.0f}, oneui::MouseButton::Left});
    expectEqual(
        "ReorderableGrid uses the absolute content bottom with asymmetric padding",
        targetIndex,
        3);
    reorderCount = 0;

    grid.onMouseDown(betaDown);
    grid.onMouseMove(oneui::MouseEvent{oneui::Point{205.0f, 140.0f}});
    RecordingCanvas dragCanvas;
    grid.paint(dragCanvas);
    grid.onMouseUp(oneui::MouseEvent{
        oneui::Point{205.0f, 140.0f}, oneui::MouseButton::Left});
    expectEqual("ReorderableGrid drag emits exactly one request", reorderCount, 1);
    expectEqual("ReorderableGrid reports a stable source ID", sourceId == L"beta" ? 1 : 0, 1);
    expectEqual("ReorderableGrid reports the final target index", targetIndex, 3);
    expectEqual("ReorderableGrid drag suppresses child activation", activationCount, 1);
    expectEqual("ReorderableGrid internal reorder does not leak external drag phases", externalDragEventCount, 0);
    const auto indicator = std::find_if(
        dragCanvas.lines.begin(), dragCanvas.lines.end(), [](const DrawLineCall& line) {
            return sameColor(line.color, oneui::Color{34, 211, 238});
        });
    expectEqual(
        "ReorderableGrid paints a CSS-colored insertion indicator",
        indicator != dragCanvas.lines.end() ? 1 : 0,
        1);
    if (indicator != dragCanvas.lines.end()) {
        expectNear("ReorderableGrid applies CSS indicator width", indicator->width, 3.0f);
    }
    expectEqual(
        "ReorderableGrid applies an accepted request in place",
        grid.moveItem(L"beta", targetIndex) ? 1 : 0,
        1);
    RecordingCanvas reorderedCanvas;
    grid.paint(reorderedCanvas);
    expectNear(
        "ReorderableGrid relayouts the accepted item at its target row",
        surfaces[1]->frame().y,
        94.0f);
    grid.setStyleBox(oneui::StyleBox{});
    expectNear(
        "ReorderableGrid clears CSS geometry without mutating API defaults",
        grid.contentHeight(),
        320.0f);
}

void testReorderableGridEmitsExternalItemDragWithoutInternalReorder() {
    oneui::ReorderableGrid grid;
    grid.setColumnCount(1);
    grid.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 100.0f});
    grid.setReorderEnabled(false);
    grid.setItemDragEnabled(true);
    grid.addItem(oneui::ReorderableGridItem{
        L"host-42", std::make_shared<oneui::InteractiveSurface>()});
    RecordingCanvas canvas;
    grid.paint(canvas);

    std::vector<oneui::ItemDragEvent> events;
    grid.setOnItemDrag([&](const oneui::ItemDragEvent& event) {
        events.push_back(event);
    });
    grid.onMouseDown(oneui::MouseEvent{
        oneui::Point{40.0f, 30.0f}, oneui::MouseButton::Left});
    grid.onMouseMove(oneui::MouseEvent{oneui::Point{50.0f, 42.0f}});
    grid.onMouseMove(oneui::MouseEvent{oneui::Point{220.0f, 60.0f}});
    grid.onMouseMove(oneui::MouseEvent{oneui::Point{230.0f, 64.0f}});
    grid.onMouseUp(oneui::MouseEvent{
        oneui::Point{230.0f, 64.0f}, oneui::MouseButton::Left});

    expectEqual("ReorderableGrid external drag emits three phases", static_cast<int>(events.size()), 3);
    if (events.size() == 3) {
        expectEqual("ReorderableGrid external drag starts once", events[0].phase == oneui::ItemDragPhase::Started ? 1 : 0, 1);
        expectEqual("ReorderableGrid external drag updates once", events[1].phase == oneui::ItemDragPhase::Updated ? 1 : 0, 1);
        expectEqual("ReorderableGrid external drag drops once", events[2].phase == oneui::ItemDragPhase::Dropped ? 1 : 0, 1);
        expectEqual("ReorderableGrid external drag keeps a stable ID", events[2].sourceId == L"host-42" ? 1 : 0, 1);
        expectNear("ReorderableGrid external drag reports client x", events[2].position.x, 230.0f);
        expectNear("ReorderableGrid external drag reports client y", events[2].position.y, 64.0f);
    }

}

void testPointerActivationUsesSystemClickCountAndSeparateContextAction() {
    oneui::InteractiveSurface surface;
    surface.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 80.0f});

    int activationCount = 0;
    int activationClicks = 0;
    bool activationControl = false;
    int semanticActivationCount = 0;
    int contextCount = 0;
    oneui::Point contextPoint{};
    surface.setOnPointerActivated(
        [&](const oneui::MouseEvent& event) {
            ++activationCount;
            activationClicks = event.clickCount;
            activationControl = event.control;
        });
    surface.setOnContextMenuRequested(
        [&](const oneui::MouseEvent& event) {
            ++contextCount;
            contextPoint = event.position;
        });
    surface.setOnClick([&] { ++semanticActivationCount; });

    const oneui::MouseEvent doubleClick{
        oneui::Point{40.0f, 30.0f}, oneui::MouseButton::Left, false, true, false, 2};
    surface.onMouseDown(doubleClick);
    surface.onMouseUp(doubleClick);
    expectEqual("InteractiveSurface emits one pointer activation", activationCount, 1);
    expectEqual("InteractiveSurface preserves the system click count", activationClicks, 2);
    expectEqual("InteractiveSurface preserves pointer modifiers", activationControl ? 1 : 0, 1);
    expectEqual("InteractiveSurface pointer callback owns pointer activation", semanticActivationCount, 0);

    surface.onKeyDown(oneui::KeyEvent{oneui::Key::Enter, false, true});
    expectEqual("InteractiveSurface keyboard activation uses the semantic action", semanticActivationCount, 1);
    expectEqual("InteractiveSurface keyboard activation does not fake pointer input", activationCount, 1);
    expectEqual(
        "InteractiveSurface ignores unrelated keys",
        surface.onKeyDown(oneui::KeyEvent{oneui::Key::Escape}) ? 1 : 0,
        0);

    const oneui::MouseEvent context{
        oneui::Point{52.0f, 36.0f}, oneui::MouseButton::Right};
    surface.onMouseDown(context);
    surface.onMouseUp(context);
    expectEqual("InteractiveSurface keeps context separate from activation", activationCount, 1);
    expectEqual("InteractiveSurface emits one context request", contextCount, 1);
    expectNear("InteractiveSurface context retains x", contextPoint.x, 52.0f);

    auto compositeContent = std::make_shared<oneui::View>();
    compositeContent->setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 80.0f});
    auto childButton = std::make_shared<oneui::Button>(L"Child action");
    childButton->setFrame(oneui::Rect{8.0f, 8.0f, 88.0f, 32.0f});
    int childActivationCount = 0;
    childButton->setOnClick([&] { ++childActivationCount; });
    compositeContent->add(childButton);
    surface.setContent(compositeContent);

    const oneui::MouseEvent compositeBlank{
        oneui::Point{180.0f, 60.0f}, oneui::MouseButton::Left};
    surface.onMouseDown(compositeBlank);
    surface.onMouseUp(compositeBlank);
    expectEqual(
        "InteractiveSurface activates unhandled space around focusable descendants",
        activationCount,
        2);

    const oneui::MouseEvent childClick{
        oneui::Point{20.0f, 20.0f}, oneui::MouseButton::Left};
    surface.onMouseDown(childClick);
    surface.onMouseUp(childClick);
    expectEqual("InteractiveSurface preserves child actions", childActivationCount, 1);
    expectEqual("InteractiveSurface does not duplicate child actions", activationCount, 2);

    oneui::VirtualList list;
    list.setItems({{L"Alpha", L""}, {L"Beta", L""}});
    list.setRowHeight(40.0f);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 80.0f});
    int activated = -1;
    list.setOnActivated([&activated](int index) { activated = index; });
    const oneui::MouseEvent rowDoubleClick{
        oneui::Point{20.0f, 60.0f}, oneui::MouseButton::Left, false, false, false, 2};
    list.onMouseDown(rowDoubleClick);
    list.onMouseUp(rowDoubleClick);
    expectEqual("VirtualList double click activates the row", activated, 1);
}

void testVirtualListCssControlsCompactTypographyAndScrollbar() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool parsed = sheet.addRulesFromCss(R"css(
        virtual-list.compact-data-list {
            content-background: transparent;
            font-size: 12px;
            font-weight: 600;
            detail-font-size: 11px;
            detail-font-weight: 400;
            text-inset: 9px;
            title-offset-y: 4px;
            detail-offset-y: 22px;
            scrollbar-color: #6d7188aa;
            scrollbar-width: 3px;
        }
        virtual-list.compact-data-list:hover {
            content-background: #253142;
        }
        virtual-list.compact-data-list:active {
            content-background: #34445a;
        }
    )css", &error);
    expectEqual("VirtualList compact CSS parses", parsed ? 1 : 0, 1);

    const auto style = oneui::listStyleOverrideFromStyleSheet(
        sheet,
        oneui::StyleNode{"virtual-list", {"compact-data-list"}, oneui::StyleStateNone});
    expectEqual("VirtualList CSS has a normal state", style.normal.has_value() ? 1 : 0, 1);
    if (!style.normal) {
        return;
    }
    expectNear("VirtualList CSS title font size", style.normal->titleFontSize.value_or(0.0f), 12.0f);
    expectNear("VirtualList CSS detail font size", style.normal->detailFontSize.value_or(0.0f), 11.0f);
    expectEqual("VirtualList CSS title font weight", style.normal->titleFontWeight.value_or(0), 600);
    expectNear("VirtualList CSS text inset", style.normal->textInset.value_or(0.0f), 9.0f);
    expectNear("VirtualList CSS title offset", style.normal->titleOffsetY.value_or(0.0f), 4.0f);
    expectNear("VirtualList CSS detail offset", style.normal->detailOffsetY.value_or(0.0f), 22.0f);
    expectNear("VirtualList CSS scrollbar width", style.normal->scrollbarWidth.value_or(0.0f), 3.0f);
    expectEqual("VirtualList CSS scrollbar alpha", style.normal->scrollbarColor->a, 170);

    oneui::VirtualList list;
    list.setItems({
        {L"Alpha", L"First"},
        {L"Beta", L"Second"},
        {L"Gamma", L"Third"},
        {L"Delta", L"Fourth"},
    });
    list.setRowHeight(44.0f);
    list.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 88.0f});
    list.setStyleOverride(style);
    list.setSelectedIndex(-1);
    RecordingCanvas canvas;
    list.paint(canvas);
    expectEqual(
        "VirtualList normal rows do not inherit hover state",
        countFillRectsWithColor(canvas, oneui::Color{37, 49, 66, 255}),
        0);
    expectEqual(
        "VirtualList normal rows do not inherit pressed state",
        countFillRectsWithColor(canvas, oneui::Color{52, 68, 90, 255}),
        0);
    const oneui::Color scrollbar{109, 113, 136, 170};
    const auto thumb = std::find_if(canvas.fillRects.begin(), canvas.fillRects.end(), [&](const FillRectCall& call) {
        return sameColor(call.color, scrollbar);
    });
    expectEqual("VirtualList paints configured scrollbar", thumb != canvas.fillRects.end() ? 1 : 0, 1);
    if (thumb != canvas.fillRects.end()) {
        expectNear("VirtualList paints configured scrollbar width", thumb->rect.width, 3.0f);
    }

    list.onMouseMove(oneui::MouseEvent{oneui::Point{20.0f, 60.0f}});
    RecordingCanvas hoverCanvas;
    list.paint(hoverCanvas);
    expectEqual(
        "VirtualList hover styles only the active row",
        countFillRectsWithColor(hoverCanvas, oneui::Color{37, 49, 66, 255}),
        1);
}

void testTreeViewStyleAdapterSharesListContract() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool parsed = sheet.addRulesFromCss(R"css(
        tree-view.data-list {
            background: #101820;
            color: #d5e3ff;
            border-color: #384963;
            border-width: 2px;
            border-radius: 7px;
            content-background: transparent;
            font-size: 13px;
            font-weight: 600;
            detail-font-size: 10px;
            detail-font-weight: 500;
        }
        tree-view.data-list:selected {
            content-background: #263f7a;
            color: #92b7ff;
        }
    )css", &error);
    expectEqual("TreeView CSS style parses", parsed ? 1 : 0, 1);

    oneui::TreeView tree;
    tree.setItems({
        oneui::TreeItem{L"root", L"", L"Root", L"", true},
        oneui::TreeItem{L"child", L"root", L"Child", L"Detail", true},
    });
    tree.setSelectedId(L"child");
    tree.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 88.0f});
    tree.setStyleOverride(oneui::treeViewStyleOverrideFromStyleSheet(
        sheet,
        oneui::StyleNode{"tree-view", {"data-list"}, oneui::StyleStateNone}));

    RecordingCanvas canvas;
    tree.paint(canvas);

    expectEqual("TreeView style adapter applies dark container", countFillRectsWithColor(canvas, oneui::Color{16, 24, 32}), 1);
    expectEqual("TreeView style adapter applies selected row", countFillRectsWithColor(canvas, oneui::Color{38, 63, 122}), 1);
    expectEqual("TreeView style adapter applies selected title", countTextsWithTextAndColor(canvas, L"Child", oneui::Color{146, 183, 255}), 1);
    const auto title = std::find_if(canvas.texts.begin(), canvas.texts.end(), [](const DrawTextCall& text) {
        return text.text == L"Child";
    });
    const auto detail = std::find_if(canvas.texts.begin(), canvas.texts.end(), [](const DrawTextCall& text) {
        return text.text == L"Detail";
    });
    expectEqual("TreeView paints the CSS title font", title != canvas.texts.end() ? 1 : 0, 1);
    expectEqual("TreeView paints the CSS detail font", detail != canvas.texts.end() ? 1 : 0, 1);
    if (title != canvas.texts.end()) {
        expectNear("TreeView applies CSS title font size", title->size, 13.0f);
        expectEqual("TreeView applies CSS title font weight", title->weight, 600);
    }
    if (detail != canvas.texts.end()) {
        expectNear("TreeView applies CSS detail font size", detail->size, 10.0f);
        expectEqual("TreeView applies CSS detail font weight", detail->weight, 500);
    }
}

void testTreeViewReportsStableReorderIdsAndPreservesToggleBehavior() {
    oneui::TreeView tree;
    tree.setItems({
        oneui::TreeItem{L"root", L"", L"Root", L"", true},
        oneui::TreeItem{L"alpha", L"root", L"Alpha", L"1", true},
        oneui::TreeItem{L"beta", L"root", L"Beta", L"2", true},
    });
    tree.setSelectedId(L"alpha");
    tree.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 96.0f});
    tree.setReorderEnabled(true);

    int reorderCount = 0;
    std::wstring sourceId;
    std::wstring targetId;
    tree.setOnReorderRequested([&](const std::wstring& source, const std::wstring& target) {
        ++reorderCount;
        sourceId = source;
        targetId = target;
    });

    tree.onMouseDown(oneui::MouseEvent{
        oneui::Point{60.0f, 48.0f}, oneui::MouseButton::Left});
    tree.onMouseMove(oneui::MouseEvent{oneui::Point{60.0f, 88.0f}});
    tree.onMouseUp(oneui::MouseEvent{
        oneui::Point{60.0f, 88.0f}, oneui::MouseButton::Left});
    expectEqual("TreeView drag emits one reorder request", reorderCount, 1);
    expectEqual("TreeView reorder source uses a stable ID", sourceId == L"alpha" ? 1 : 0, 1);
    expectEqual("TreeView reorder target uses a stable ID", targetId == L"beta" ? 1 : 0, 1);
    expectEqual("TreeView reorder preserves selection", tree.selectedId() == L"alpha" ? 1 : 0, 1);

    tree.onMouseDown(oneui::MouseEvent{
        oneui::Point{12.0f, 16.0f}, oneui::MouseButton::Left});
    tree.onMouseMove(oneui::MouseEvent{oneui::Point{12.0f, 80.0f}});
    tree.onMouseUp(oneui::MouseEvent{
        oneui::Point{12.0f, 16.0f}, oneui::MouseButton::Left});
    expectEqual("TreeView expansion toggle never starts a reorder", reorderCount, 1);
    expectEqual("TreeView expansion toggle keeps its original behavior", tree.isExpanded(L"root") ? 1 : 0, 0);

    tree.setExpanded(L"root", true);
    tree.setSelectedId(L"beta");
    oneui::KeyEvent altUp{oneui::Key::Up};
    altUp.alt = true;
    tree.onKeyDown(altUp);
    expectEqual("TreeView Alt+Up emits a reorder request", reorderCount, 2);
    expectEqual("TreeView keyboard reorder source uses selected ID", sourceId == L"beta" ? 1 : 0, 1);
    expectEqual("TreeView keyboard reorder target uses adjacent ID", targetId == L"alpha" ? 1 : 0, 1);
}

void testTreeViewOwnsTransientExternalDropTargetState() {
    oneui::TreeView tree;
    tree.setItems({
        oneui::TreeItem{L"all", L"", L"All hosts", L"", true},
        oneui::TreeItem{L"group:platform", L"", L"Platform", L"", true},
        oneui::TreeItem{L"group:database", L"", L"Database", L"", true},
    });
    tree.setFrame(oneui::Rect{20.0f, 40.0f, 220.0f, 96.0f});

    tree.updateExternalDropTarget(oneui::Point{80.0f, 88.0f});
    expectEqual(
        "TreeView external drop target uses the visible row stable ID",
        tree.externalDropTargetId() == L"group:platform" ? 1 : 0,
        1);

    tree.updateExternalDropTarget(oneui::Point{260.0f, 88.0f});
    expectEqual(
        "TreeView clears the external target outside its frame",
        tree.externalDropTargetId().empty() ? 1 : 0,
        1);

    tree.updateExternalDropTarget(oneui::Point{80.0f, 120.0f});
    tree.clearExternalDropTarget();
    expectEqual(
        "TreeView exposes explicit external target cleanup",
        tree.externalDropTargetId().empty() ? 1 : 0,
        1);
}

void testTableStyleOverridePaintsCustomColorsAndGeometry() {
    oneui::Table table;
    table.setColumns({
        oneui::TableColumn{L"Name", 70.0f},
        oneui::TableColumn{L"Status", 0.0f},
    });
    table.setRows({
        {L"Acme", L"Live"},
        {L"Billing", L"Review"},
    });
    table.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 110.0f});

    const oneui::Color background{15, 23, 42};
    const oneui::Color border{71, 85, 105};
    const oneui::Color headerBackground{30, 41, 59};
    const oneui::Color headerForeground{203, 213, 225};
    const oneui::Color cellForeground{241, 245, 249};
    const oneui::Color gridLine{51, 65, 85};

    oneui::TableStyleOverride style;
    style.background = background;
    style.border = border;
    style.headerBackground = headerBackground;
    style.headerForeground = headerForeground;
    style.cellForeground = cellForeground;
    style.gridLine = gridLine;
    style.borderWidth = 2.0f;
    style.radius = 9.0f;
    style.headerHeight = 34.0f;
    style.cellPadding = oneui::Insets{0.0f, 14.0f};
    table.setStyleOverride(style);

    RecordingCanvas canvas;
    table.paint(canvas);

    expectEqual("Table style override background", countFillRectsWithColor(canvas, background), 1);
    expectEqual("Table style override border", countStrokeRectsWithColor(canvas, border), 1);
    expectEqual("Table style override header background", countFillRectsWithColor(canvas, headerBackground), 1);
    expectEqual("Table style override grid lines", countLinesWithColor(canvas, gridLine), 3);
    expectEqual("Table style override header text", countTextsWithTextAndColor(canvas, L"Name", headerForeground), 1);
    expectEqual("Table style override cell text", countTextsWithTextAndColor(canvas, L"Acme", cellForeground), 1);
    if (!canvas.fillRects.empty() && !canvas.strokeRects.empty()) {
        expectNear("Table style override radius reaches fillRect", canvas.fillRects[0].radius, 9.0f);
        expectNear("Table style override radius reaches strokeRect", canvas.strokeRects[0].radius, 9.0f);
        expectNear("Table style override borderWidth reaches strokeRect", canvas.strokeRects[0].width, 2.0f);
    }
    if (canvas.fillRects.size() >= 2 && canvas.texts.size() >= 2) {
        expectRect("Table header height follows style", canvas.fillRects[1].rect, oneui::Rect{0.0f, 0.0f, 180.0f, 34.0f});
        expectRect("Table cell padding follows style", canvas.texts[1].rect, oneui::Rect{14.0f, 34.0f, 42.0f, 38.0f});
    }
}

void testTableEmptyStyleOverrideKeepsDefaultPaint() {
    oneui::Table table;
    table.setColumns({oneui::TableColumn{L"Name", 0.0f}});
    table.setRows({{L"Acme"}});
    table.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 70.0f});
    table.setStyleOverride(oneui::TableStyleOverride{});

    RecordingCanvas canvas;
    table.paint(canvas);

    expectEqual("Table empty style override keeps background", countFillRectsWithColor(canvas, oneui::theme().surface), 1);
    expectEqual("Table empty style override keeps header background", countFillRectsWithColor(canvas, oneui::theme().surfaceMuted), 1);
    expectEqual("Table empty style override keeps cell text", countTextsWithTextAndColor(canvas, L"Acme", oneui::theme().text), 1);
}

void testTableDisabledStyleAndClearRestoresDefault() {
    oneui::Table table;
    table.setColumns({oneui::TableColumn{L"Name", 0.0f}});
    table.setRows({{L"Acme"}});
    table.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 70.0f});

    const oneui::Color disabledBackground{15, 23, 42};
    const oneui::Color disabledText{148, 163, 184};
    oneui::TableStyleOverride style;
    style.background = disabledBackground;
    style.headerBackground = disabledBackground;
    style.headerForeground = disabledText;
    style.cellForeground = disabledText;
    table.setStyleOverride(style);
    table.setDisabled(true);

    RecordingCanvas disabledCanvas;
    table.paint(disabledCanvas);

    expectEqual("Table style override applies while disabled", countFillRectsWithColor(disabledCanvas, disabledBackground), 2);
    expectEqual("Table disabled style text", countTextsWithColor(disabledCanvas, disabledText), 2);

    table.setDisabled(false);
    table.clearStyleOverride();
    RecordingCanvas defaultCanvas;
    table.paint(defaultCanvas);

    expectEqual("Table clearStyleOverride restores default background", countFillRectsWithColor(defaultCanvas, oneui::theme().surface), 1);
    expectEqual("Table clearStyleOverride removes disabled background", countFillRectsWithColor(defaultCanvas, disabledBackground), 0);
}

void testBadgeStyleOverridePaintsCustomColorsAndGeometry() {
    oneui::Badge badge(L"MVP", oneui::BadgeVariant::Accent);
    badge.setFrame(oneui::Rect{0.0f, 0.0f, 72.0f, 24.0f});

    const oneui::Color background{15, 23, 42};
    const oneui::Color foreground{241, 245, 249};
    const oneui::Color border{71, 85, 105};

    oneui::BadgeStyleOverride style;
    style.background = background;
    style.foreground = foreground;
    style.border = border;
    style.borderWidth = 2.0f;
    style.radius = 8.0f;
    style.padding = oneui::Insets{0.0f, 12.0f};
    style.fontSize = 13.0f;
    badge.setStyleOverride(style);

    RecordingCanvas canvas;
    badge.paint(canvas);

    expectEqual("Badge style override background", countFillRectsWithColor(canvas, background), 1);
    expectEqual("Badge style override border", countStrokeRectsWithColor(canvas, border), 1);
    expectEqual("Badge style override foreground", countTextsWithTextAndColor(canvas, L"MVP", foreground), 1);
    if (!canvas.fillRects.empty() && !canvas.strokeRects.empty() && !canvas.texts.empty()) {
        expectNear("Badge style override fill radius", canvas.fillRects[0].radius, 8.0f);
        expectNear("Badge style override stroke radius", canvas.strokeRects[0].radius, 8.0f);
        expectNear("Badge style override border width", canvas.strokeRects[0].width, 2.0f);
        expectRect("Badge style override padding", canvas.texts[0].rect, oneui::Rect{12.0f, 0.0f, 48.0f, 24.0f});
    }
}

void testBadgeEmptyStyleOverrideKeepsVariantPaintAndClearRestoresDefault() {
    oneui::Badge badge(L"Ok", oneui::BadgeVariant::Success);
    badge.setFrame(oneui::Rect{0.0f, 0.0f, 72.0f, 24.0f});
    badge.setStyleOverride(oneui::BadgeStyleOverride{});

    RecordingCanvas emptyCanvas;
    badge.paint(emptyCanvas);

    expectEqual("Badge empty style override keeps success background", countFillRectsWithColor(emptyCanvas, oneui::theme().successSoft), 1);
    expectEqual("Badge empty style override keeps success text", countTextsWithTextAndColor(emptyCanvas, L"Ok", oneui::theme().success), 1);

    const oneui::Color customBackground{15, 23, 42};
    oneui::BadgeStyleOverride style;
    style.background = customBackground;
    badge.setStyleOverride(style);
    badge.clearStyleOverride();

    RecordingCanvas defaultCanvas;
    badge.paint(defaultCanvas);

    expectEqual("Badge clearStyleOverride restores variant background", countFillRectsWithColor(defaultCanvas, oneui::theme().successSoft), 1);
    expectEqual("Badge clearStyleOverride removes custom background", countFillRectsWithColor(defaultCanvas, customBackground), 0);
}

void testProgressBarStyleOverridePaintsCustomColorsAndGeometry() {
    oneui::ProgressBar progress;
    progress.setValue(0.25);
    progress.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 12.0f});

    const oneui::Color track{15, 23, 42};
    const oneui::Color fill{37, 99, 235};

    oneui::ProgressBarStyleOverride style;
    style.trackBackground = track;
    style.fill = fill;
    style.radius = 3.0f;
    progress.setStyleOverride(style);

    RecordingCanvas canvas;
    progress.paint(canvas);

    expectEqual("ProgressBar style override track", countFillRectsWithColor(canvas, track), 1);
    expectEqual("ProgressBar style override fill", countFillRectsWithColor(canvas, fill), 1);
    if (canvas.fillRects.size() >= 2) {
        expectNear("ProgressBar style override track radius", canvas.fillRects[0].radius, 3.0f);
        expectRect("ProgressBar style override fill width", canvas.fillRects[1].rect, oneui::Rect{0.0f, 0.0f, 50.0f, 12.0f});
    }
}

void testProgressBarDisabledStyleAndClearRestoresDefault() {
    oneui::ProgressBar progress;
    progress.setValue(0.5);
    progress.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 10.0f});

    const oneui::Color customFill{20, 184, 166};
    const oneui::Color disabledFill{148, 163, 184};
    oneui::ProgressBarStyleOverride style;
    style.fill = customFill;
    style.disabledFill = disabledFill;
    progress.setStyleOverride(style);
    progress.setDisabled(true);

    RecordingCanvas disabledCanvas;
    progress.paint(disabledCanvas);

    expectEqual("ProgressBar disabled style uses disabled fill", countFillRectsWithColor(disabledCanvas, disabledFill), 1);
    expectEqual("ProgressBar disabled style ignores normal fill", countFillRectsWithColor(disabledCanvas, customFill), 0);

    progress.setDisabled(false);
    progress.clearStyleOverride();
    RecordingCanvas defaultCanvas;
    progress.paint(defaultCanvas);

    expectEqual("ProgressBar clearStyleOverride restores default fill", countFillRectsWithColor(defaultCanvas, oneui::theme().primary), 1);
    expectEqual("ProgressBar clearStyleOverride removes custom fill", countFillRectsWithColor(defaultCanvas, customFill), 0);
}

void testSeparatorStyleOverridePaintsCustomColorAndThickness() {
    oneui::Separator separator;
    separator.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 8.0f});

    const oneui::Color color{71, 85, 105};
    oneui::SeparatorStyleOverride style;
    style.color = color;
    style.thickness = 2.0f;
    separator.setStyleOverride(style);

    RecordingCanvas canvas;
    separator.paint(canvas);

    expectEqual("Separator style override line color", countLinesWithColor(canvas, color), 1);
    if (!canvas.lines.empty()) {
        expectNear("Separator style override line thickness", canvas.lines[0].width, 2.0f);
        expectNear("Separator horizontal y remains centered", canvas.lines[0].from.y, 4.0f);
    }
}

void testSeparatorEmptyStyleOverrideAndClearRestoresDefault() {
    oneui::Separator separator(oneui::SeparatorOrientation::Vertical);
    separator.setFrame(oneui::Rect{0.0f, 0.0f, 8.0f, 160.0f});
    separator.setStyleOverride(oneui::SeparatorStyleOverride{});

    RecordingCanvas emptyCanvas;
    separator.paint(emptyCanvas);

    expectEqual("Separator empty style override keeps default border", countLinesWithColor(emptyCanvas, oneui::theme().border), 1);

    const oneui::Color customColor{15, 23, 42};
    oneui::SeparatorStyleOverride style;
    style.color = customColor;
    separator.setStyleOverride(style);
    separator.clearStyleOverride();

    RecordingCanvas defaultCanvas;
    separator.paint(defaultCanvas);

    expectEqual("Separator clearStyleOverride restores default border", countLinesWithColor(defaultCanvas, oneui::theme().border), 1);
    expectEqual("Separator clearStyleOverride removes custom color", countLinesWithColor(defaultCanvas, customColor), 0);
}

void testSliderMouseFocusIsNotFocusVisible() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 260.0f, 80.0f});

    auto slider = std::make_shared<oneui::Slider>();
    slider->setFrame(oneui::Rect{10.0f, 10.0f, 220.0f, 32.0f});
    view.add(slider);

    view.onMouseDown(oneui::MouseEvent{oneui::Point{120.0f, 26.0f}});

    expectEqual("Slider mouse focus focused", slider->focused() ? 1 : 0, 1);
    expectEqual("Slider mouse focus is not focus-visible", slider->focusVisible() ? 1 : 0, 0);
}

void testSliderKeyboardFocusVisible() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 280.0f, 120.0f});

    auto first = std::make_shared<oneui::Slider>();
    first->setFrame(oneui::Rect{10.0f, 10.0f, 220.0f, 32.0f});
    auto second = std::make_shared<oneui::Slider>();
    second->setFrame(oneui::Rect{10.0f, 52.0f, 220.0f, 32.0f});

    view.add(first);
    view.add(second);
    view.onFocusChanged(true);

    view.onKeyDown(oneui::KeyEvent{oneui::Key::Tab});

    expectEqual("Slider Tab focus focused", second->focused() ? 1 : 0, 1);
    expectEqual("Slider Tab focus is focus-visible", second->focusVisible() ? 1 : 0, 1);
}

void testSliderStyleOverridePaintsCustomColorsAndGeometry() {
    oneui::Slider slider;
    slider.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 32.0f});
    slider.setValue(0.5);

    const oneui::Color track{11, 22, 33};
    const oneui::Color fill{44, 55, 66};
    const oneui::Color thumb{77, 88, 99};
    const oneui::Color thumbBorder{111, 122, 133};
    oneui::SliderStyleOverride style;
    oneui::SliderStateStyleOverride normal;
    normal.trackBackground = track;
    normal.trackFill = fill;
    normal.thumbBackground = thumb;
    normal.thumbBorder = thumbBorder;
    normal.trackHeight = 6.0f;
    normal.thumbSize = 20.0f;
    normal.thumbBorderWidth = 2.0f;
    style.normal = normal;
    slider.setStyleOverride(style);

    RecordingCanvas canvas;
    slider.paint(canvas);

    expectEqual("Slider style override track", countFillRectsWithColor(canvas, track), 1);
    expectEqual("Slider style override fill", countFillRectsWithColor(canvas, fill), 1);
    expectEqual("Slider style override thumb", countFillRectsWithColor(canvas, thumb), 1);
    expectEqual("Slider style override thumb border", countStrokeRectsWithColor(canvas, thumbBorder), 1);
    if (canvas.fillRects.size() >= 3) {
        expectRect("Slider style override track geometry", canvas.fillRects[0].rect, oneui::Rect{10.0f, 13.0f, 100.0f, 6.0f});
        expectRect("Slider style override fill geometry", canvas.fillRects[1].rect, oneui::Rect{10.0f, 13.0f, 50.0f, 6.0f});
        expectRect("Slider style override thumb geometry", canvas.fillRects[2].rect, oneui::Rect{50.0f, 6.0f, 20.0f, 20.0f});
    }
}

void testSliderEmptyStyleOverrideKeepsDefaultPaint() {
    oneui::Slider slider;
    slider.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 32.0f});
    slider.setValue(0.5);
    slider.setStyleOverride(oneui::SliderStyleOverride{});

    RecordingCanvas canvas;
    slider.paint(canvas);

    expectEqual("Slider empty style override keeps default track", countFillRectsWithColor(canvas, oneui::Color{226, 232, 240}), 1);
    expectEqual("Slider empty style override keeps default fill", countFillRectsWithColor(canvas, oneui::theme().primary), 2);
}

void testSliderStyleOverrideCanHideFocusRingAndStylePressed() {
    oneui::Slider slider;
    slider.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 32.0f});
    slider.onFocusChanged(true);
    slider.setFocusVisible(true);

    const oneui::Color pressedThumb{7, 8, 9};
    oneui::SliderStyleOverride style;
    oneui::SliderStateStyleOverride pressed;
    pressed.thumbBackground = pressedThumb;
    style.pressed = pressed;

    oneui::SliderStateStyleOverride focus;
    oneui::FocusRingStyleOverride ring;
    ring.visible = false;
    focus.focusRing = ring;
    style.focusVisible = focus;
    slider.setStyleOverride(style);

    slider.onMouseDown(oneui::MouseEvent{oneui::Point{60.0f, 16.0f}});
    RecordingCanvas canvas;
    slider.paint(canvas);

    expectEqual("Slider style override pressed thumb", countFillRectsWithColor(canvas, pressedThumb), 1);
    expectEqual("Slider style override hides focus ring", countPrimaryOrThickStrokeRects(canvas), 0);
}

void testSliderDisabledStyleOverrideWinsAndClearRestoresDefault() {
    oneui::Slider slider;
    slider.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 32.0f});
    slider.setValue(0.5);

    const oneui::Color hoveredThumb{10, 20, 30};
    const oneui::Color disabledThumb{40, 50, 60};
    oneui::SliderStyleOverride style;
    oneui::SliderStateStyleOverride hovered;
    hovered.thumbBackground = hoveredThumb;
    style.hovered = hovered;
    oneui::SliderStateStyleOverride disabled;
    disabled.thumbBackground = disabledThumb;
    disabled.trackFill = oneui::Color{70, 80, 90};
    style.disabled = disabled;
    slider.setStyleOverride(style);

    slider.onMouseMove(oneui::MouseEvent{oneui::Point{60.0f, 16.0f}});
    slider.setDisabled(true);

    RecordingCanvas disabledCanvas;
    slider.paint(disabledCanvas);

    expectEqual("Slider disabled style override wins over hover", countFillRectsWithColor(disabledCanvas, disabledThumb), 1);
    expectEqual("Slider hovered style override ignored while disabled", countFillRectsWithColor(disabledCanvas, hoveredThumb), 0);

    slider.setDisabled(false);
    slider.clearStyleOverride();
    RecordingCanvas defaultCanvas;
    slider.paint(defaultCanvas);

    expectEqual("Slider clearStyleOverride restores default fill and thumb", countFillRectsWithColor(defaultCanvas, oneui::theme().primary), 2);
}

void testNestedButtonKeyboardFocusVisible() {
    oneui::View outer;
    outer.setFrame(oneui::Rect{0.0f, 0.0f, 300.0f, 120.0f});

    auto inner = std::make_shared<oneui::View>();
    inner->setFrame(oneui::Rect{10.0f, 10.0f, 220.0f, 80.0f});

    auto button = std::make_shared<oneui::Button>(L"Nested");
    button->setFrame(oneui::Rect{20.0f, 20.0f, 100.0f, 36.0f});

    inner->add(button);
    outer.add(inner);
    outer.onFocusChanged(true);

    expectEqual("Nested Button initial focus is not focus-visible", button->focusVisible() ? 1 : 0, 0);

    outer.onKeyDown(oneui::KeyEvent{oneui::Key::Tab});

    expectEqual("Nested Button Tab focus focused", button->focused() ? 1 : 0, 1);
    expectEqual("Nested Button Tab focus is focus-visible", button->focusVisible() ? 1 : 0, 1);
}

void testFieldMouseFocusIsNotFocusVisible() {
    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 260.0f, 120.0f});

    auto textField = std::make_shared<oneui::TextField>(L"Name");
    textField->setFrame(oneui::Rect{10.0f, 10.0f, 120.0f, 30.0f});

    auto select = std::make_shared<oneui::Select>();
    select->setFrame(oneui::Rect{10.0f, 50.0f, 120.0f, 30.0f});
    select->setItems({L"One", L"Two"});

    view.add(textField);
    view.add(select);

    view.onMouseDown(oneui::MouseEvent{oneui::Point{20.0f, 20.0f}});
    expectEqual("TextField mouse focus focused", textField->focused() ? 1 : 0, 1);
    expectEqual("TextField mouse focus is not focus-visible", textField->focusVisible() ? 1 : 0, 0);
    RecordingCanvas textFieldCanvas;
    textField->paint(textFieldCanvas);
    expectEqual("TextField mouse focus does not paint keyboard focus ring", countPrimaryOrThickStrokeRects(textFieldCanvas), 0);

    view.onMouseDown(oneui::MouseEvent{oneui::Point{20.0f, 60.0f}});
    expectEqual("Select mouse focus focused", select->focused() ? 1 : 0, 1);
    expectEqual("Select mouse focus is not focus-visible", select->focusVisible() ? 1 : 0, 0);
    RecordingCanvas selectCanvas;
    select->paint(selectCanvas);
    expectEqual("Select mouse focus does not paint keyboard focus ring", countPrimaryOrThickStrokeRects(selectCanvas), 0);
}

void testFieldKeyboardFocusVisible() {
    oneui::OverlayHost host;
    host.setFrame(oneui::Rect{0.0f, 0.0f, 260.0f, 120.0f});

    auto view = std::make_shared<oneui::View>();
    view->setFrame(oneui::Rect{0.0f, 0.0f, 260.0f, 120.0f});

    auto textField = std::make_shared<oneui::TextField>(L"Name");
    textField->setFrame(oneui::Rect{10.0f, 10.0f, 120.0f, 30.0f});

    auto select = std::make_shared<oneui::Select>();
    select->setFrame(oneui::Rect{10.0f, 50.0f, 120.0f, 30.0f});
    select->setItems({L"One", L"Two"});

    view->add(textField);
    view->add(select);
    host.setContent(view);
    host.onFocusChanged(true);
    host.setFocusVisible(true);

    host.onKeyDown(oneui::KeyEvent{oneui::Key::Tab});
    expectEqual("Select keyboard focus focused", select->focused() ? 1 : 0, 1);
    expectEqual("Select keyboard focus is focus-visible", select->focusVisible() ? 1 : 0, 1);

    host.onKeyDown(oneui::KeyEvent{oneui::Key::Tab});
    expectEqual("TextField keyboard focus focused", textField->focused() ? 1 : 0, 1);
    expectEqual("TextField keyboard focus is focus-visible", textField->focusVisible() ? 1 : 0, 1);
}

void openSelectForPaint(oneui::Select& select) {
    select.onMouseDown(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});
    select.onMouseUp(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});
}

void testSelectStyleOverridePaintsCustomColorsAndPopupGeometry() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 140.0f, 32.0f});
    select.setItems({L"Windows", L"Linux", L"macOS"});
    select.setSelectedIndex(1);

    const oneui::Color background{12, 34, 56};
    const oneui::Color foreground{230, 240, 250};
    const oneui::Color border{90, 110, 130};
    const oneui::Color arrow{200, 210, 220};
    const oneui::Color popupBackground{245, 247, 250};
    const oneui::Color popupBorder{55, 65, 81};
    const oneui::Color popupShadow{1, 2, 3, 44};
    const oneui::Color hoverBackground{224, 231, 255};
    const oneui::Color selectedBackground{220, 252, 231};
    const oneui::Color selectedForeground{22, 101, 52};

    oneui::SelectStyleOverride style;
    oneui::SelectStateStyleOverride normal;
    normal.background = background;
    normal.foreground = foreground;
    normal.border = border;
    normal.arrowColor = arrow;
    normal.popupBackground = popupBackground;
    normal.popupBorder = popupBorder;
    normal.popupShadow = popupShadow;
    normal.padding = oneui::Insets{0.0f, 14.0f};
    normal.popupOffset = 8.0f;
    normal.optionInset = oneui::Insets{3.0f, 5.0f};
    style.normal = normal;
    oneui::SelectStateStyleOverride hovered;
    hovered.optionBackground = hoverBackground;
    style.hovered = hovered;
    oneui::SelectStateStyleOverride selected;
    selected.selectedOptionBackground = selectedBackground;
    selected.selectedOptionForeground = selectedForeground;
    style.selected = selected;
    select.setStyleOverride(style);

    openSelectForPaint(select);
    select.onMouseMove(oneui::MouseEvent{oneui::Point{10.0f, 44.0f}});

    RecordingCanvas canvas;
    select.paint(canvas);

    expectEqual("Select style override field background", countFillRectsWithColor(canvas, background), 1);
    expectEqual("Select style override field border", countStrokeRectsWithColor(canvas, border), 1);
    expectEqual("Select style override field text", countTextsWithTextAndColor(canvas, L"Linux", foreground), 1);
    expectEqual("Select style override arrow", countLinesWithColor(canvas, arrow), 2);
    expectEqual("Select style override popup background", countFillRectsWithColor(canvas, popupBackground), 1);
    expectEqual("Select style override popup border", countStrokeRectsWithColor(canvas, popupBorder), 1);
    expectEqual("Select style override popup shadow", countFillRectsWithColor(canvas, popupShadow), 1);
    expectEqual("Select style override hovered option", countFillRectsWithColor(canvas, hoverBackground), 1);
    expectEqual("Select style override selected option", countFillRectsWithColor(canvas, selectedBackground), 1);
    expectEqual("Select style override selected option text", countTextsWithTextAndColor(canvas, L"Linux", selectedForeground), 1);
    if (canvas.fillRects.size() >= 3) {
        expectRect("Select popup offset follows style", canvas.fillRects[2].rect, oneui::Rect{0.0f, 40.0f, 140.0f, 96.0f});
    }
}

void testSelectPopupGeometryUsesPopupPlacementAdapter() {
    oneui::Select select;
    const oneui::Rect anchor{20.0f, 30.0f, 80.0f, 24.0f};
    select.setFrame(anchor);
    select.setItems({L"Windows", L"Linux"});

    const oneui::Color popupBackground{17, 24, 39};
    oneui::SelectStyleOverride style;
    oneui::SelectStateStyleOverride normal;
    normal.popupBackground = popupBackground;
    normal.popupOffset = 6.0f;
    style.normal = normal;
    select.setStyleOverride(style);

    select.onKeyDown(oneui::KeyEvent{oneui::Key::Space});

    RecordingCanvas canvas;
    select.paint(canvas);

    const auto expected = oneui::PopupPlacement::resolve(oneui::PopupPlacementRequest{
        anchor,
        oneui::Size{80.0f, 56.0f},
        oneui::Rect{-1000.0f, -1000.0f, 2000.0f, 2000.0f},
        oneui::PopupPreferredPlacement::BottomStart,
        6.0f
    });

    bool found = false;
    for (const auto& fill : canvas.fillRects) {
        if (sameColor(fill.color, popupBackground)) {
            expectRect("Select popup geometry delegates to PopupPlacement", fill.rect, expected.rect);
            found = true;
        }
    }
    expectEqual("Select popup geometry paints popup background", found ? 1 : 0, 1);
}

void testSelectEmptyStyleOverrideKeepsDefaultPaint() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 140.0f, 32.0f});
    select.setItems({L"Windows", L"Linux"});
    select.setSelectedIndex(1);
    select.setStyleOverride(oneui::SelectStyleOverride{});

    RecordingCanvas canvas;
    select.paint(canvas);

    expectEqual("Select empty style override keeps field background", countFillRectsWithColor(canvas, oneui::theme().surface), 1);
    expectEqual("Select empty style override keeps field border", countStrokeRectsWithColor(canvas, oneui::theme().border), 1);
    expectEqual("Select empty style override keeps field text", countTextsWithTextAndColor(canvas, L"Linux", oneui::theme().text), 1);
}

void testSelectStyleOverrideCanHideFocusRingAndStylePressed() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 140.0f, 32.0f});
    select.setItems({L"Windows", L"Linux"});
    select.onFocusChanged(true);
    select.setFocusVisible(true);

    const oneui::Color pressedBackground{30, 41, 59};
    oneui::SelectStyleOverride style;
    oneui::SelectStateStyleOverride pressed;
    pressed.background = pressedBackground;
    style.pressed = pressed;
    oneui::SelectStateStyleOverride focus;
    oneui::FocusRingStyleOverride ring;
    ring.visible = false;
    focus.focusRing = ring;
    style.focusVisible = focus;
    select.setStyleOverride(style);

    select.onMouseDown(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});

    RecordingCanvas canvas;
    select.paint(canvas);

    expectEqual("Select pressed style background", countFillRectsWithColor(canvas, pressedBackground), 1);
    expectEqual("Select style override hides focus ring", countPrimaryOrThickStrokeRects(canvas), 0);
}

void testSelectDisabledStyleOverrideWinsAndClearRestoresDefault() {
    oneui::Select select;
    select.setFrame(oneui::Rect{0.0f, 0.0f, 140.0f, 32.0f});
    select.setItems({L"Windows", L"Linux"});

    const oneui::Color hoveredBackground{224, 231, 255};
    const oneui::Color disabledBackground{15, 23, 42};
    const oneui::Color disabledForeground{148, 163, 184};
    oneui::SelectStyleOverride style;
    oneui::SelectStateStyleOverride hovered;
    hovered.background = hoveredBackground;
    style.hovered = hovered;
    oneui::SelectStateStyleOverride disabled;
    disabled.background = disabledBackground;
    disabled.foreground = disabledForeground;
    disabled.arrowColor = disabledForeground;
    style.disabled = disabled;
    select.setStyleOverride(style);

    select.onMouseMove(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});
    select.setDisabled(true);

    RecordingCanvas disabledCanvas;
    select.paint(disabledCanvas);

    expectEqual("Select disabled style background wins", countFillRectsWithColor(disabledCanvas, disabledBackground), 1);
    expectEqual("Select hovered field style ignored while disabled", countFillRectsWithColor(disabledCanvas, hoveredBackground), 0);
    expectEqual("Select disabled style foreground wins", countTextsWithColor(disabledCanvas, disabledForeground), 1);

    select.setDisabled(false);
    select.clearStyleOverride();
    RecordingCanvas defaultCanvas;
    select.paint(defaultCanvas);

    expectEqual("Select clearStyleOverride restores default field background", countFillRectsWithColor(defaultCanvas, oneui::theme().surface), 1);
    expectEqual("Select clearStyleOverride removes disabled background", countFillRectsWithColor(defaultCanvas, disabledBackground), 0);
}

void testTextFieldCaretEditingKeys() {
    oneui::TextField field(L"Name");
    field.setText(L"abcd");

    int changes = 0;
    field.setOnChanged([&](const std::wstring&) {
        ++changes;
    });

    field.setCaretIndex(2);
    expectEqual("TextField setCaretIndex clamps inside text", static_cast<int>(field.caretIndex()), 2);

    field.onTextInput(L'X');
    expectWideEqual("TextField inserts text at caret", field.text(), L"abXcd");
    expectEqual("TextField insertion advances caret", static_cast<int>(field.caretIndex()), 3);
    expectEqual("TextField insertion emits once", changes, 1);

    field.onKeyDown(oneui::KeyEvent{oneui::Key::Home});
    expectEqual("TextField Home moves caret to start", static_cast<int>(field.caretIndex()), 0);
    expectEqual("TextField Home does not emit change", changes, 1);

    field.onKeyDown(oneui::KeyEvent{oneui::Key::Delete});
    expectWideEqual("TextField Delete removes at caret", field.text(), L"bXcd");
    expectEqual("TextField Delete keeps caret at removal index", static_cast<int>(field.caretIndex()), 0);
    expectEqual("TextField Delete emits once", changes, 2);

    field.onKeyDown(oneui::KeyEvent{oneui::Key::End});
    expectEqual("TextField End moves caret to end", static_cast<int>(field.caretIndex()), 4);
    expectEqual("TextField End does not emit change", changes, 2);

    field.onKeyDown(oneui::KeyEvent{oneui::Key::Backspace});
    expectWideEqual("TextField Backspace removes before caret", field.text(), L"bXc");
    expectEqual("TextField Backspace moves caret left", static_cast<int>(field.caretIndex()), 3);
    expectEqual("TextField Backspace emits once", changes, 3);

    field.onKeyDown(oneui::KeyEvent{oneui::Key::Delete});
    expectWideEqual("TextField Delete at end is noop", field.text(), L"bXc");
    expectEqual("TextField Delete at end keeps change count", changes, 3);
}

void testTextFieldSelectionEditingKeys() {
    oneui::TextField field(L"Name");
    field.setText(L"abcde");

    int changes = 0;
    field.setOnChanged([&](const std::wstring&) {
        ++changes;
    });

    field.setCaretIndex(1);
    field.onKeyDown(oneui::KeyEvent{oneui::Key::Right, true});
    field.onKeyDown(oneui::KeyEvent{oneui::Key::Right, true});

    expectEqual("TextField Shift+Right creates selection", field.hasSelection() ? 1 : 0, 1);
    expectEqual("TextField selection start", static_cast<int>(field.selectionStart()), 1);
    expectEqual("TextField selection end", static_cast<int>(field.selectionEnd()), 3);
    expectWideEqual("TextField selectedText", field.selectedText(), L"bc");

    field.onTextInput(L'X');

    expectWideEqual("TextField typing replaces selection", field.text(), L"aXde");
    expectEqual("TextField typing replacement caret", static_cast<int>(field.caretIndex()), 2);
    expectEqual("TextField typing replacement clears selection", field.hasSelection() ? 1 : 0, 0);
    expectEqual("TextField typing replacement emits once", changes, 1);

    field.setSelectionRange(1, 3);
    field.onKeyDown(oneui::KeyEvent{oneui::Key::Delete});

    expectWideEqual("TextField Delete removes selection", field.text(), L"ae");
    expectEqual("TextField Delete selection caret", static_cast<int>(field.caretIndex()), 1);
    expectEqual("TextField Delete selection emits once", changes, 2);

    field.selectAll();
    field.onKeyDown(oneui::KeyEvent{oneui::Key::Backspace});

    expectWideEqual("TextField Backspace removes selectAll", field.text(), L"");
    expectEqual("TextField Backspace selectAll caret", static_cast<int>(field.caretIndex()), 0);
    expectEqual("TextField Backspace selectAll clears selection", field.hasSelection() ? 1 : 0, 0);
    expectEqual("TextField Backspace selectAll emits once", changes, 3);
}

void testTextFieldMouseDragSelection() {
    oneui::TextField field(L"Name");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 36.0f});
    field.setText(L"abcdef");

    expectEqual("TextField mouse down starts caret placement", field.onMouseDown(oneui::MouseEvent{oneui::Point{19.0f, 18.0f}}) ? 1 : 0, 1);
    expectEqual("TextField mouse down places caret", static_cast<int>(field.caretIndex()), 1);
    expectEqual("TextField mouse down clears selection", field.hasSelection() ? 1 : 0, 0);

    expectEqual("TextField mouse drag selection handled", field.onMouseMove(oneui::MouseEvent{oneui::Point{40.0f, 18.0f}}) ? 1 : 0, 1);
    expectEqual("TextField mouse drag creates selection", field.hasSelection() ? 1 : 0, 1);
    expectEqual("TextField mouse drag selection start", static_cast<int>(field.selectionStart()), 1);
    expectEqual("TextField mouse drag selection end", static_cast<int>(field.selectionEnd()), 4);
    expectWideEqual("TextField mouse drag selected text", field.selectedText(), L"bcd");

    expectEqual("TextField mouse up ends selection capture", field.onMouseUp(oneui::MouseEvent{oneui::Point{40.0f, 18.0f}}) ? 1 : 0, 1);
    field.onMouseMove(oneui::MouseEvent{oneui::Point{54.0f, 18.0f}});
    expectEqual("TextField mouse move after release preserves selection start", static_cast<int>(field.selectionStart()), 1);
    expectEqual("TextField mouse move after release preserves selection end", static_cast<int>(field.selectionEnd()), 4);
}

void testTextFieldClipboardOperations() {
    oneui::TextField field(L"Name");
    field.setText(L"abcdef");
    field.setSelectionRange(1, 4);

    oneui::MemoryClipboard clipboard;
    int changes = 0;
    field.setOnChanged([&](const std::wstring&) {
        ++changes;
    });

    expectEqual("TextField copy selection handled", field.copySelectionToClipboard(clipboard) ? 1 : 0, 1);
    expectWideEqual("TextField copy writes selected text", clipboard.text(), L"bcd");
    expectWideEqual("TextField copy preserves text", field.text(), L"abcdef");
    expectEqual("TextField copy does not emit change", changes, 0);

    expectEqual("TextField cut selection handled", field.cutSelectionToClipboard(clipboard) ? 1 : 0, 1);
    expectWideEqual("TextField cut writes selected text", clipboard.text(), L"bcd");
    expectWideEqual("TextField cut removes selected text", field.text(), L"aef");
    expectEqual("TextField cut caret at removal start", static_cast<int>(field.caretIndex()), 1);
    expectEqual("TextField cut clears selection", field.hasSelection() ? 1 : 0, 0);
    expectEqual("TextField cut emits once", changes, 1);

    oneui::MemoryClipboard pasteClipboard;
    pasteClipboard.setText(L"XYZ");
    field.setCaretIndex(2);

    expectEqual("TextField paste handled", field.pasteFromClipboard(pasteClipboard) ? 1 : 0, 1);
    expectWideEqual("TextField paste inserts at caret", field.text(), L"aeXYZf");
    expectEqual("TextField paste advances caret", static_cast<int>(field.caretIndex()), 5);
    expectEqual("TextField paste emits once", changes, 2);

    field.setSelectionRange(2, 5);
    pasteClipboard.setText(L"Q");

    expectEqual("TextField paste replaces selection", field.pasteFromClipboard(pasteClipboard) ? 1 : 0, 1);
    expectWideEqual("TextField paste replacement text", field.text(), L"aeQf");
    expectEqual("TextField paste replacement caret", static_cast<int>(field.caretIndex()), 3);
    expectEqual("TextField paste replacement emits once", changes, 3);
}

void testTextFieldClipboardKeyboardShortcuts() {
    oneui::TextField field(L"Name");
    field.setText(L"abcdef");
    auto clipboard = std::make_shared<oneui::MemoryClipboard>();
    field.setClipboard(clipboard);

    int changes = 0;
    field.setOnChanged([&](const std::wstring&) {
        ++changes;
    });

    field.setSelectionRange(1, 4);
    expectEqual("TextField Ctrl+C handled", field.onKeyDown(oneui::KeyEvent{oneui::Key::C, false, true}) ? 1 : 0, 1);
    expectWideEqual("TextField Ctrl+C copies selection", clipboard->text(), L"bcd");
    expectWideEqual("TextField Ctrl+C preserves text", field.text(), L"abcdef");
    expectEqual("TextField Ctrl+C no change", changes, 0);

    expectEqual("TextField Ctrl+X handled", field.onKeyDown(oneui::KeyEvent{oneui::Key::X, false, true}) ? 1 : 0, 1);
    expectWideEqual("TextField Ctrl+X cuts selection", clipboard->text(), L"bcd");
    expectWideEqual("TextField Ctrl+X updates text", field.text(), L"aef");
    expectEqual("TextField Ctrl+X emits once", changes, 1);

    field.setCaretIndex(2);
    clipboard->setText(L"Q");
    expectEqual("TextField Ctrl+V handled", field.onKeyDown(oneui::KeyEvent{oneui::Key::V, false, true}) ? 1 : 0, 1);
    expectWideEqual("TextField Ctrl+V pastes text", field.text(), L"aeQf");
    expectEqual("TextField Ctrl+V emits once", changes, 2);

    expectEqual("TextField Ctrl+A handled", field.onKeyDown(oneui::KeyEvent{oneui::Key::A, false, true}) ? 1 : 0, 1);
    expectEqual("TextField Ctrl+A selects all", field.hasSelection() ? 1 : 0, 1);
    expectWideEqual("TextField Ctrl+A selected text", field.selectedText(), L"aeQf");
}

void testTextFieldUndoRedoEditingPaths() {
    oneui::TextField field(L"Name");
    field.setText(L"abcde");

    oneui::MemoryClipboard clipboard;
    int changes = 0;
    field.setOnChanged([&](const std::wstring&) {
        ++changes;
    });

    field.setCaretIndex(2);
    field.onKeyDown(oneui::KeyEvent{oneui::Key::Backspace});
    expectWideEqual("TextField undo setup Backspace text", field.text(), L"acde");
    expectEqual("TextField undo setup Backspace caret", static_cast<int>(field.caretIndex()), 1);
    expectEqual("TextField undo setup Backspace emits once", changes, 1);

    field.onKeyDown(oneui::KeyEvent{oneui::Key::Delete});
    expectWideEqual("TextField undo setup Delete text", field.text(), L"ade");
    expectEqual("TextField undo setup Delete caret", static_cast<int>(field.caretIndex()), 1);
    expectEqual("TextField undo setup Delete emits once", changes, 2);

    clipboard.setText(L"XYZ");
    field.setSelectionRange(1, 2);
    field.pasteFromClipboard(clipboard);
    expectWideEqual("TextField undo setup paste text", field.text(), L"aXYZe");
    expectEqual("TextField undo setup paste caret", static_cast<int>(field.caretIndex()), 4);
    expectEqual("TextField undo setup paste emits once", changes, 3);

    field.setSelectionRange(1, 4);
    field.cutSelectionToClipboard(clipboard);
    expectWideEqual("TextField undo setup cut text", field.text(), L"ae");
    expectEqual("TextField undo setup cut caret", static_cast<int>(field.caretIndex()), 1);
    expectEqual("TextField undo setup cut emits once", changes, 4);

    expectEqual("TextField undo cut handled", field.undo() ? 1 : 0, 1);
    expectWideEqual("TextField undo restores cut text", field.text(), L"aXYZe");
    expectEqual("TextField undo restores cut selection", field.hasSelection() ? 1 : 0, 1);
    expectEqual("TextField undo restores cut selection start", static_cast<int>(field.selectionStart()), 1);
    expectEqual("TextField undo restores cut selection end", static_cast<int>(field.selectionEnd()), 4);
    expectEqual("TextField undo cut emits once", changes, 5);

    expectEqual("TextField undo paste handled", field.undo() ? 1 : 0, 1);
    expectWideEqual("TextField undo restores paste text", field.text(), L"ade");
    expectEqual("TextField undo restores paste selection", field.hasSelection() ? 1 : 0, 1);
    expectEqual("TextField undo restores paste selection start", static_cast<int>(field.selectionStart()), 1);
    expectEqual("TextField undo restores paste selection end", static_cast<int>(field.selectionEnd()), 2);
    expectEqual("TextField undo paste emits once", changes, 6);

    expectEqual("TextField undo Delete handled", field.undo() ? 1 : 0, 1);
    expectWideEqual("TextField undo restores Delete text", field.text(), L"acde");
    expectEqual("TextField undo restores Delete caret", static_cast<int>(field.caretIndex()), 1);
    expectEqual("TextField undo Delete emits once", changes, 7);

    expectEqual("TextField undo Backspace handled", field.undo() ? 1 : 0, 1);
    expectWideEqual("TextField undo restores Backspace text", field.text(), L"abcde");
    expectEqual("TextField undo restores Backspace caret", static_cast<int>(field.caretIndex()), 2);
    expectEqual("TextField undo Backspace emits once", changes, 8);

    expectEqual("TextField undo empty history is noop", field.undo() ? 1 : 0, 0);
    expectEqual("TextField undo empty history emits no change", changes, 8);

    expectEqual("TextField redo Backspace handled", field.redo() ? 1 : 0, 1);
    expectWideEqual("TextField redo reapplies Backspace text", field.text(), L"acde");
    expectEqual("TextField redo Backspace caret", static_cast<int>(field.caretIndex()), 1);
    expectEqual("TextField redo Backspace emits once", changes, 9);

    expectEqual("TextField redo Delete handled", field.redo() ? 1 : 0, 1);
    expectWideEqual("TextField redo reapplies Delete text", field.text(), L"ade");
    expectEqual("TextField redo Delete caret", static_cast<int>(field.caretIndex()), 1);
    expectEqual("TextField redo Delete emits once", changes, 10);

    expectEqual("TextField redo paste handled", field.redo() ? 1 : 0, 1);
    expectWideEqual("TextField redo reapplies paste text", field.text(), L"aXYZe");
    expectEqual("TextField redo paste caret", static_cast<int>(field.caretIndex()), 4);
    expectEqual("TextField redo paste emits once", changes, 11);

    expectEqual("TextField redo cut handled", field.redo() ? 1 : 0, 1);
    expectWideEqual("TextField redo reapplies cut text", field.text(), L"ae");
    expectEqual("TextField redo cut caret", static_cast<int>(field.caretIndex()), 1);
    expectEqual("TextField redo cut emits once", changes, 12);
}

void testTextFieldUndoRedoTextInputAndBinding() {
    oneui::State<std::wstring> state(L"hi");
    oneui::TextField field(L"Name");
    field.bindText(state);

    int changes = 0;
    field.setOnChanged([&](const std::wstring&) {
        ++changes;
    });

    field.setCaretIndex(2);
    field.onTextInput(L'!');
    expectWideEqual("TextField undo text input updates field", field.text(), L"hi!");
    expectWideEqual("TextField undo text input updates binding", state.get(), L"hi!");
    expectEqual("TextField undo text input emits once", changes, 1);

    expectEqual("TextField undo text input handled", field.undo() ? 1 : 0, 1);
    expectWideEqual("TextField undo text input restores field", field.text(), L"hi");
    expectWideEqual("TextField undo text input restores binding", state.get(), L"hi");
    expectEqual("TextField undo text input restores caret", static_cast<int>(field.caretIndex()), 2);
    expectEqual("TextField undo text input emits once", changes, 2);

    expectEqual("TextField redo text input handled", field.redo() ? 1 : 0, 1);
    expectWideEqual("TextField redo text input restores field", field.text(), L"hi!");
    expectWideEqual("TextField redo text input restores binding", state.get(), L"hi!");
    expectEqual("TextField redo text input restores caret", static_cast<int>(field.caretIndex()), 3);
    expectEqual("TextField redo text input emits once", changes, 3);

    state.set(L"external");
    expectWideEqual("TextField external binding update applies", field.text(), L"external");
    expectEqual("TextField external binding update clears undo", field.undo() ? 1 : 0, 0);
    expectEqual("TextField external binding update does not call onChanged", changes, 3);
}

void testTextAreaSupportsMultilineEditingAndLineNavigation() {
    oneui::TextArea area(L"Write notes");
    expectEqual("TextArea enables multiline mode", area.multiline() ? 1 : 0, 1);
    expectNear("TextArea default preferred height", area.preferredSize().height, 160.0f);

    area.setText(L"alpha\nbeta\ngamma");
    area.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 160.0f});
    RecordingCanvas canvas;
    area.paint(canvas);
    expectEqual("TextArea paints one draw call per line", static_cast<int>(canvas.texts.size()), 3);
    if (canvas.texts.size() == 3) {
        expectWideEqual("TextArea first painted line", canvas.texts[0].text, L"alpha");
        expectWideEqual("TextArea second painted line", canvas.texts[1].text, L"beta");
        expectWideEqual("TextArea third painted line", canvas.texts[2].text, L"gamma");
        expectNear("TextArea line spacing", canvas.texts[1].rect.y - canvas.texts[0].rect.y, area.lineHeight());
    }
    int changes = 0;
    area.setOnChanged([&](const std::wstring&) {
        ++changes;
    });
    area.setCaretIndex(8);

    expectEqual("TextArea Up moves to matching column", area.onKeyDown(oneui::KeyEvent{oneui::Key::Up}) ? 1 : 0, 1);
    expectEqual("TextArea Up caret", static_cast<int>(area.caretIndex()), 2);
    expectEqual("TextArea Down moves to matching column", area.onKeyDown(oneui::KeyEvent{oneui::Key::Down}) ? 1 : 0, 1);
    expectEqual("TextArea Down caret", static_cast<int>(area.caretIndex()), 8);
    expectEqual("TextArea End moves to current line end", area.onKeyDown(oneui::KeyEvent{oneui::Key::End}) ? 1 : 0, 1);
    expectEqual("TextArea End caret", static_cast<int>(area.caretIndex()), 10);
    expectEqual("TextArea Home moves to current line start", area.onKeyDown(oneui::KeyEvent{oneui::Key::Home}) ? 1 : 0, 1);
    expectEqual("TextArea Home caret", static_cast<int>(area.caretIndex()), 6);

    expectEqual("TextArea Enter inserts newline", area.onKeyDown(oneui::KeyEvent{oneui::Key::Enter}) ? 1 : 0, 1);
    expectWideEqual("TextArea Enter result", area.text(), L"alpha\n\nbeta\ngamma");
    expectEqual("TextArea Enter emits once", changes, 1);
    expectEqual("TextArea text input handled", area.onTextInput(L'X') ? 1 : 0, 1);
    expectWideEqual("TextArea inserts after newline", area.text(), L"alpha\n\nXbeta\ngamma");
    expectEqual("TextArea text input emits", changes, 2);
}

void testTextFieldDisabledDoesNotEditOrCut() {
    oneui::TextField field(L"Name");
    field.setText(L"abcdef");
    field.setSelectionRange(1, 4);
    field.setDisabled(true);

    oneui::MemoryClipboard clipboard;
    clipboard.setText(L"keep");
    int changes = 0;
    field.setOnChanged([&](const std::wstring&) {
        ++changes;
    });

    expectEqual("TextField disabled text input ignored", field.onTextInput(L'Z') ? 1 : 0, 0);
    expectEqual("TextField disabled backspace ignored", field.onKeyDown(oneui::KeyEvent{oneui::Key::Backspace}) ? 1 : 0, 0);
    expectEqual("TextField disabled paste ignored", field.pasteFromClipboard(clipboard) ? 1 : 0, 0);
    expectEqual("TextField disabled cut ignored", field.cutSelectionToClipboard(clipboard) ? 1 : 0, 0);
    expectWideEqual("TextField disabled keeps text", field.text(), L"abcdef");
    expectWideEqual("TextField disabled cut keeps clipboard", clipboard.text(), L"keep");
    expectEqual("TextField disabled emits no changes", changes, 0);
}

void testTextFieldReadOnlyAllowsSelectionCopyAndNavigationButNotMutation() {
    oneui::TextField field(L"Name");
    field.setText(L"abcdef");
    field.setSelectionRange(1, 4);
    field.setReadOnly(true);

    oneui::MemoryClipboard clipboard;
    clipboard.setText(L"keep");
    field.setClipboard(std::make_shared<oneui::MemoryClipboard>());

    int changes = 0;
    field.setOnChanged([&](const std::wstring&) {
        ++changes;
    });

    const auto info = field.accessibilityInfo();
    expectEqual("TextField readOnly property", field.readOnly() ? 1 : 0, 1);
    expectEqual("TextField readOnly accessibility state", info.state.readOnly ? 1 : 0, 1);
    expectEqual("TextField readOnly still focusable", field.isFocusable() ? 1 : 0, 1);

    expectEqual("TextField readOnly copy selection handled", field.copySelectionToClipboard(clipboard) ? 1 : 0, 1);
    expectWideEqual("TextField readOnly copy writes selected text", clipboard.text(), L"bcd");

    clipboard.setText(L"keep");
    expectEqual("TextField readOnly cut ignored", field.cutSelectionToClipboard(clipboard) ? 1 : 0, 0);
    expectWideEqual("TextField readOnly cut keeps text", field.text(), L"abcdef");
    expectWideEqual("TextField readOnly cut keeps clipboard", clipboard.text(), L"keep");

    clipboard.setText(L"XYZ");
    expectEqual("TextField readOnly paste ignored", field.pasteFromClipboard(clipboard) ? 1 : 0, 0);
    expectWideEqual("TextField readOnly paste keeps text", field.text(), L"abcdef");

    expectEqual("TextField readOnly text input ignored", field.onTextInput(L'Z') ? 1 : 0, 0);
    expectEqual("TextField readOnly Delete ignored", field.onKeyDown(oneui::KeyEvent{oneui::Key::Delete}) ? 1 : 0, 0);
    expectEqual("TextField readOnly Backspace ignored", field.onKeyDown(oneui::KeyEvent{oneui::Key::Backspace}) ? 1 : 0, 0);
    expectWideEqual("TextField readOnly mutation keys keep text", field.text(), L"abcdef");

    field.clearSelection();
    field.setCaretIndex(2);
    expectEqual("TextField readOnly Right navigates", field.onKeyDown(oneui::KeyEvent{oneui::Key::Right}) ? 1 : 0, 1);
    expectEqual("TextField readOnly Right updates caret", static_cast<int>(field.caretIndex()), 3);
    expectEqual("TextField readOnly Shift+Right navigates selection", field.onKeyDown(oneui::KeyEvent{oneui::Key::Right, true}) ? 1 : 0, 1);
    expectEqual("TextField readOnly can select", field.hasSelection() ? 1 : 0, 1);

    field.selectAll();
    expectEqual("TextField readOnly Ctrl+A handled", field.onKeyDown(oneui::KeyEvent{oneui::Key::A, false, true}) ? 1 : 0, 1);
    expectEqual("TextField readOnly Ctrl+X ignored", field.onKeyDown(oneui::KeyEvent{oneui::Key::X, false, true}) ? 1 : 0, 0);
    expectWideEqual("TextField readOnly Ctrl+X keeps text", field.text(), L"abcdef");
    expectEqual("TextField readOnly emits no changes", changes, 0);
}

void testTextFieldPasswordModeMasksDisplayOnly() {
    oneui::TextField field(L"Password");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 36.0f});
    field.setText(L"secret");
    field.setPasswordMode(true);

    expectEqual("TextField password mode enabled", field.passwordMode() ? 1 : 0, 1);
    expectWideEqual("TextField password mode keeps real text", field.text(), L"secret");

    RecordingCanvas canvas;
    field.paint(canvas);

    expectEqual("TextField password paint emits text", canvas.texts.empty() ? 0 : 1, 1);
    if (!canvas.texts.empty()) {
        expectWideEqual("TextField password paint masks value", canvas.texts.back().text, L"******");
    }

    field.setPasswordMask(L'#');
    RecordingCanvas customMaskCanvas;
    field.paint(customMaskCanvas);
    expectEqual("TextField password custom mask stored", field.passwordMask() == L'#' ? 1 : 0, 1);
    if (!customMaskCanvas.texts.empty()) {
        expectWideEqual("TextField password custom mask paints", customMaskCanvas.texts.back().text, L"######");
    }

    field.setSelectionRange(0, 6);
    oneui::MemoryClipboard clipboard;
    expectEqual("TextField password copy still uses real value", field.copySelectionToClipboard(clipboard) ? 1 : 0, 1);
    expectWideEqual("TextField password copy writes real value", clipboard.text(), L"secret");

    const auto info = field.accessibilityInfo();
    expectWideEqual("TextField password accessibility value is masked", info.value, L"######");
}

void testTextFieldHorizontalScrollClipsAndFollowsCaret() {
    oneui::TextField field(L"Name");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 80.0f, 36.0f});
    field.setText(L"abcdefghijkl");
    field.onFocusChanged(true);

    RecordingCanvas scrolledCanvas;
    field.paint(scrolledCanvas);

    expectEqual("TextField horizontal scroll clips once", static_cast<int>(scrolledCanvas.clips.size()), 1);
    if (!scrolledCanvas.clips.empty()) {
        expectRect("TextField horizontal scroll clips content rect", scrolledCanvas.clips.back(), oneui::Rect{12.0f, 0.0f, 56.0f, 36.0f});
    }
    expectEqual("TextField horizontal scroll balances save", scrolledCanvas.saves, 1);
    expectEqual("TextField horizontal scroll balances restore", scrolledCanvas.restores, 1);
    expectEqual("TextField horizontal scroll paints text", scrolledCanvas.texts.empty() ? 0 : 1, 1);
    if (!scrolledCanvas.texts.empty()) {
        expectWideEqual("TextField horizontal scroll keeps real drawn text", scrolledCanvas.texts.back().text, L"abcdefghijkl");
        expectNear("TextField horizontal scroll shifts text left", scrolledCanvas.texts.back().rect.x, -7.5f);
    }
    expectEqual("TextField horizontal scroll paints caret", countFillRectsWithColor(scrolledCanvas, oneui::theme().primary), 1);
    if (!scrolledCanvas.fillRects.empty()) {
        expectNear("TextField horizontal scroll keeps caret inside content", scrolledCanvas.fillRects.back().rect.x, 66.5f);
    }

    field.onMouseDown(oneui::MouseEvent{oneui::Point{19.0f, 18.0f}});
    expectEqual("TextField horizontal scroll click maps through offset", static_cast<int>(field.caretIndex()), 4);

    field.setCaretIndex(0);
    RecordingCanvas resetCanvas;
    field.paint(resetCanvas);
    if (!resetCanvas.texts.empty()) {
        expectNear("TextField horizontal scroll resets when caret returns", resetCanvas.texts.back().rect.x, 12.0f);
    }
}

void testTextFieldCaretBlinkPaintsAndHidesOnSchedule() {
    oneui::TextField field(L"Name");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 36.0f});
    field.setText(L"abc");
    field.onFocusChanged(true);

    RecordingCanvas visibleCanvas;
    field.paint(visibleCanvas);
    expectEqual("TextField focused caret paints initially", countCaretRects(visibleCanvas), 1);

    const bool keepsBlinkScheduling = field.tickAnimations(testSteadyTimeMs() + 620.0);
    RecordingCanvas hiddenCanvas;
    field.paint(hiddenCanvas);
    expectEqual("TextField focused caret hides after blink interval", countCaretRects(hiddenCanvas), 0);
    expectEqual("TextField focused caret keeps scheduling blink", keepsBlinkScheduling ? 1 : 0, 1);

    field.tickAnimations(testSteadyTimeMs() + 1140.0);
    RecordingCanvas visibleAgainCanvas;
    field.paint(visibleAgainCanvas);
    expectEqual("TextField focused caret returns after blink period", countCaretRects(visibleAgainCanvas), 1);
}

void testTextFieldFocusedEmptyHidesPlaceholderAndOffsetsCaret() {
    oneui::TextField field(L"Enter code");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 42.0f});

    RecordingCanvas unfocusedCanvas;
    field.paint(unfocusedCanvas);
    expectEqual("TextField unfocused empty paints placeholder", countTextsWithText(unfocusedCanvas, L"Enter code"), 1);

    field.onFocusChanged(true);

    RecordingCanvas focusedCanvas;
    field.paint(focusedCanvas);
    expectEqual("TextField focused empty hides placeholder", countTextsWithText(focusedCanvas, L"Enter code"), 0);
    expectEqual("TextField focused empty paints caret", countCaretRects(focusedCanvas), 1);
    if (!focusedCanvas.fillRects.empty()) {
        const auto caret = focusedCanvas.fillRects.back().rect;
        expectNear("TextField focused empty caret offset from content edge", caret.x, 13.5f);
        expectNear("TextField focused empty caret product height", caret.height, 14.0f);
    }
}

void testTextFieldStartsCaretBlinkWhenSchedulerArrivesAfterFocus() {
    oneui::TextField field(L"Name");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 36.0f});
    field.setText(L"abc");
    field.onFocusChanged(true);

    int schedules = 0;
    field.setAnimationScheduler([&] {
        ++schedules;
    });

    expectEqual("TextField schedules caret blink after scheduler install", schedules, 1);
}

void testTextFieldPasswordHorizontalScrollMasksAndClips() {
    oneui::TextField field(L"Password");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 80.0f, 36.0f});
    field.setText(L"supersecret");
    field.setPasswordMode(true);
    field.onFocusChanged(true);

    RecordingCanvas canvas;
    field.paint(canvas);

    expectEqual("TextField password horizontal scroll clips once", static_cast<int>(canvas.clips.size()), 1);
    expectEqual("TextField password horizontal scroll paints text", canvas.texts.empty() ? 0 : 1, 1);
    if (!canvas.texts.empty()) {
        expectWideEqual("TextField password horizontal scroll masks full display", canvas.texts.back().text, L"***********");
        expectNear("TextField password horizontal scroll shifts mask left", canvas.texts.back().rect.x, -7.5f);
    }
    expectEqual("TextField password horizontal scroll keeps real value", field.text() == L"supersecret" ? 1 : 0, 1);
}

void testTextFieldUsesMeasuredTextWidthsForCaretHitTesting() {
    oneui::TextField field(L"Name");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 36.0f});
    field.setText(L"WiWi");

    RecordingCanvas canvas;
    field.paint(canvas);

    field.onMouseDown(oneui::MouseEvent{oneui::Point{23.0f, 18.0f}});
    expectEqual("TextField proportional hit test lands after wide glyph", static_cast<int>(field.caretIndex()), 1);

    field.setSelectionRange(0, 1);
    RecordingCanvas selectionCanvas;
    field.paint(selectionCanvas);
    expectEqual("TextField proportional selection adds overlay fill", static_cast<int>(selectionCanvas.fillRects.size()) >= 2 ? 1 : 0, 1);
    if (selectionCanvas.fillRects.size() >= 2) {
        expectNear("TextField proportional selection width follows measured glyph", selectionCanvas.fillRects[1].rect.width, 9.28571f);
    }
}

void testTextFieldStyleOverridePaintsCustomColors() {
    oneui::TextField field(L"Name");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 36.0f});
    field.setText(L"OneUI");
    field.setSelectionRange(0, 3);
    field.onFocusChanged(true);

    const oneui::Color background{20, 30, 40};
    const oneui::Color foreground{230, 240, 250};
    const oneui::Color border{70, 80, 90};
    const oneui::Color selection{100, 110, 120};
    const oneui::Color caret{130, 140, 150};
    oneui::TextFieldStyleOverride style;
    oneui::TextFieldStateStyleOverride normal;
    normal.background = background;
    normal.foreground = foreground;
    normal.border = border;
    normal.selectionBackground = selection;
    normal.caretColor = caret;
    normal.borderWidth = 3.0f;
    normal.radius = 9.0f;
    normal.padding = oneui::Insets{0.0f, 16.0f};
    normal.shadows = std::vector<oneui::ControlShadowStyle>{
        oneui::ControlShadowStyle{oneui::Color{0, 0, 0, 80}, oneui::Point{0.0f, 2.0f}, 8.0f, 0.0f, false},
        oneui::ControlShadowStyle{oneui::Color{255, 255, 255, 24}, oneui::Point{}, 1.0f, 0.0f, true}};
    style.normal = normal;
    field.setStyleOverride(style);

    RecordingCanvas canvas;
    field.paint(canvas);

    expectEqual("TextField style override background", countFillRectsWithColor(canvas, background), 1);
    expectEqual("TextField style override selection", countFillRectsWithColor(canvas, selection), 1);
    expectEqual("TextField style override border", countStrokeRectsWithColor(canvas, border), 1);
    expectEqual("TextField style override outer shadow", static_cast<int>(canvas.boxShadows.size()), 1);
    expectEqual("TextField style override inset shadow", countStrokeRectsWithColor(canvas, oneui::Color{255, 255, 255, 24}), 1);
    expectEqual("TextField style override text", countTextsWithColor(canvas, foreground), 1);
    expectEqual("TextField style override caret", countFillRectsWithColor(canvas, caret), 1);
    if (!canvas.clips.empty()) {
        expectRect("TextField style override padding affects content clip", canvas.clips.back(), oneui::Rect{16.0f, 0.0f, 148.0f, 36.0f});
    }
}

void testTextFieldEmptyStyleOverrideKeepsDefaultPaint() {
    oneui::TextField field(L"Name");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 36.0f});
    field.setText(L"OneUI");
    field.setStyleOverride(oneui::TextFieldStyleOverride{});

    RecordingCanvas canvas;
    field.paint(canvas);

    expectEqual("TextField empty style override keeps default background", countFillRectsWithColor(canvas, oneui::theme().surface), 1);
    expectEqual("TextField empty style override keeps default text", countTextsWithColor(canvas, oneui::theme().text), 1);
    expectEqual("TextField empty style override keeps default border", countStrokeRectsWithColor(canvas, oneui::theme().border), 1);
}

void testTextFieldStyleOverrideCanHideFocusRingAndStylePlaceholder() {
    oneui::TextField field(L"Name");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 36.0f});
    const oneui::Color placeholder{42, 52, 62};
    oneui::TextFieldStyleOverride style;
    oneui::TextFieldStateStyleOverride normal;
    normal.placeholderForeground = placeholder;
    style.normal = normal;

    oneui::TextFieldStateStyleOverride focus;
    oneui::FocusRingStyleOverride ring;
    ring.visible = false;
    focus.focusRing = ring;
    style.focusVisible = focus;
    field.setStyleOverride(style);

    RecordingCanvas canvas;
    field.paint(canvas);

    expectEqual("TextField style override placeholder", countTextsWithColor(canvas, placeholder), 1);

    field.onFocusChanged(true);
    field.setFocusVisible(true);

    RecordingCanvas focusedCanvas;
    field.paint(focusedCanvas);
    expectEqual("TextField style override hides focused empty placeholder", countTextsWithColor(focusedCanvas, placeholder), 0);
    expectEqual("TextField style override hides focus ring", countPrimaryOrThickStrokeRects(focusedCanvas), 0);
}

void testTextFieldReadOnlyStyleOverridePaintsStateWithoutCaret() {
    oneui::TextField field(L"Name");
    field.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 36.0f});
    field.setText(L"OneUI");
    field.setReadOnly(true);
    field.onFocusChanged(true);

    const oneui::Color background{32, 36, 44};
    const oneui::Color foreground{210, 220, 230};
    const oneui::Color border{80, 90, 105};
    oneui::TextFieldStyleOverride style;
    oneui::TextFieldStateStyleOverride readOnly;
    readOnly.background = background;
    readOnly.foreground = foreground;
    readOnly.border = border;
    style.readOnly = readOnly;
    field.setStyleOverride(style);

    RecordingCanvas canvas;
    field.paint(canvas);

    expectEqual("TextField readOnly style background", countFillRectsWithColor(canvas, background), 1);
    expectEqual("TextField readOnly style foreground", countTextsWithColor(canvas, foreground), 1);
    expectEqual("TextField readOnly style border", countStrokeRectsWithColor(canvas, border), 1);
    expectEqual("TextField readOnly focused does not paint editable caret", canvas.lines.empty() ? 1 : 0, 1);
}

void testCardDrawsConfiguredShadow() {
    oneui::Card card;
    card.setFrame(oneui::Rect{10.0f, 20.0f, 120.0f, 64.0f});
    card.setRadius(10.0f);
    card.setShadow(oneui::BoxShadow{oneui::Color{1, 2, 3, 64}, oneui::Point{0.0f, 6.0f}, 18.0f, 2.0f});

    RecordingCanvas canvas;
    card.paint(canvas);

    expectEqual("Card paints one box shadow", static_cast<int>(canvas.boxShadows.size()), 1);
    if (!canvas.boxShadows.empty()) {
        expectRect("Card shadow uses card frame", canvas.boxShadows[0].rect, oneui::Rect{10.0f, 20.0f, 120.0f, 64.0f});
        expectEqual("Card shadow color r", canvas.boxShadows[0].shadow.color.r, 1);
        expectEqual("Card shadow offset y", static_cast<int>(canvas.boxShadows[0].shadow.offset.y), 6);
        expectNear("Card shadow blur", canvas.boxShadows[0].shadow.blurRadius, 18.0f);
        expectNear("Card shadow spread", canvas.boxShadows[0].shadow.spreadRadius, 2.0f);
        expectNear("Card shadow radius", canvas.boxShadows[0].radius, 10.0f);
    }
}

void testFormFieldHelperErrorAndRequiredMarker() {
    oneui::FormField field;
    field.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 90.0f});
    field.setLabel(L"Project name");
    field.setHelperText(L"Shown in the title");
    field.setErrorText(L"Project key is invalid");
    field.setRequired(true);
    field.setChild(std::make_shared<LayoutProbe>(oneui::Size{0.0f, 24.0f}));

    RecordingCanvas helperCanvas;
    field.paint(helperCanvas);

    expectEqual("FormField helper text paints when valid", countTextsWithTextAndColor(helperCanvas, L"Shown in the title", oneui::theme().formField.helperColor), 1);
    expectEqual("FormField error text is hidden when valid", countTextsWithTextAndColor(helperCanvas, L"Project key is invalid", oneui::theme().formField.errorColor), 0);
    expectEqual("FormField required marker paints", countTextsWithTextAndColor(helperCanvas, L"*", oneui::theme().formField.requiredMarkerColor), 1);

    field.setInvalid(true);

    RecordingCanvas errorCanvas;
    field.paint(errorCanvas);

    expectEqual("FormField helper text is hidden when invalid", countTextsWithTextAndColor(errorCanvas, L"Shown in the title", oneui::theme().formField.helperColor), 0);
    expectEqual("FormField error text paints when invalid", countTextsWithTextAndColor(errorCanvas, L"Project key is invalid", oneui::theme().formField.errorColor), 1);
}

void testFormFieldPropagatesAccessibilityToChild() {
    auto input = std::make_shared<oneui::TextField>(L"Fallback placeholder");

    oneui::FormField field;
    field.setLabel(L"Project key");
    field.setHelperText(L"Use lowercase letters");
    field.setErrorText(L"Project key is invalid");
    field.setRequired(true);
    field.setChild(input);

    auto validInfo = input->accessibilityInfo();
    expectWideEqual("FormField child accessibility name from label", validInfo.name, L"Project key");
    expectWideEqual("FormField child accessibility helper description", validInfo.description, L"Use lowercase letters");
    expectEqual("FormField child accessibility required state", validInfo.state.required ? 1 : 0, 1);
    expectEqual("FormField child accessibility valid state", validInfo.state.invalid ? 1 : 0, 0);

    field.setInvalid(true);

    auto invalidInfo = input->accessibilityInfo();
    expectWideEqual("FormField child accessibility error description", invalidInfo.description, L"Project key is invalid");
    expectEqual("FormField child accessibility invalid state", invalidInfo.state.invalid ? 1 : 0, 1);

    input->setAccessibleName(L"Custom project key");
    field.setLabel(L"Project code");

    auto customInfo = input->accessibilityInfo();
    expectWideEqual("FormField preserves explicit child accessibility name", customInfo.name, L"Custom project key");
}

void testFormFieldStyleOverridePaintsCustomColors() {
    oneui::FormField field;
    field.setFrame(oneui::Rect{0.0f, 0.0f, 220.0f, 90.0f});
    field.setLabel(L"Project name");
    field.setHelperText(L"Helper copy");
    field.setRequired(true);

    const oneui::Color label{12, 34, 56};
    const oneui::Color helper{44, 66, 88};
    const oneui::Color marker{200, 30, 40};
    oneui::FormFieldStyleOverride style;
    style.labelColor = label;
    style.helperColor = helper;
    style.requiredMarkerColor = marker;
    field.setStyleOverride(style);

    RecordingCanvas canvas;
    field.paint(canvas);

    expectEqual("FormField style override label", countTextsWithTextAndColor(canvas, L"Project name", label), 1);
    expectEqual("FormField style override helper", countTextsWithTextAndColor(canvas, L"Helper copy", helper), 1);
    expectEqual("FormField style override required marker", countTextsWithTextAndColor(canvas, L"*", marker), 1);
}

void testValidationMessageStyleOverridePaintsCustomColors() {
    oneui::ValidationMessage helper(L"Helper message");
    helper.setFrame(oneui::Rect{0.0f, 0.0f, 180.0f, 18.0f});

    const oneui::Color helperColor{20, 80, 120};
    const oneui::Color errorColor{180, 20, 20};
    oneui::ValidationMessageStyleOverride style;
    style.helperColor = helperColor;
    style.errorColor = errorColor;
    style.lineHeight = 24.0f;
    helper.setStyleOverride(style);

    RecordingCanvas helperCanvas;
    helper.paint(helperCanvas);

    expectEqual("ValidationMessage helper style override", countTextsWithTextAndColor(helperCanvas, L"Helper message", helperColor), 1);
    expectNear("ValidationMessage line-height style override", helper.preferredSize().height, 24.0f);

    helper.setTone(oneui::ValidationMessageTone::Error);

    RecordingCanvas errorCanvas;
    helper.paint(errorCanvas);

    expectEqual("ValidationMessage error style override", countTextsWithTextAndColor(errorCanvas, L"Helper message", errorColor), 1);
}

void testFormFieldChildLayoutUsesLabelPaddingAndControlWidth() {
    oneui::FormField field;
    field.setFrame(oneui::Rect{10.0f, 20.0f, 200.0f, 120.0f});
    field.setLabel(L"Platform");

    oneui::FormFieldStyleOverride style;
    style.padding = oneui::Insets{2.0f, 6.0f, 4.0f, 8.0f};
    style.labelLineHeight = 10.0f;
    style.labelGap = 3.0f;
    field.setStyleOverride(style);

    auto child = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 24.0f});
    field.setChild(child);

    RecordingCanvas canvas;
    field.paint(canvas);

    expectRect("FormField child frame", child->frame(), oneui::Rect{18.0f, 35.0f, 186.0f, 24.0f});
}

void testWrapLaysOutRowsWithPaddingAndGaps() {
    oneui::Wrap wrap;
    wrap.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    wrap.setPadding(oneui::Insets{4.0f, 6.0f, 8.0f, 10.0f});
    wrap.setGap(5.0f);
    wrap.setRowGap(7.0f);

    auto first = std::make_shared<LayoutProbe>(oneui::Size{40.0f, 12.0f});
    auto hidden = std::make_shared<LayoutProbe>(oneui::Size{90.0f, 50.0f});
    auto second = std::make_shared<LayoutProbe>(oneui::Size{50.0f, 18.0f});
    auto third = std::make_shared<LayoutProbe>(oneui::Size{40.0f, 10.0f});
    hidden->setVisible(false);

    wrap.add(first);
    wrap.add(hidden);
    wrap.add(second);
    wrap.add(third);

    RecordingCanvas canvas;
    wrap.paint(canvas);

    expectRect("Wrap first frame", first->frame(), oneui::Rect{10.0f, 4.0f, 40.0f, 12.0f});
    expectRect("Wrap second frame", second->frame(), oneui::Rect{55.0f, 4.0f, 50.0f, 18.0f});
    expectRect("Wrap third frame", third->frame(), oneui::Rect{10.0f, 29.0f, 40.0f, 10.0f});
    expectRect("Wrap hidden child unchanged", hidden->frame(), oneui::Rect{0.0f, 0.0f, 0.0f, 0.0f});
}

void testDockViewLaysOutRegionsWithPaddingGapAndHiddenChild() {
    oneui::DockView dock;
    dock.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 150.0f});
    dock.setPadding(oneui::Insets{10.0f});
    dock.setGap(5.0f);

    auto top = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 20.0f});
    auto right = std::make_shared<LayoutProbe>(oneui::Size{30.0f, 0.0f});
    auto bottom = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 15.0f});
    auto left = std::make_shared<LayoutProbe>(oneui::Size{25.0f, 0.0f});
    auto center = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});

    dock.setTop(top);
    dock.setRight(right);
    dock.setBottom(bottom);
    dock.setLeft(left);
    dock.setCenter(center);

    RecordingCanvas canvas;
    dock.paint(canvas);

    expectRect("Dock top frame", top->frame(), oneui::Rect{10.0f, 10.0f, 180.0f, 20.0f});
    expectRect("Dock bottom frame", bottom->frame(), oneui::Rect{10.0f, 125.0f, 180.0f, 15.0f});
    expectRect("Dock left frame", left->frame(), oneui::Rect{10.0f, 35.0f, 25.0f, 85.0f});
    expectRect("Dock right frame", right->frame(), oneui::Rect{160.0f, 35.0f, 30.0f, 85.0f});
    expectRect("Dock center frame", center->frame(), oneui::Rect{40.0f, 35.0f, 115.0f, 85.0f});

    left->setVisible(false);
    dock.paint(canvas);

    expectRect("Dock hidden left keeps previous frame", left->frame(), oneui::Rect{10.0f, 35.0f, 25.0f, 85.0f});
    expectRect("Dock center expands past hidden left", center->frame(), oneui::Rect{10.0f, 35.0f, 145.0f, 85.0f});
}

void testAppShellLaysOutProductRegionsAndCollapsibleSidebar() {
    oneui::AppShell shell;
    shell.setFrame(oneui::Rect{0.0f, 0.0f, 1000.0f, 700.0f});
    shell.setPadding(oneui::Insets{10.0f});
    shell.setGap(4.0f);
    shell.setSidebarWidth(200.0f);
    shell.setHeaderHeight(60.0f);
    shell.setFooterHeight(30.0f);

    auto sidebar = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    auto header = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    auto footer = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});

    shell.setSidebar(sidebar);
    shell.setHeader(header);
    shell.setContent(content);
    shell.setFooter(footer);

    RecordingCanvas canvas;
    shell.paint(canvas);

    expectRect("AppShell sidebar frame", sidebar->frame(), oneui::Rect{10.0f, 10.0f, 200.0f, 680.0f});
    expectRect("AppShell header frame", header->frame(), oneui::Rect{214.0f, 10.0f, 776.0f, 60.0f});
    expectRect("AppShell content frame", content->frame(), oneui::Rect{214.0f, 74.0f, 776.0f, 582.0f});
    expectRect("AppShell footer frame", footer->frame(), oneui::Rect{214.0f, 660.0f, 776.0f, 30.0f});

    shell.setSidebarVisible(false);
    shell.paint(canvas);

    expectRect("AppShell hidden sidebar keeps previous frame", sidebar->frame(), oneui::Rect{10.0f, 10.0f, 200.0f, 680.0f});
    expectRect("AppShell header expands with hidden sidebar", header->frame(), oneui::Rect{10.0f, 10.0f, 980.0f, 60.0f});
    expectRect("AppShell content expands with hidden sidebar", content->frame(), oneui::Rect{10.0f, 74.0f, 980.0f, 582.0f});
    expectRect("AppShell footer expands with hidden sidebar", footer->frame(), oneui::Rect{10.0f, 660.0f, 980.0f, 30.0f});
}

void testProductShellComputesReusableRemoteClientLayout() {
    oneui::ProductShellMetrics metrics;
    metrics.sidebarWidth = 180.0f;
    metrics.headerHeight = 80.0f;
    metrics.footerHeight = 30.0f;
    metrics.contentPadding = 20.0f;
    metrics.cardGap = 12.0f;
    metrics.serviceCardHeight = 70.0f;
    metrics.actionCardHeight = 150.0f;
    metrics.minLogHeight = 120.0f;

    const auto shell = oneui::computeProductShellLayout(oneui::Size{1000.0f, 700.0f}, metrics);
    expectEqual("ProductShell sidebar visible", shell.sidebarVisible ? 1 : 0, 1);
    expectRect("ProductShell sidebar", shell.sidebar, oneui::Rect{0.0f, 0.0f, 180.0f, 700.0f});
    expectRect("ProductShell header", shell.header, oneui::Rect{180.0f, 0.0f, 820.0f, 80.0f});
    expectRect("ProductShell content", shell.content, oneui::Rect{180.0f, 80.0f, 820.0f, 590.0f});
    expectRect("ProductShell footer", shell.footer, oneui::Rect{180.0f, 670.0f, 820.0f, 30.0f});

    const auto sidebarChrome = oneui::computeProductSidebarLayout(shell.sidebar, metrics);
    expectRect("ProductSidebar avatar", sidebarChrome.avatar, oneui::Rect{12.0f, 21.0f, 32.0f, 32.0f});
    expectRect("ProductSidebar first nav", sidebarChrome.navItems[0], oneui::Rect{8.0f, 82.0f, 164.0f, 38.0f});
    expectRect("ProductSidebar bottom settings", sidebarChrome.bottomSettings, oneui::Rect{8.0f, 638.0f, 164.0f, 38.0f});

    const auto topBar = oneui::computeProductTopBarLayout(shell.header, metrics);
    expectRect("ProductTopBar search", topBar.search, oneui::Rect{278.4f, 12.0f, 472.0f, 32.0f});
    expectRect("ProductTopBar new segment", topBar.newSegment, oneui::Rect{874.0f, 12.0f, 56.0f, 32.0f});
    expectRect("ProductTopBar notification", topBar.notification, oneui::Rect{948.0f, 12.0f, 32.0f, 32.0f});

    const auto dashboard = oneui::computeProductDashboardLayout(shell.content, metrics);
    expectEqual("ProductDashboard two columns", dashboard.twoColumns ? 1 : 0, 1);
    expectRect("ProductDashboard service", dashboard.serviceCard, oneui::Rect{200.0f, 100.0f, 780.0f, 70.0f});
    expectRect("ProductDashboard primary", dashboard.primaryCard, oneui::Rect{200.0f, 182.0f, 384.0f, 150.0f});
    expectRect("ProductDashboard secondary", dashboard.secondaryCard, oneui::Rect{596.0f, 182.0f, 384.0f, 150.0f});
    expectRect("ProductDashboard log", dashboard.logCard, oneui::Rect{200.0f, 344.0f, 780.0f, 306.0f});

    const auto compact = oneui::computeProductShellLayout(oneui::Size{700.0f, 500.0f}, metrics);
    expectEqual("ProductShell compact hides sidebar", compact.sidebarVisible ? 1 : 0, 0);
    expectRect("ProductShell compact header", compact.header, oneui::Rect{0.0f, 0.0f, 700.0f, 80.0f});

    const auto row = oneui::computeProductFormRowLayout(dashboard.primaryCard, 220.0f, 96.0f, metrics);
    expectRect("ProductFormRow label", row.label, oneui::Rect{216.0f, 222.0f, 108.0f, 26.0f});
    expectRect("ProductFormRow control", row.control, oneui::Rect{324.0f, 220.0f, 136.0f, 26.0f});
    expectRect("ProductFormRow trailing", row.trailing, oneui::Rect{472.0f, 220.0f, 96.0f, 26.0f});

    const auto assist = oneui::computeProductAssistHomeLayout(shell.content, metrics);
    expectRect("ProductAssist local title", assist.localTitle, oneui::Rect{270.0f, 98.0f, 640.0f, 28.0f});
    expectRect("ProductAssist local device pill", assist.localDevicePill, oneui::Rect{490.0f, 97.0f, 104.0f, 28.0f});
    expectRect("ProductAssist local switch", assist.localSwitch, oneui::Rect{432.0f, 100.0f, 48.0f, 24.0f});
    expectRect("ProductAssist local code", assist.localCode, oneui::Rect{270.0f, 152.0f, 640.0f, 56.0f});
    expectRect("ProductAssist permanent code", assist.permanentCode, oneui::Rect{270.0f, 212.0f, 640.0f, 28.0f});
    expectRect("ProductAssist local credential input", assist.localCredentialInput, oneui::Rect{270.0f, 262.0f, 438.0f, 42.0f});
    expectRect("ProductAssist generate button", assist.generateButton, oneui::Rect{720.0f, 262.0f, 190.0f, 42.0f});
    expectRect("ProductAssist remote device", assist.remoteDeviceInput, oneui::Rect{270.0f, 380.0f, 254.04f, 42.0f});
    expectRect("ProductAssist remote code", assist.remoteCodeInput, oneui::Rect{536.04f, 380.0f, 171.96f, 42.0f});
    expectRect("ProductAssist connect button", assist.remoteConnectButton, oneui::Rect{720.0f, 380.0f, 190.0f, 42.0f});
    expectEqual("ProductAssist recent card count", assist.recentCardCount, 6);
    expectRect("ProductAssist first recent card", assist.recentCards[0], oneui::Rect{270.0f, 508.0f, 190.0f, 88.0f});
    expectRect("ProductAssist third recent card", assist.recentCards[2], oneui::Rect{674.0f, 508.0f, 190.0f, 88.0f});

    const auto status = oneui::computeProductStatusStripLayout(oneui::Rect{270.0f, 612.0f, 640.0f, 54.0f});
    expectRect("ProductStatus icon", status.icon, oneui::Rect{286.0f, 628.0f, 16.0f, 16.0f});
    expectRect("ProductStatus copy button", status.copyButton, oneui::Rect{792.0f, 623.0f, 48.0f, 26.0f});
    expectRect("ProductStatus details button", status.detailsButton, oneui::Rect{848.0f, 623.0f, 48.0f, 26.0f});
    expectRect("ProductStatus collapsed details", status.details, oneui::Rect{312.0f, 666.0f, 584.0f, 0.0f});

    const auto expandedStatus = oneui::computeProductStatusStripLayout(oneui::Rect{270.0f, 612.0f, 640.0f, 132.0f}, true);
    expectRect("ProductStatus expanded details", expandedStatus.details, oneui::Rect{312.0f, 670.0f, 584.0f, 64.0f});

    const auto chrome = oneui::computeProductWindowChromeLayout(oneui::Size{1000.0f, 700.0f}, metrics);
    expectRect("ProductChrome frame", chrome.frame, oneui::Rect{0.0f, 0.0f, 1000.0f, 700.0f});
    expectRect("ProductChrome title bar", chrome.titleBar, oneui::Rect{1.0f, 1.0f, 998.0f, 34.0f});
    expectRect("ProductChrome minimize", chrome.minimizeButton, oneui::Rect{861.0f, 1.0f, 46.0f, 34.0f});
    expectRect("ProductChrome maximize", chrome.maximizeButton, oneui::Rect{907.0f, 1.0f, 46.0f, 34.0f});
    expectRect("ProductChrome close", chrome.closeButton, oneui::Rect{953.0f, 1.0f, 46.0f, 34.0f});
    expectRect("ProductChrome content", chrome.content, oneui::Rect{1.0f, 35.0f, 998.0f, 664.0f});
}

void testMaterial3TokensResolveStateLayersAndElevation() {
    const oneui::Color base{29, 27, 32};
    const oneui::Color state{230, 225, 229};
    const auto hover = oneui::material3StateLayer(base, state, oneui::MaterialState::Hovered);
    const auto focus = oneui::material3StateLayer(base, state, oneui::MaterialState::Focused);
    const auto pressed = oneui::material3StateLayer(base, state, oneui::MaterialState::Pressed);

    expectEqual("Material3 hover red", hover.r, 45);
    expectEqual("Material3 hover green", hover.g, 43);
    expectEqual("Material3 hover blue", hover.b, 48);
    expectEqual("Material3 focus red", focus.r, 49);
    expectEqual("Material3 pressed equals focus", pressed.r, focus.r);

    const auto level0 = oneui::material3ElevationShadow(oneui::MaterialElevationLevel::Level0);
    const auto level2 = oneui::material3ElevationShadow(oneui::MaterialElevationLevel::Level2);
    expectNear("Material3 level0 blur", level0.blurRadius, 0.0f);
    expectNear("Material3 level2 blur", level2.blurRadius, 6.0f);
    expectNear("Material3 level2 y", level2.offset.y, 2.0f);
}

void testAnimationTransitionsInterpolateAndComplete() {
    expectNear("Animation clamp lower", static_cast<float>(oneui::clampUnit(-0.5)), 0.0f);
    expectNear("Animation clamp upper", static_cast<float>(oneui::clampUnit(1.5)), 1.0f);
    expectNear("Animation linear easing", static_cast<float>(oneui::applyEasing(oneui::EasingCurve::Linear, 0.25)), 0.25f);

    oneui::FloatTransition alpha(0.0f);
    alpha.animateTo(1.0f, 1000.0, oneui::TransitionSpec{100.0, oneui::EasingCurve::Linear});
    expectEqual("FloatTransition starts running", alpha.running() ? 1 : 0, 1);
    expectEqual("FloatTransition mid tick handled", alpha.tick(1050.0) ? 1 : 0, 1);
    expectNear("FloatTransition mid value", alpha.value(), 0.5f);
    expectEqual("FloatTransition complete tick handled", alpha.tick(1100.0) ? 1 : 0, 1);
    expectNear("FloatTransition final value", alpha.value(), 1.0f);
    expectEqual("FloatTransition stops at target", alpha.running() ? 1 : 0, 0);

    oneui::ColorTransition color(oneui::Color{0, 10, 20, 30});
    color.animateTo(oneui::Color{100, 110, 120, 130}, 0.0, oneui::TransitionSpec{100.0, oneui::EasingCurve::Linear});
    color.tick(50.0);
    expectEqual("ColorTransition red halfway", color.value().r, 50);
    expectEqual("ColorTransition alpha halfway", color.value().a, 80);
    color.tick(100.0);
    expectEqual("ColorTransition final blue", color.value().b, 120);
    expectEqual("ColorTransition complete", color.running() ? 1 : 0, 0);
}

void testStyleSheetResolvesCssLikeSelectorsAndStates() {
    oneui::StyleSheet sheet;
    expectEqual("StyleSheet initial version", static_cast<int>(sheet.version()), 0);

    oneui::StyleRule base;
    base.selector = ".button";
    base.box.background.color = oneui::Color{32, 34, 42};
    base.box.borderColor = oneui::Color{54, 58, 70};
    base.box.borderWidth = 1.0f;
    base.box.radius = 6.0f;
    sheet.addRule(base);
    expectEqual("StyleSheet addRule bumps version", static_cast<int>(sheet.version()), 1);

    oneui::StyleRule primary;
    primary.selector = ".button.primary";
    primary.box.background.color = oneui::Color{39, 94, 225};
    primary.box.foreground = oneui::Color{255, 255, 255};
    sheet.addRule(primary);

    oneui::StyleRule hover;
    hover.selector = ".button.primary:hover";
    hover.box.background.color = oneui::Color{52, 113, 246};
    hover.box.shadows.push_back(oneui::StyleShadow{oneui::Color{0, 0, 0, 90}, oneui::Point{0.0f, 8.0f}, 18.0f, 0.0f, false});
    sheet.addRule(hover);

    const oneui::StyleBox resolved = sheet.resolve(
        oneui::StyleNode{"button", {"button", "primary"}, oneui::StyleStateHover});

    expectEqual("StyleSheet hover keeps base radius", static_cast<int>(resolved.radius.value_or(0.0f)), 6);
    expectEqual("StyleSheet hover color wins", resolved.background.color->r, 52);
    expectEqual("StyleSheet class foreground wins", resolved.foreground->b, 255);
    expectEqual("StyleSheet hover shadow applied", static_cast<int>(resolved.shadows.size()), 1);

    oneui::StyleRule lateHover;
    lateHover.selector = ".button.primary:hover";
    lateHover.box.background.color = oneui::Color{77, 128, 255};
    sheet.addRule(lateHover);
    expectEqual("StyleSheet later addRule bumps version", static_cast<int>(sheet.version()), 4);
    const oneui::StyleBox afterCacheInvalidation = sheet.resolve(
        oneui::StyleNode{"button", {"button", "primary"}, oneui::StyleStateHover});
    expectEqual("StyleSheet resolve cache invalidates after addRule", afterCacheInvalidation.background.color->r, 77);

    sheet.setCustomProperty("--accent", "#123456");
    expectEqual("StyleSheet custom property bumps version", static_cast<int>(sheet.version()), 5);

    expectEqual("StyleSheet selected pseudo parsing", static_cast<int>(oneui::parseStylePseudoState("selected")), static_cast<int>(oneui::StyleStateSelected));
    expectEqual("StyleSheet read-only pseudo parsing", static_cast<int>(oneui::parseStylePseudoState("read-only")), static_cast<int>(oneui::StyleStateReadOnly));
    expectEqual("StyleSheet focus-visible pseudo parsing", static_cast<int>(oneui::parseStylePseudoState("focus-visible")), static_cast<int>(oneui::StyleStateFocus));
    expectEqual(
        "StyleSheet unsupported pseudo does not match normal state",
        oneui::selectorMatches(
            ".button:unsupported",
            oneui::StyleNode{"button", {"button"}, oneui::StyleStateNone})
            ? 1
            : 0,
        0);
}

void testStyleSheetParsesCssLikeRules() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool ok = sheet.addRulesFromCss(std::string("\xEF\xBB\xBF") + R"css(
        :root {
            --input-bg: #191a20;
            --input-fg: #eef2ff;
            --focus-blue: #4a79e6;
            --missing-fallback-check: #010203;
        }

        .input {
            background: var(--input-bg);
            color: var(--input-fg);
            placeholder-color: #8b92a3;
            caret-color: var(--focus-blue);
            selection-color: #2859c566;
            border-color: #2a2d38;
            border-width: 1px;
            border-radius: 8px;
            width: 240px;
            height: 42px;
            grid-min-column-width: 228px;
            font-size: 13px;
            font-weight: 600;
            detail-font-size: 11px;
            detail-font-weight: 400;
            text-inset: 9px;
            title-offset-y: 4px;
            detail-offset-y: 22px;
            scrollbar-color: #6d7188aa;
            scrollbar-width: 3px;
            outline-color: var(--unknown-outline, #1f54da44);
            outline-width: 2px;
            outline-offset: 1px;
            opacity: 0.5;
            content-background-color: #17181e;
            content-inset: 10px 12px;
            content-radius: 4px;
            transition-duration: 140ms;
            transition-timing-function: ease-out;
            box-shadow: 0px 2px 8px #00000070, inset 0px 0px 1px #ffffff18;
        }

        .input:focus {
            border-color: var(--focus-blue);
        }

        .input:read-only {
            background: #20222a;
            color: #a8afbd;
        }

        .recent-card {
            background: linear-gradient(135deg, #5386ff, #5930d2);
        }

        .transparent-surface {
            background: transparent;
        }

        .chip {
            border: 2px solid rgba(42, 45, 56, 0.75);
            outline: 3px solid rgb(31, 84, 218);
            transition: all 90ms ease-in-out;
            box-shadow: 0px 6px 18px rgba(0, 0, 0, 0.28), inset 0px 0px 1px rgba(255, 255, 255, 0.10);
        }
    )css", &error);

    expectEqual("StyleSheet CSS parse succeeds", ok ? 1 : 0, 1);
    expectEqual("StyleSheet CSS parse stores rules", static_cast<int>(sheet.rules().size()), 6);
    expectEqual("StyleSheet CSS stores root variables", static_cast<int>(sheet.customProperties().size()), 4);
    const auto inputBg = sheet.customProperty("--input-bg");
    expectEqual("StyleSheet CSS custom property lookup exists", inputBg ? 1 : 0, 1);
    if (inputBg) {
        expectEqual("StyleSheet CSS custom property lookup value", *inputBg == "#191a20" ? 1 : 0, 1);
    }

    const auto focused = sheet.resolve(oneui::StyleNode{"input", {"input"}, oneui::StyleStateFocus});
    expectEqual("StyleSheet CSS focus border wins", focused.borderColor->r, 74);
    expectEqual("StyleSheet CSS keeps base radius", static_cast<int>(focused.radius.value_or(0.0f)), 8);
    expectEqual("StyleSheet CSS parses outline width", static_cast<int>(focused.outlineWidth.value_or(0.0f)), 2);
    expectEqual("StyleSheet CSS parses outline alpha", focused.outlineColor->a, 68);
    expectEqual("StyleSheet CSS parses opacity", static_cast<int>(focused.opacity.value_or(0.0f) * 100.0f), 50);
    expectEqual("StyleSheet CSS parses font size", static_cast<int>(focused.fontSize.value_or(0.0f)), 13);
    expectEqual("StyleSheet CSS parses font weight", focused.fontWeight.value_or(0), 600);
    expectEqual("StyleSheet CSS parses detail font size", static_cast<int>(focused.detailFontSize.value_or(0.0f)), 11);
    expectEqual("StyleSheet CSS parses detail font weight", focused.detailFontWeight.value_or(0), 400);
    expectEqual("StyleSheet CSS parses text inset", static_cast<int>(focused.textInset.value_or(0.0f)), 9);
    expectEqual("StyleSheet CSS parses title offset", static_cast<int>(focused.titleOffsetY.value_or(0.0f)), 4);
    expectEqual("StyleSheet CSS parses detail offset", static_cast<int>(focused.detailOffsetY.value_or(0.0f)), 22);
    expectEqual("StyleSheet CSS parses scrollbar alpha", focused.scrollbarColor->a, 170);
    expectEqual("StyleSheet CSS parses scrollbar width", static_cast<int>(focused.scrollbarWidth.value_or(0.0f)), 3);
    expectEqual("StyleSheet CSS parses width", static_cast<int>(focused.width.value_or(0.0f)), 240);
    expectEqual("StyleSheet CSS parses height", static_cast<int>(focused.height.value_or(0.0f)), 42);
    expectEqual("StyleSheet CSS parses grid minimum column width", static_cast<int>(focused.gridMinColumnWidth.value_or(0.0f)), 228);
    expectEqual("StyleSheet CSS parses placeholder color", focused.placeholderColor->r, 139);
    expectEqual("StyleSheet CSS parses caret color", focused.caretColor->b, 230);
    expectEqual("StyleSheet CSS parses selection color alpha", focused.selectionColor->a, 102);
    expectEqual("StyleSheet CSS parses content radius", static_cast<int>(focused.content.radius.value_or(0.0f)), 4);
    expectEqual("StyleSheet CSS parses content inset left", static_cast<int>(focused.content.inset->left), 12);
    expectEqual("StyleSheet CSS parses transition duration", static_cast<int>(focused.transitionDurationMs.value_or(0.0)), 140);
    expectEqual("StyleSheet CSS parses transition easing", static_cast<int>(focused.transitionEasing.value_or(oneui::EasingCurve::Linear)), static_cast<int>(oneui::EasingCurve::EaseOutCubic));
    expectEqual("StyleSheet CSS parses two shadows", static_cast<int>(focused.shadows.size()), 2);
    expectEqual("StyleSheet CSS parses alpha", focused.shadows.back().color.a, 24);

    const auto readOnly = sheet.resolve(oneui::StyleNode{"input", {"input"}, oneui::StyleStateReadOnly});
    expectEqual("StyleSheet CSS read-only background wins", readOnly.background.color->r, 32);
    expectEqual("StyleSheet CSS read-only foreground wins", readOnly.foreground->r, 168);

    const auto card = sheet.resolve(oneui::StyleNode{"div", {"recent-card"}, oneui::StyleStateNone});
    expectEqual("StyleSheet CSS gradient start blue", card.background.gradientStart->b, 255);
    expectEqual("StyleSheet CSS gradient end red", card.background.gradientEnd->r, 89);
    expectEqual("StyleSheet CSS gradient angle", static_cast<int>(card.background.gradientAngleDegrees.value_or(0.0f)), 135);

    const auto transparent = sheet.resolve(oneui::StyleNode{"div", {"transparent-surface"}, oneui::StyleStateNone});
    expectEqual("StyleSheet CSS parses transparent keyword", transparent.background.color->a, 0);

    const auto chip = sheet.resolve(oneui::StyleNode{"span", {"chip"}, oneui::StyleStateNone});
    expectEqual("StyleSheet CSS border shorthand width", static_cast<int>(chip.borderWidth.value_or(0.0f)), 2);
    expectEqual("StyleSheet CSS border shorthand rgba alpha", chip.borderColor->a, 191);
    expectEqual("StyleSheet CSS outline shorthand width", static_cast<int>(chip.outlineWidth.value_or(0.0f)), 3);
    expectEqual("StyleSheet CSS outline shorthand rgb blue", chip.outlineColor->b, 218);
    expectEqual("StyleSheet CSS transition shorthand duration", static_cast<int>(chip.transitionDurationMs.value_or(0.0)), 90);
    expectEqual("StyleSheet CSS transition shorthand easing", static_cast<int>(chip.transitionEasing.value_or(oneui::EasingCurve::Linear)), static_cast<int>(oneui::EasingCurve::EaseInOutCubic));
    expectEqual("StyleSheet CSS box-shadow rgba count", static_cast<int>(chip.shadows.size()), 2);
    expectEqual("StyleSheet CSS box-shadow rgba alpha", chip.shadows.front().color.a, 71);
    expectEqual("StyleSheet CSS inset rgba alpha", chip.shadows.back().color.a, 26);

    oneui::StyleSheet invalidSheet;
    std::string invalidError;
    const bool invalidOk = invalidSheet.addRulesFromCss(".bad { color: var(--missing-token); }", &invalidError);
    expectEqual("StyleSheet CSS unknown variable without fallback fails", invalidOk ? 1 : 0, 0);
    expectEqual("StyleSheet CSS unknown variable reports token", invalidError.find("--missing-token") != std::string::npos ? 1 : 0, 1);
}

void testStyleAdapterBuildsButtonAndTextFieldOverrides() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool ok = sheet.addRulesFromCss(R"css(
        .button {
            background: #20222a;
            color: #ffffff;
            border-color: #363a46;
            border-width: 1px;
            border-radius: 6px;
            outline-color: #1f54da44;
            outline-width: 2px;
            outline-offset: 1px;
            transition-duration: 90ms;
            transition-timing-function: ease-in-out;
            box-shadow: 0px 4px 14px #00000050;
        }

        .button:hover {
            background: #3471f6;
        }

        .button:active {
            background: #2459d9;
        }

        .button:disabled {
            color: #7b8190;
        }

        .input {
            background: #191a20;
            color: #e9edf7;
            placeholder-color: #8b92a3;
            caret-color: #4a79e6;
            selection-color: #2859d960;
            border-color: #2a2d38;
            border-radius: 8px;
            padding: 0px 14px;
            outline-color: #1f54da44;
            outline-width: 2px;
            outline-offset: 1px;
            transition-duration: 0.16s;
            transition-timing-function: linear;
            box-shadow: 0px 2px 8px #00000050, inset 0px 0px 1px #ffffff18;
        }

        .input:focus {
            border-color: #4a79e6;
        }

        .input:read-only {
            background: #20222a;
            color: #a8afbd;
        }

        .select {
            background: #22242b;
            content-background: #111318;
            color: #d7dbe5;
            border-color: #414552;
            border-width: 1px;
            border-radius: 4px;
            content-radius: 3px;
            padding: 0px 8px;
            outline-color: #1f54da44;
            outline-width: 2px;
            outline-offset: 1px;
        }

        .select:hover {
            background: #333640;
        }

        .select:selected {
            content-background: #44495a;
            color: #f2f4f8;
        }

        .select:focus {
            border-color: #4a79e6;
        }

        .popup {
            background: #14161c;
            color: #eef1f7;
            border-color: #363b48;
            border-width: 2px;
            border-radius: 6px;
            padding: 4px 8px;
            gap: 7px;
            box-shadow: 0px 20px 30px #00000080;
        }

        .interactive-surface {
            background: #1f2130;
            border-color: #3a3e50;
            border-width: 1px;
            border-radius: 8px;
            transition-duration: 120ms;
            transition-timing-function: ease-out;
        }

        .interactive-surface:hover {
            background: #292d40;
        }

        .interactive-surface:active {
            background: #191b28;
        }

        .interactive-surface:disabled {
            border-color: #252836;
        }

        .interactive-surface:focus {
            border-color: #4a79e6;
            border-width: 2px;
        }

        .card {
            background: #181a20;
            border-color: #303440;
            border-radius: 10px;
            box-shadow: 0px 10px 24px #0000005a, inset 0px 0px 1px #ffffff18;
        }

        .window {
            background: #101116;
        }

        .sidebar {
            background: #1d1e24;
        }

        .nav-item:selected {
            background: #35323f;
            color: #ff2d74;
        }

        .legacy-nav.selected {
            background: #3a3444;
            color: #f4f4f8;
        }
    )css", &error);

    expectEqual("StyleAdapter CSS parse succeeds", ok ? 1 : 0, 1);

    const auto button = oneui::buttonStyleOverrideFromStyleSheet(sheet, oneui::StyleNode{"button", {"button"}, oneui::StyleStateNone});
    expectEqual("StyleAdapter button normal background", button.normal->background->r, 32);
    expectEqual("StyleAdapter button hover background", button.hovered->background->r, 52);
    expectEqual("StyleAdapter button pressed background", button.pressed->background->r, 36);
    expectEqual("StyleAdapter button disabled foreground", button.disabled->foreground->r, 123);
    expectEqual("StyleAdapter button shadows", static_cast<int>(button.normal->shadows->size()), 1);
    expectEqual("StyleAdapter button transition duration", static_cast<int>(button.normal->transition->durationMs), 90);
    expectEqual("StyleAdapter button transition easing", static_cast<int>(button.normal->transition->easing), static_cast<int>(oneui::EasingCurve::EaseInOutCubic));
    expectEqual("StyleAdapter button focus ring width", static_cast<int>(button.focusVisible->focusRing->width.value_or(0.0f)), 2);
    expectEqual("StyleAdapter button focus ring alpha", button.focusVisible->focusRing->color->a, 68);

    const auto textField = oneui::textFieldStyleOverrideFromStyleSheet(sheet, oneui::StyleNode{"input", {"input"}, oneui::StyleStateNone});
    expectEqual("StyleAdapter input normal background", textField.normal->background->r, 25);
    expectEqual("StyleAdapter input normal foreground", textField.normal->foreground->r, 233);
    expectEqual("StyleAdapter input placeholder", textField.normal->placeholderForeground->r, 139);
    expectEqual("StyleAdapter input caret", textField.normal->caretColor->b, 230);
    expectEqual("StyleAdapter input selection", textField.normal->selectionBackground->a, 96);
    expectEqual("StyleAdapter input padding left", static_cast<int>(textField.normal->padding->left), 14);
    expectEqual("StyleAdapter input shadows", static_cast<int>(textField.normal->shadows->size()), 2);
    expectEqual("StyleAdapter input transition duration", static_cast<int>(textField.normal->transition->durationMs), 160);
    expectEqual("StyleAdapter input transition easing", static_cast<int>(textField.normal->transition->easing), static_cast<int>(oneui::EasingCurve::Linear));
    expectEqual("StyleAdapter input focus border", textField.focusVisible->border->r, 74);
    expectEqual("StyleAdapter input read-only background", textField.readOnly->background->r, 32);
    expectEqual("StyleAdapter input read-only foreground", textField.readOnly->foreground->r, 168);

    const auto select = oneui::selectStyleOverrideFromStyleSheet(
        sheet,
        oneui::StyleNode{"select", {"select"}, oneui::StyleStateNone});
    expectEqual("StyleAdapter select normal background", select.normal->background->r, 34);
    expectEqual("StyleAdapter select popup background", select.normal->popupBackground->r, 17);
    expectEqual("StyleAdapter select option foreground", select.normal->optionForeground->r, 215);
    expectEqual("StyleAdapter select hover option background", select.hovered->optionBackground->r, 51);
    expectEqual(
        "StyleAdapter select selected option background",
        select.selected->selectedOptionBackground->r,
        68);
    expectEqual("StyleAdapter select focus border", select.focusVisible->border->r, 74);

    const auto popup = oneui::popupStyleOverrideFromStyleSheet(
        sheet,
        oneui::StyleNode{"popup", {"popup"}, oneui::StyleStateNone});
    expectEqual("StyleAdapter popup background", popup.background->r, 20);
    expectEqual("StyleAdapter popup foreground", popup.foreground->r, 238);
    expectEqual("StyleAdapter popup border", popup.border->r, 54);
    expectEqual("StyleAdapter popup border width", static_cast<int>(*popup.borderWidth), 2);
    expectEqual("StyleAdapter popup radius", static_cast<int>(*popup.radius), 6);
    expectEqual("StyleAdapter popup padding left", static_cast<int>(popup.padding->left), 8);
    expectEqual("StyleAdapter popup offset", static_cast<int>(*popup.offset), 7);
    expectEqual("StyleAdapter popup elevation", static_cast<int>(*popup.elevation), 3);

    const auto interactive = oneui::interactiveSurfaceStyleFromStyleSheet(
        sheet,
        oneui::StyleNode{"interactive-surface", {"interactive-surface"}, oneui::StyleStateNone});
    expectEqual("StyleAdapter interactive normal background", interactive.normal.background.r, 31);
    expectEqual("StyleAdapter interactive hover background", interactive.hovered.background.r, 41);
    expectEqual("StyleAdapter interactive pressed background", interactive.pressed.background.r, 25);
    expectEqual("StyleAdapter interactive disabled border", interactive.disabled.border.r, 37);
    expectEqual("StyleAdapter interactive focus border", interactive.focusVisible.border.r, 74);
    expectEqual("StyleAdapter interactive focus width", static_cast<int>(interactive.focusVisible.borderWidth), 2);
    expectEqual("StyleAdapter interactive transition duration", static_cast<int>(interactive.transition.durationMs), 120);
    expectEqual("StyleAdapter interactive transition easing", static_cast<int>(interactive.transition.easing), static_cast<int>(oneui::EasingCurve::EaseOutCubic));

    const auto card = oneui::cardStyleBoxFromStyleSheet(sheet, oneui::StyleNode{"section", {"card"}, oneui::StyleStateNone});
    expectEqual("StyleAdapter card background", card.background.color->r, 24);
    expectEqual("StyleAdapter card border", card.borderColor->r, 48);
    expectEqual("StyleAdapter card shadows", static_cast<int>(card.shadows.size()), 2);

    const auto shell = oneui::productShellStyleFromStyleSheet(sheet);
    expectEqual("StyleAdapter shell window background", shell.window.background.color->r, 16);
    expectEqual("StyleAdapter shell sidebar background", shell.sidebar.background.color->r, 29);
    expectEqual("StyleAdapter shell selected nav background", shell.selectedNavItem.background.color->r, 53);
    expectEqual("StyleAdapter shell selected nav foreground", shell.selectedNavItem.foreground->r, 255);

    const auto legacyNav = oneui::buttonStyleOverrideFromStyleSheet(sheet, oneui::StyleNode{"button", {"legacy-nav"}, oneui::StyleStateNone});
    expectEqual("StyleAdapter selected also resolves selected class background", legacyNav.selected->background->r, 58);
    expectEqual("StyleAdapter selected also resolves selected class foreground", legacyNav.selected->foreground->r, 244);

    const auto hoveredButtonBox = oneui::buttonStyleBoxFromStyleSheet(sheet, oneui::StyleNode{"button", {"button"}, oneui::StyleStateNone}, oneui::StyleStateHover);
    expectEqual("StyleAdapter button style box helper resolves hover", hoveredButtonBox.background.color->r, 52);

    const auto focusedInputBox = oneui::textFieldStyleBoxFromStyleSheet(sheet, oneui::StyleNode{"input", {"input"}, oneui::StyleStateNone}, oneui::StyleStateFocus);
    expectEqual("StyleAdapter input style box helper resolves focus", focusedInputBox.borderColor->r, 74);
}

void testTextInputBridgeComputesHostEditorGeometryAndStates() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool ok = sheet.addRulesFromCss(R"css(
        .input {
            background: #101116;
            border-color: #2a2d38;
            content-inset: 4px 12px;
            border-radius: 8px;
        }

        .input:focus {
            border-color: #4a79e6;
        }

        .input:read-only {
            background: #20222a;
        }
    )css", &error);
    expectEqual("TextInputBridge CSS parse succeeds", ok ? 1 : 0, 1);

    oneui::TextInputBridgeConfig config;
    config.focused = true;
    config.revealNativeEditor = true;
    config.leadingReservedWidth = 20.0f;
    const auto layout = oneui::computeTextInputBridgeLayout(sheet, oneui::Rect{10.0f, 20.0f, 240.0f, 42.0f}, config);
    expectEqual("TextInputBridge focused border", layout.frameStyle.borderColor->r, 74);
    expectNear("TextInputBridge content reserves leading width", layout.contentRect.x, 42.0f);
    expectNear("TextInputBridge editor max height", layout.editorRect.height, 20.0f);
    expectEqual("TextInputBridge focused shows native editor", layout.showNativeEditor ? 1 : 0, 1);

    config.readOnly = true;
    const auto readOnlyLayout = oneui::computeTextInputBridgeLayout(sheet, oneui::Rect{10.0f, 20.0f, 240.0f, 42.0f}, config);
    expectEqual("TextInputBridge read-only background", readOnlyLayout.frameStyle.background.color->r, 32);
    expectEqual("TextInputBridge read-only hides native editor", readOnlyLayout.showNativeEditor ? 1 : 0, 0);
}

void testTitleBarBridgeComputesChromeGeometryAndStates() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool ok = sheet.addRulesFromCss(R"css(
        .titlebar {
            background: #121117;
            border-color: #24212a;
        }

        .logo {
            background: #2b2930;
            border-radius: 10px;
        }

        .chrome-button {
            background: #00000000;
            border-radius: 5px;
        }

        .chrome-button:hover {
            background: #28252e;
        }

        .chrome-button.close:hover {
            background: #e5485b;
        }
    )css", &error);
    expectEqual("TitleBarBridge CSS parse succeeds", ok ? 1 : 0, 1);

    oneui::TitleBarBridgeConfig config;
    config.chrome = oneui::computeProductWindowChromeLayout(oneui::Size{980.0f, 720.0f});
    config.hoveredButton = oneui::TitleBarButtonId::Close;
    config.maximized = true;

    const auto layout = oneui::computeTitleBarBridgeLayout(sheet, config);
    expectEqual("TitleBarBridge titlebar background", layout.titleBarStyle.background.color->r, 18);
    expectNear("TitleBarBridge logo x", layout.logo.x, config.chrome.caption.x + 6.0f);
    expectEqual("TitleBarBridge maximize uses restore icon", static_cast<int>(layout.buttons[1].symbol), static_cast<int>(oneui::IconSymbol::Restore));
    expectEqual("TitleBarBridge close hover background", layout.buttons[2].style.background.color->r, 229);
    expectEqual("TitleBarBridge hit close", static_cast<int>(oneui::hitTestTitleBarButton(layout, oneui::Point{layout.buttons[2].frame.x + 1.0f, layout.buttons[2].frame.y + 1.0f})), static_cast<int>(oneui::TitleBarButtonId::Close));
    expectEqual("TitleBarBridge hit empty", static_cast<int>(oneui::hitTestTitleBarButton(layout, oneui::Point{20.0f, 20.0f})), static_cast<int>(oneui::TitleBarButtonId::None));
}

void testSidebarNavBridgeComputesItemGeometryAndStates() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool ok = sheet.addRulesFromCss(R"css(
        .nav-item {
            background: #00000000;
            color: #caced6;
            border-radius: 5px;
        }

        .nav-item:hover {
            background: #2b2930;
        }

        .nav-item.selected {
            background: #38343d;
            color: #ff2f69;
        }
    )css", &error);
    expectEqual("SidebarNavBridge CSS parse succeeds", ok ? 1 : 0, 1);

    oneui::SidebarNavItemBridgeConfig config;
    config.frame = oneui::Rect{20.0f, 80.0f, 152.0f, 38.0f};
    config.icon = oneui::Rect{28.0f, 92.0f, 14.0f, 14.0f};
    config.symbol = oneui::IconSymbol::RemoteAssist;
    config.selected = true;

    const auto layout = oneui::computeSidebarNavItemBridgeLayout(sheet, config);
    expectEqual("SidebarNavBridge selected background", layout.style.background.color->r, 56);
    expectEqual("SidebarNavBridge selected foreground", layout.foreground.r, 255);
    expectNear("SidebarNavBridge label x", layout.label.x, 59.0f);
    expectEqual("SidebarNavBridge hit inside", oneui::hitTestSidebarNavItem(layout, oneui::Point{21.0f, 81.0f}) ? 1 : 0, 1);
    expectEqual("SidebarNavBridge hit outside", oneui::hitTestSidebarNavItem(layout, oneui::Point{5.0f, 5.0f}) ? 1 : 0, 0);

    config.selected = false;
    config.hovered = true;
    const auto hovered = oneui::computeSidebarNavItemBridgeLayout(sheet, config);
    expectEqual("SidebarNavBridge hover background", hovered.style.background.color->r, 43);
    expectEqual("SidebarNavBridge hover foreground", hovered.foreground.r, 202);
}

void testButtonBridgeComputesButtonGeometryAndStates() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool ok = sheet.addRulesFromCss(R"css(
        .button {
            background: #2b2930;
            color: #ffffff;
            border-color: #49454f;
            content-inset: 2px 10px;
            border-radius: 6px;
        }

        .button.primary {
            background: #2f66d8;
        }

        .button.primary:hover {
            background: #4478ec;
        }

        .button:disabled {
            background: #211f26;
            color: #9297a4;
        }
    )css", &error);
    expectEqual("ButtonBridge CSS parse succeeds", ok ? 1 : 0, 1);

    oneui::ButtonBridgeConfig config;
    config.frame = oneui::Rect{10.0f, 20.0f, 190.0f, 42.0f};
    config.node = oneui::StyleNode{"button", {"button", "primary"}, oneui::StyleStateNone};
    config.hovered = true;
    const auto layout = oneui::computeButtonBridgeLayout(sheet, config);
    expectEqual("ButtonBridge hover primary background", layout.style.background.color->r, 68);
    expectEqual("ButtonBridge foreground from CSS", layout.foreground.r, 255);
    expectNear("ButtonBridge content inset x", layout.content.x, 20.0f);
    expectEqual("ButtonBridge hit inside", oneui::hitTestButtonBridge(layout, oneui::Point{11.0f, 21.0f}) ? 1 : 0, 1);

    config.hovered = false;
    config.disabled = true;
    const auto disabled = oneui::computeButtonBridgeLayout(sheet, config);
    expectEqual("ButtonBridge disabled background", disabled.style.background.color->r, 33);
    expectEqual("ButtonBridge disabled foreground", disabled.foreground.r, 146);
}

void testStyleBoxPainterDrawsShadowFillBorderAndInset() {
    RecordingCanvas canvas;
    oneui::StyleBox box;
    box.background.color = oneui::Color{18, 18, 23};
    box.borderColor = oneui::Color{42, 45, 56};
    box.borderWidth = 1.0f;
    box.outlineColor = oneui::Color{31, 84, 218, 70};
    box.outlineWidth = 2.0f;
    box.outlineOffset = 1.0f;
    box.content.backgroundColor = oneui::Color{23, 24, 30};
    box.opacity = 0.5f;
    box.content.inset = oneui::Insets{8.0f, 10.0f};
    box.content.radius = 4.0f;
    box.radius = 8.0f;
    box.shadows.push_back(oneui::StyleShadow{oneui::Color{0, 0, 0, 120}, oneui::Point{0.0f, 8.0f}, 20.0f, 0.0f, false});
    box.shadows.push_back(oneui::StyleShadow{oneui::Color{255, 255, 255, 18}, oneui::Point{}, 1.0f, 0.0f, true});

    oneui::paintStyleBox(canvas, oneui::Rect{10.0f, 12.0f, 100.0f, 40.0f}, box);

    expectEqual("StyleBox painter emits outer shadow", static_cast<int>(canvas.boxShadows.size()), 1);
    expectEqual("StyleBox painter emits shell and content fills", static_cast<int>(canvas.fillRects.size()), 2);
    expectEqual("StyleBox painter emits border, inset, and outline strokes", static_cast<int>(canvas.strokeRects.size()), 3);
    expectEqual("StyleBox painter fill alpha applies opacity", canvas.fillRects.front().color.a, 127);
    expectEqual("StyleBox painter content inset x", static_cast<int>(canvas.fillRects.back().rect.x), 20);
    expectEqual("StyleBox painter outline expands", static_cast<int>(canvas.strokeRects.back().rect.x), 8);
}

void testStyleBoxTransitionInterpolatesCommonVisualProperties() {
    oneui::StyleBox from;
    from.background.color = oneui::Color{0, 0, 0, 255};
    from.foreground = oneui::Color{20, 20, 20, 255};
    from.borderColor = oneui::Color{40, 40, 40, 255};
    from.opacity = 1.0f;

    oneui::StyleBox target;
    target.background.color = oneui::Color{100, 100, 100, 255};
    target.foreground = oneui::Color{120, 120, 120, 255};
    target.borderColor = oneui::Color{140, 140, 140, 255};
    target.opacity = 0.5f;
    target.transitionDurationMs = 100.0;
    target.transitionEasing = oneui::EasingCurve::Linear;

    oneui::StyleBoxTransition transition;
    transition.animateTo(from, target, 0.0);
    expectEqual("StyleBoxTransition initializes", transition.initialized() ? 1 : 0, 1);
    expectEqual("StyleBoxTransition starts running", transition.running() ? 1 : 0, 1);
    expectEqual("StyleBoxTransition ticks mid frame", transition.tick(50.0) ? 1 : 0, 1);

    const oneui::StyleBox mid = transition.applyTo(target);
    expectEqual("StyleBoxTransition interpolates background", mid.background.color->r, 50);
    expectEqual("StyleBoxTransition interpolates foreground", mid.foreground->r, 70);
    expectEqual("StyleBoxTransition interpolates border", mid.borderColor->r, 90);
    expectEqual("StyleBoxTransition interpolates opacity", static_cast<int>(mid.opacity.value_or(0.0f) * 100.0f), 75);

    transition.tick(100.0);
    const oneui::StyleBox done = transition.applyTo(target);
    expectEqual("StyleBoxTransition finishes", transition.running() ? 1 : 0, 0);
    expectEqual("StyleBoxTransition final background", done.background.color->r, 100);
    expectEqual("StyleBoxTransition final opacity", static_cast<int>(done.opacity.value_or(0.0f) * 100.0f), 50);
}

void testCardCanPaintStyleBox() {
    oneui::Card card;
    card.setFrame(oneui::Rect{10.0f, 20.0f, 120.0f, 64.0f});

    oneui::StyleBox style;
    style.background.color = oneui::Color{18, 19, 24};
    style.borderColor = oneui::Color{48, 52, 64};
    style.borderWidth = 2.0f;
    style.radius = 9.0f;
    style.shadows.push_back(oneui::StyleShadow{oneui::Color{0, 0, 0, 90}, oneui::Point{0.0f, 10.0f}, 24.0f, 0.0f, false});
    style.shadows.push_back(oneui::StyleShadow{oneui::Color{255, 255, 255, 24}, oneui::Point{}, 1.0f, 0.0f, true});
    card.setStyleBox(style);

    RecordingCanvas canvas;
    card.paint(canvas);

    expectEqual("Card style box draws one outer shadow", static_cast<int>(canvas.boxShadows.size()), 1);
    expectEqual("Card style box background", countFillRectsWithColor(canvas, oneui::Color{18, 19, 24}), 1);
    expectEqual("Card style box border", countStrokeRectsWithColor(canvas, oneui::Color{48, 52, 64}), 1);
    expectEqual("Card style box inset shadow stroke", countStrokeRectsWithColor(canvas, oneui::Color{255, 255, 255, 24}), 1);

    card.clearStyleBox();
    RecordingCanvas defaultCanvas;
    card.paint(defaultCanvas);
    expectEqual("Card clear style box restores default background", countFillRectsWithColor(defaultCanvas, oneui::Color{255, 255, 255}), 1);
}

void testToastPaintsAndDispatchesActions() {
    oneui::Toast toast(L"Notice", L"Reusable toast message");
    toast.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 88.0f});
    toast.setPrimaryAction(L"Use now");
    toast.setSecondaryAction(L"Close");
    toast.setIconSymbol(oneui::IconSymbol::Sparkle);

    oneui::StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        .toast {
            background: #303039;
            border-color: #51515d;
            border-width: 1px;
            border-radius: 8px;
            color: #ffffff;
            padding: 14px 16px 14px 16px;
            box-shadow: 0px 10px 24px 0px #00000060;
        }
        .toast-action {
            background: #101018;
            border-color: #444450;
            border-width: 1px;
            border-radius: 6px;
            color: #eeeeff;
        }
        .toast-action.primary {
            background: #2f66d8;
            border-color: #2f66d8;
        }
        .toast-action.primary:active {
            background: #244fb0;
            border-color: #244fb0;
        }
      )css", &error);
    toast.setStyleSheet(std::make_shared<oneui::StyleSheet>(sheet), oneui::StyleNode{"toast", {"toast"}, oneui::StyleStateNone});

    RecordingCanvas canvas;
    toast.paint(canvas);

    expectEqual("Toast paints configured shadow", static_cast<int>(canvas.boxShadows.size()), 1);
    expectEqual("Toast paints CSS primary action background", countFillRectsWithColor(canvas, oneui::Color{47, 102, 216}), 1);
    expectEqual("Toast paints CSS secondary action background", countFillRectsWithColor(canvas, oneui::Color{16, 16, 24}), 1);
    expectEqual("Toast paints title and message text", static_cast<int>(canvas.texts.size()) >= 2 ? 1 : 0, 1);

    int primaryClicks = 0;
    int closeClicks = 0;
    toast.setOnPrimaryAction([&] {
        ++primaryClicks;
    });
    toast.setOnSecondaryAction([&] {
        ++closeClicks;
    });

    toast.onMouseDown(oneui::MouseEvent{oneui::Point{270.0f, 44.0f}});
    RecordingCanvas activeCanvas;
    toast.paint(activeCanvas);
    expectEqual("Toast active action style comes from CSS", countFillRectsWithColor(activeCanvas, oneui::Color{36, 79, 176}), 1);
    toast.onMouseUp(oneui::MouseEvent{oneui::Point{270.0f, 44.0f}});
    expectEqual("Toast primary action click", primaryClicks, 1);
    expectEqual("Toast secondary action untouched", closeClicks, 0);

    toast.onMouseDown(oneui::MouseEvent{oneui::Point{200.0f, 44.0f}});
    toast.onMouseUp(oneui::MouseEvent{oneui::Point{200.0f, 44.0f}});
    expectEqual("Toast secondary action click", closeClicks, 1);
}

void testStatusStripActionStyleComesFromCss() {
    oneui::StatusStrip strip(L"Status", L"Reusable status text");
    strip.setFrame(oneui::Rect{0.0f, 0.0f, 360.0f, 62.0f});
    strip.setPrimaryAction(L"Copy");
    strip.setSecondaryAction(L"Details");

    oneui::StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        .status-strip {
            background: #202028;
            border-color: #4a4a56;
            border-width: 1px;
            border-radius: 7px;
            color: #f4f5fa;
            padding: 12px 18px 12px 18px;
        }
        .status-strip-action {
            background: #15151b;
            border-color: #393946;
            border-width: 1px;
            border-radius: 6px;
            color: #dde0ea;
        }
        .status-strip-action:hover {
            background: #292932;
            border-color: #535362;
        }
    )css", &error);
    strip.setStyleSheet(std::make_shared<oneui::StyleSheet>(sheet), oneui::StyleNode{"status-strip", {"status-strip"}, oneui::StyleStateNone});

    RecordingCanvas canvas;
    strip.paint(canvas);
    expectEqual("StatusStrip action background comes from CSS", countFillRectsWithColor(canvas, oneui::Color{21, 21, 27}), 2);

    strip.onMouseMove(oneui::MouseEvent{oneui::Point{260.0f, 26.0f}});
    RecordingCanvas hoverCanvas;
    strip.paint(hoverCanvas);
    expectEqual("StatusStrip hover action background comes from CSS", countFillRectsWithColor(hoverCanvas, oneui::Color{41, 41, 50}), 1);
}

void testStateViewPaintsSemanticContentAndDispatchesAction() {
    oneui::StateView state(L"No hosts yet", L"Create a host or import an existing connection.");
    state.setFrame(oneui::Rect{0.0f, 0.0f, 400.0f, 220.0f});
    state.setAction(L"Create host");

    int actionClicks = 0;
    state.setOnAction([&] {
        ++actionClicks;
    });

    oneui::StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        .content-state {
            background: #171821;
            color: #f2f3f8;
            padding: 24px;
        }
        .state-view-action {
            background: #5146d9;
            border-color: #6e64ef;
            border-width: 1px;
            border-radius: 6px;
            color: #ffffff;
        }
        .state-view-action:hover {
            background: #665be8;
        }
    )css", &error);
    state.setStyleSheet(
        std::make_shared<oneui::StyleSheet>(sheet),
        oneui::StyleNode{"state-view", {"content-state"}, oneui::StyleStateNone});

    RecordingCanvas canvas;
    state.paint(canvas);
    expectEqual("StateView paints semantic title", countTextsWithText(canvas, L"No hosts yet"), 1);
    expectEqual("StateView paints semantic message", countTextsWithText(canvas, L"Create a host or import an existing connection."), 1);
    expectEqual("StateView paints CSS action background", countFillRectsWithColor(canvas, oneui::Color{81, 70, 217}), 1);

    state.onMouseMove(oneui::MouseEvent{oneui::Point{200.0f, 178.0f}});
    RecordingCanvas hoverCanvas;
    state.paint(hoverCanvas);
    expectEqual("StateView hover action background comes from CSS", countFillRectsWithColor(hoverCanvas, oneui::Color{102, 91, 232}), 1);
    state.onMouseDown(oneui::MouseEvent{oneui::Point{200.0f, 178.0f}});
    state.onMouseUp(oneui::MouseEvent{oneui::Point{200.0f, 178.0f}});
    expectEqual("StateView action click", actionClicks, 1);
}

void testCardLaysOutContentWithPadding() {
    oneui::Card card;
    card.setFrame(oneui::Rect{10.0f, 20.0f, 200.0f, 80.0f});
    card.setPadding(oneui::Insets{6.0f, 8.0f, 10.0f, 12.0f});

    auto child = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    card.setContent(child);

    RecordingCanvas canvas;
    card.paint(canvas);

    const auto frame = child->frame();
    expectEqual("Card padded child x", static_cast<int>(frame.x), 22);
    expectEqual("Card padded child y", static_cast<int>(frame.y), 26);
    expectEqual("Card padded child width", static_cast<int>(frame.width), 180);
    expectEqual("Card padded child height", static_cast<int>(frame.height), 64);
}

void testIconPrimitivesProvideReusableNativeShapes() {
    const auto search = oneui::buildIconPrimitives(
        oneui::IconSymbol::Search,
        oneui::Rect{0.0f, 0.0f, 16.0f, 16.0f},
        oneui::Color{220, 224, 234});
    expectEqual("Icon search primitive count", static_cast<int>(search.size()), 2);
    expectEqual("Icon search circle kind", static_cast<int>(search.front().kind), static_cast<int>(oneui::IconPrimitiveKind::Circle));

    const auto brand = oneui::buildIconPrimitives(
        oneui::IconSymbol::BrandBloom,
        oneui::Rect{0.0f, 0.0f, 24.0f, 24.0f},
        oneui::Color{255, 255, 255},
        oneui::Color{255, 47, 105});
    expectEqual("Icon brand bloom has petals", static_cast<int>(brand.size()), 6);
    expectEqual("Icon brand background filled", brand.front().filled ? 1 : 0, 1);

    const auto monitor = oneui::buildIconPrimitives(
        oneui::IconSymbol::Monitor,
        oneui::Rect{0.0f, 0.0f, 24.0f, 18.0f},
        oneui::Color{209, 218, 255, 92});
    expectEqual("Icon primitive preserves alpha", monitor.front().color.a, 92);

    const auto radio = oneui::buildIconPrimitives(
        oneui::IconSymbol::RadioOn,
        oneui::Rect{0.0f, 0.0f, 16.0f, 16.0f},
        oneui::Color{120, 128, 144},
        oneui::Color{80, 145, 255});
    expectEqual("Icon radio on emits ring and dot", static_cast<int>(radio.size()), 2);
    expectEqual("Icon radio dot filled", radio.back().filled ? 1 : 0, 1);

    const auto toggle = oneui::buildIconPrimitives(
        oneui::IconSymbol::ToggleOn,
        oneui::Rect{0.0f, 0.0f, 42.0f, 22.0f},
        oneui::Color{255, 255, 255},
        oneui::Color{80, 145, 255});
    expectEqual("Icon toggle emits track and knob", static_cast<int>(toggle.size()), 2);
    expectEqual("Icon toggle track filled", toggle.front().filled ? 1 : 0, 1);

    const auto copy = oneui::buildIconPrimitives(
        oneui::IconSymbol::Copy,
        oneui::Rect{0.0f, 0.0f, 16.0f, 16.0f},
        oneui::Color{160, 168, 184});
    expectEqual("Icon copy emits two sheets", static_cast<int>(copy.size()), 2);

    const auto terminal = oneui::buildIconPrimitives(
        oneui::IconSymbol::Terminal,
        oneui::Rect{0.0f, 0.0f, 24.0f, 24.0f},
        oneui::Color{209, 218, 255});
    expectEqual("Icon terminal emits frame prompt and baseline", static_cast<int>(terminal.size()), 3);
    expectEqual("Icon terminal starts with rounded frame", static_cast<int>(terminal.front().kind), static_cast<int>(oneui::IconPrimitiveKind::RoundRect));

    const auto grid = oneui::buildIconPrimitives(
        oneui::IconSymbol::LayoutGrid,
        oneui::Rect{0.0f, 0.0f, 16.0f, 16.0f},
        oneui::Color{209, 218, 255});
    expectEqual("Icon grid emits four cells", static_cast<int>(grid.size()), 4);

    const auto edit = oneui::buildIconPrimitives(
        oneui::IconSymbol::Edit,
        oneui::Rect{0.0f, 0.0f, 16.0f, 16.0f},
        oneui::Color{209, 218, 255});
    expectEqual("Icon edit emits pencil and baseline", static_cast<int>(edit.size()), 3);

    const auto trash = oneui::buildIconPrimitives(
        oneui::IconSymbol::Trash,
        oneui::Rect{0.0f, 0.0f, 16.0f, 16.0f},
        oneui::Color{209, 218, 255});
    expectEqual("Icon trash emits bin structure", static_cast<int>(trash.size()), 5);
}

void testIconViewPaintsRegistryPrimitives() {
    oneui::IconView icon(oneui::IconSymbol::BrandBloom);
    icon.setFrame(oneui::Rect{0.0f, 0.0f, 24.0f, 24.0f});
    icon.setColor(oneui::Color{255, 255, 255});
    icon.setAccent(oneui::Color{255, 47, 105});

    RecordingCanvas canvas;
    icon.paint(canvas);

    expectEqual("IconView brand paints filled circles", static_cast<int>(canvas.fillEllipses.size()), 6);
    expectEqual("IconView accent alpha", canvas.fillEllipses.front().color.a, 255);
}

void testButtonSupportsLeadingContentAndTrailingMetadata() {
    oneui::Button button(L"default");
    button.setFrame(oneui::Rect{0.0f, 0.0f, 200.0f, 32.0f});
    button.setContentAlign(oneui::TextAlign::Left);
    button.setTrailingText(L"4");

    RecordingCanvas canvas;
    button.paint(canvas);

    expectEqual("Button trailing metadata paints both labels", static_cast<int>(canvas.texts.size()), 2);
    expectEqual("Button primary label stays left", static_cast<int>(canvas.texts[0].rect.x), 12);
    expectEqual("Button trailing metadata stays right", static_cast<int>(canvas.texts[1].rect.x), 181);
}

void testWindowTitleBarPaintsAndDispatchesChromeActions() {
    oneui::WindowTitleBar titleBar(L"Remote");
    titleBar.setFrame(oneui::Rect{0.0f, 0.0f, 980.0f, 34.0f});
    int animationRequests = 0;
    titleBar.setAnimationScheduler([&]() {
        ++animationRequests;
    });

    int minimizeCount = 0;
    int maximizeCount = 0;
    int closeCount = 0;
    titleBar.setOnMinimize([&]() { ++minimizeCount; });
    titleBar.setOnMaximize([&]() { ++maximizeCount; });
    titleBar.setOnClose([&]() { ++closeCount; });

    RecordingCanvas canvas;
    titleBar.paint(canvas);

    expectEqual("WindowTitleBar paints title text", static_cast<int>(canvas.texts.empty() ? 0 : 1), 1);
    expectEqual("WindowTitleBar paints brand mark", static_cast<int>(canvas.fillEllipses.empty() ? 0 : 1), 1);

    titleBar.onMouseMove(oneui::MouseEvent{oneui::Point{960.0f, 17.0f}});
    expectEqual("WindowTitleBar hover requests animation", animationRequests > 0 ? 1 : 0, 1);
    expectEqual("WindowTitleBar hover animation ticking", titleBar.tickAnimations(0.0) ? 1 : 0, 1);
    titleBar.onMouseDown(oneui::MouseEvent{oneui::Point{960.0f, 17.0f}});
    titleBar.onMouseUp(oneui::MouseEvent{oneui::Point{960.0f, 17.0f}});

    expectEqual("WindowTitleBar close callback", closeCount, 1);
    expectEqual("WindowTitleBar minimize untouched", minimizeCount, 0);
    expectEqual("WindowTitleBar maximize untouched", maximizeCount, 0);
}

void testNavItemPaintsSelectionAndDispatchesClick() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool ok = sheet.addRulesFromCss(R"css(
        .nav-item {
            background: #1f1f24;
            border-color: #1f1f24;
            border-width: 1px;
            border-radius: 5px;
            color: #dadbe1;
        }
        .nav-item.selected {
            background: #3b3843;
            border-color: #484451;
            color: #ff3870;
        }
    )css", &error);
    expectEqual("NavItem CSS parse succeeds", ok ? 1 : 0, 1);

    oneui::NavItem item(L"Remote Assist", oneui::IconSymbol::RemoteAssist);
    item.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 38.0f});
    item.setSelected(true);
    item.setStyleSheet(std::make_shared<oneui::StyleSheet>(sheet));
    int clickCount = 0;
    item.setOnClick([&]() { ++clickCount; });

    RecordingCanvas canvas;
    item.paint(canvas);

    expectEqual("NavItem paints label", static_cast<int>(canvas.texts.empty() ? 0 : 1), 1);
    if (!canvas.texts.empty()) {
        expectEqual("NavItem selected text red", canvas.texts.front().color.r, 255);
    }

    item.onMouseDown(oneui::MouseEvent{oneui::Point{12.0f, 12.0f}});
    item.onMouseUp(oneui::MouseEvent{oneui::Point{12.0f, 12.0f}});
    expectEqual("NavItem click callback", clickCount, 1);
}

void testNavItemHoverKeepsSemanticForegroundFallback() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool ok = sheet.addRulesFromCss(R"css(
        .nav-item {
            background: #1f1f24;
            border-color: #1f1f24;
            transition-duration: 120ms;
        }
        .nav-item:hover {
            background: #2b2b32;
            border-color: #41414a;
        }
    )css", &error);
    expectEqual("NavItem fallback hover CSS parse succeeds", ok ? 1 : 0, 1);

    oneui::NavItem item(L"Device", oneui::IconSymbol::Device);
    item.setFrame(oneui::Rect{0.0f, 0.0f, 160.0f, 38.0f});
    item.setStyleSheet(std::make_shared<oneui::StyleSheet>(sheet));
    item.onMouseMove(oneui::MouseEvent{oneui::Point{20.0f, 18.0f}});

    RecordingCanvas canvas;
    item.paint(canvas);

    expectEqual("NavItem hover fallback text visible", countTextsWithText(canvas, L"Device"), 1);
    if (!canvas.texts.empty()) {
        expectEqual("NavItem hover fallback text alpha", canvas.texts.front().color.a > 0 ? 1 : 0, 1);
    }
}

void testSwitchStyleSheetOverridePaintsCheckedState() {
    oneui::StyleSheet sheet;
    std::string error;
    const bool ok = sheet.addRulesFromCss(R"css(
        .local-switch {
            background: #41424a;
            content-background: #f2f4f8;
            border-color: #4a4b54;
            border-width: 1px;
            border-radius: 12px;
        }
        .local-switch:checked {
            background: #3d82ff;
            content-background: #ffffff;
            border-color: #5b98ff;
        }
    )css", &error);
    expectEqual("Switch CSS parse succeeds", ok ? 1 : 0, 1);

    oneui::Switch control;
    control.setFrame(oneui::Rect{0.0f, 0.0f, 54.0f, 28.0f});
    control.setChecked(true);
    control.setStyleOverride(oneui::switchStyleOverrideFromStyleSheet(sheet, oneui::StyleNode{"switch", {"local-switch"}, oneui::StyleStateNone}));

    RecordingCanvas canvas;
    control.paint(canvas);

    expectEqual("Switch checked track fill from CSS", canvas.fillRects.front().color.b, 255);
    expectEqual("Switch checked thumb fill from CSS", canvas.fillRects.back().color.r, 255);
    expectEqual("Switch checked border from CSS", canvas.strokeRects.front().color.g, 152);
}

void testTextFieldAffixIconsPaintAndOffsetText() {
    oneui::TextField field(L"Search");
    field.setText(L"http://127.0.0.1:8080");
    field.setPrefixIcon(oneui::IconSymbol::Search);
    field.setSuffixIcon(oneui::IconSymbol::ChevronDown);
    field.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 36.0f});

    RecordingCanvas canvas;
    field.paint(canvas);

    expectEqual("TextField affix icon uses canvas lines", static_cast<int>(canvas.lines.empty() ? 0 : 1), 1);
    expectEqual("TextField affix icon uses circle primitive", static_cast<int>(canvas.strokeEllipses.empty() ? 0 : 1), 1);
    if (!canvas.texts.empty()) {
        expectEqual("TextField affix offsets text", canvas.texts.front().rect.x > 24.0f ? 1 : 0, 1);
    }
}

void testSidebarNavItemsRemainVisibleAfterHoverSweep() {
    auto root = std::make_shared<oneui::Panel>();
    root->setFrame(oneui::Rect{0.0f, 0.0f, 980.0f, 720.0f});

    auto sidebar = std::make_shared<oneui::Panel>();
    sidebar->setPreferredSize(oneui::Size{184.0f, 0.0f});
    auto sidebarStack = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
    sidebarStack->setGap(10.0f);
    sidebarStack->setAlign(oneui::StackAlign::Stretch);
    sidebar->setContent(sidebarStack);

    auto brand = std::make_shared<oneui::Label>(L"Brand");
    brand->setPreferredSize(oneui::Size{0.0f, 54.0f});
    sidebarStack->add(brand);

    std::vector<std::shared_ptr<oneui::NavItem>> navItems;
    for (const auto& item : {
             std::make_pair(L"远程协助", true),
             std::make_pair(L"设备", false),
             std::make_pair(L"工具包", false),
             std::make_pair(L"贝锐", false)}) {
        auto nav = std::make_shared<oneui::NavItem>(item.first, oneui::IconSymbol::RemoteAssist);
        nav->setSelected(item.second);
        sidebarStack->add(nav);
        navItems.push_back(nav);
    }

    auto divider = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 1.0f});
    sidebarStack->add(divider);
    auto spacer = std::make_shared<oneui::Label>(L"");
    spacer->setPreferredSize(oneui::Size{0.0f, 0.0f});
    sidebarStack->add(spacer);
    auto settings = std::make_shared<oneui::NavItem>(L"设置", oneui::IconSymbol::Settings);
    sidebarStack->add(settings);

    auto shell = std::make_shared<oneui::AppShell>();
    shell->setFrame(oneui::Rect{0.0f, 0.0f, 980.0f, 720.0f});
    shell->setSidebarWidth(184.0f);
    shell->setSidebar(sidebar);
    shell->setContent(root);

    oneui::View view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 980.0f, 720.0f});
    int scheduledFrames = 0;
    view.setAnimationScheduler([&] {
        ++scheduledFrames;
    });
    view.add(shell);

    RecordingCanvas initialCanvas;
    view.paint(initialCanvas);
    expectEqual("Sidebar initial nav 1 text visible", countTextsWithText(initialCanvas, L"远程协助"), 1);
    expectEqual("Sidebar initial nav 2 text visible", countTextsWithText(initialCanvas, L"设备"), 1);
    expectEqual("Sidebar initial nav 3 text visible", countTextsWithText(initialCanvas, L"工具包"), 1);
    expectEqual("Sidebar initial nav 4 text visible", countTextsWithText(initialCanvas, L"贝锐"), 1);
    expectNear("Sidebar initial nav 1 height", navItems[0]->frame().height, 38.0f);
    expectNear("Sidebar initial nav 2 y", navItems[1]->frame().y, navItems[0]->frame().y + 48.0f);
    expectNear("Sidebar initial nav 4 y", navItems[3]->frame().y, navItems[2]->frame().y + 48.0f);

    for (const auto& point : {
             oneui::Point{60.0f, 130.0f},
             oneui::Point{60.0f, 178.0f},
             oneui::Point{60.0f, 226.0f},
             oneui::Point{60.0f, 274.0f},
             oneui::Point{60.0f, 660.0f} }) {
        view.onMouseMove(oneui::MouseEvent{point});
    }

    RecordingCanvas hoverCanvas;
    view.paint(hoverCanvas);
    expectEqual("Sidebar hover sweep keeps nav 1 text visible", countTextsWithText(hoverCanvas, L"远程协助"), 1);
    expectEqual("Sidebar hover sweep keeps nav 2 text visible", countTextsWithText(hoverCanvas, L"设备"), 1);
    expectEqual("Sidebar hover sweep keeps nav 3 text visible", countTextsWithText(hoverCanvas, L"工具包"), 1);
    expectEqual("Sidebar hover sweep keeps nav 4 text visible", countTextsWithText(hoverCanvas, L"贝锐"), 1);
    expectNear("Sidebar hover nav 1 height", navItems[0]->frame().height, 38.0f);
    expectNear("Sidebar hover nav 2 y", navItems[1]->frame().y, navItems[0]->frame().y + 48.0f);
    expectNear("Sidebar hover nav 4 y", navItems[3]->frame().y, navItems[2]->frame().y + 48.0f);
    expectEqual("Sidebar hover schedules animation frames", scheduledFrames > 0 ? 1 : 0, 1);
}

void testSplitViewRatioGapPaddingAndHiddenChild() {
    oneui::SplitView split;
    split.setFrame(oneui::Rect{0.0f, 0.0f, 210.0f, 100.0f});
    split.setPadding(oneui::Insets{5.0f});
    split.setGap(10.0f);
    split.setSplitRatio(0.25f);

    auto first = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    auto second = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    split.setFirst(first);
    split.setSecond(second);

    RecordingCanvas canvas;
    split.paint(canvas);

    expectRect("Split horizontal first frame", first->frame(), oneui::Rect{5.0f, 5.0f, 47.5f, 90.0f});
    expectRect("Split horizontal second frame", second->frame(), oneui::Rect{62.5f, 5.0f, 142.5f, 90.0f});

    split.setOrientation(oneui::SplitOrientation::Vertical);
    split.setSplitRatio(2.0f);
    split.paint(canvas);

    expectRect("Split vertical clamped first frame", first->frame(), oneui::Rect{5.0f, 5.0f, 200.0f, 80.0f});
    expectRect("Split vertical clamped second frame", second->frame(), oneui::Rect{5.0f, 95.0f, 200.0f, 0.0f});

    first->setVisible(false);
    split.paint(canvas);

    expectRect("Split hidden first keeps previous frame", first->frame(), oneui::Rect{5.0f, 5.0f, 200.0f, 80.0f});
    expectRect("Split second fills when first hidden", second->frame(), oneui::Rect{5.0f, 5.0f, 200.0f, 90.0f});
}

void testSplitViewResizableDividerHonorsMinimumExtents() {
    oneui::SplitView split;
    split.setFrame(oneui::Rect{0.0f, 0.0f, 210.0f, 100.0f});
    split.setPadding(oneui::Insets{5.0f});
    split.setGap(10.0f);
    split.setResizable(true);
    split.setMinimumPaneExtent(60.0f, 40.0f);

    auto first = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    auto second = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 0.0f});
    split.setFirst(first);
    split.setSecond(second);

    int changeCount = 0;
    float changedRatio = 0.0f;
    split.setOnSplitRatioChanged([&](float ratio) {
        ++changeCount;
        changedRatio = ratio;
    });

    RecordingCanvas canvas;
    split.paint(canvas);
    expectEqual(
        "Split divider exposes horizontal resize cursor",
        split.cursor(oneui::Point{105.0f, 50.0f}) == oneui::CursorKind::ResizeHorizontal ? 1 : 0,
        1);
    expectEqual(
        "Split divider starts drag",
        split.onMouseDown(oneui::MouseEvent{oneui::Point{105.0f, 50.0f}, oneui::MouseButton::Left}) ? 1 : 0,
        1);
    split.onMouseMove(oneui::MouseEvent{oneui::Point{10.0f, 50.0f}, oneui::MouseButton::Left});
    split.paint(canvas);

    expectNear("Split drag clamps first minimum", first->frame().width, 60.0f);
    expectNear("Split drag preserves second extent", second->frame().width, 130.0f);
    expectEqual("Split drag reports one ratio change", changeCount, 1);
    expectNear("Split drag reports constrained ratio", changedRatio, 60.0f / 190.0f);
    expectEqual(
        "Split divider ends drag",
        split.onMouseUp(oneui::MouseEvent{oneui::Point{10.0f, 50.0f}, oneui::MouseButton::Left}) ? 1 : 0,
        1);

    split.setOrientation(oneui::SplitOrientation::Vertical);
    split.setSplitRatio(0.5f);
    split.paint(canvas);
    expectEqual(
        "Split divider exposes vertical resize cursor",
        split.cursor(oneui::Point{105.0f, 58.0f}) == oneui::CursorKind::ResizeVertical ? 1 : 0,
        1);
}

void testScrollViewWheelClampsToContentBounds() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 300.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    scroll.setContent(content);
    scroll.setWheelStep(50.0f);

    scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, -1.0f});
    expectNear("ScrollView wheel waits for a sampled frame", scroll.scrollOffset(), 0.0f);
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView wheel down offset", scroll.scrollOffset(), 50.0f);
    expectRect("ScrollView content shifted after wheel", content->frame(), oneui::Rect{0.0f, -50.0f, 106.0f, 300.0f});

    scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, -10.0f});
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView clamps bottom", scroll.scrollOffset(), 200.0f);

    scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, 10.0f});
    scroll.tickAnimations(1.0e15);
    expectNear("ScrollView clamps top", scroll.scrollOffset(), 0.0f);
}

void testScrollViewNoOverflowDoesNotScrollOrPaintThumb() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 80.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    scroll.setContent(content);

    const bool handled = scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{20.0f, 20.0f}, -1.0f});
    expectEqual("ScrollView no overflow skips wheel", handled ? 1 : 0, 0);
    expectNear("ScrollView no overflow offset", scroll.scrollOffset(), 0.0f);

    RecordingCanvas canvas;
    scroll.paint(canvas);
    expectEqual("ScrollView clips viewport", static_cast<int>(canvas.clips.size()), 1);
    expectEqual("ScrollView no overflow paints viewport fill only", static_cast<int>(canvas.fillRects.size()), 1);
}

void testScrollViewIgnoresWheelOutsideBounds() {
    auto content = std::make_shared<LayoutProbe>(oneui::Size{0.0f, 300.0f});
    oneui::ScrollView scroll;
    scroll.setFrame(oneui::Rect{0.0f, 0.0f, 120.0f, 100.0f});
    scroll.setContent(content);

    const bool handled = scroll.onMouseWheel(oneui::MouseWheelEvent{oneui::Point{140.0f, 20.0f}, -1.0f});
    expectEqual("ScrollView outside wheel skipped", handled ? 1 : 0, 0);
    expectNear("ScrollView outside wheel offset", scroll.scrollOffset(), 0.0f);
}

void testPopupPlacementBottomStartUsesAnchorAndOffset() {
    const auto result = oneui::PopupPlacement::resolve(oneui::PopupPlacementRequest{
        oneui::Rect{20.0f, 30.0f, 80.0f, 24.0f},
        oneui::Size{120.0f, 60.0f},
        oneui::Rect{0.0f, 0.0f, 300.0f, 220.0f},
        oneui::PopupPreferredPlacement::BottomStart,
        6.0f
    });

    expectRect("PopupPlacement bottom-start rect", result.rect, oneui::Rect{20.0f, 60.0f, 120.0f, 60.0f});
    expectEqual("PopupPlacement bottom-start placement", static_cast<int>(result.placement), static_cast<int>(oneui::PopupPreferredPlacement::BottomStart));
    expectEqual("PopupPlacement bottom-start not flipped", result.flipped ? 1 : 0, 0);
}

void testPopupPlacementFlipsWhenBottomOverflows() {
    const auto result = oneui::PopupPlacement::resolve(oneui::PopupPlacementRequest{
        oneui::Rect{20.0f, 170.0f, 80.0f, 24.0f},
        oneui::Size{120.0f, 60.0f},
        oneui::Rect{0.0f, 0.0f, 300.0f, 220.0f},
        oneui::PopupPreferredPlacement::BottomStart,
        6.0f
    });

    expectRect("PopupPlacement flipped top-start rect", result.rect, oneui::Rect{20.0f, 104.0f, 120.0f, 60.0f});
    expectEqual("PopupPlacement flipped top-start placement", static_cast<int>(result.placement), static_cast<int>(oneui::PopupPreferredPlacement::TopStart));
    expectEqual("PopupPlacement flipped flag", result.flipped ? 1 : 0, 1);
}

void testPopupPlacementShiftsInsideViewport() {
    const auto result = oneui::PopupPlacement::resolve(oneui::PopupPlacementRequest{
        oneui::Rect{260.0f, 30.0f, 50.0f, 24.0f},
        oneui::Size{120.0f, 60.0f},
        oneui::Rect{0.0f, 0.0f, 300.0f, 220.0f},
        oneui::PopupPreferredPlacement::BottomStart,
        6.0f
    });

    expectRect("PopupPlacement right shift rect", result.rect, oneui::Rect{180.0f, 60.0f, 120.0f, 60.0f});
}

void testPopupPlacementRightStartUsesAnchorAndOffset() {
    const auto result = oneui::PopupPlacement::resolve(oneui::PopupPlacementRequest{
        oneui::Rect{40.0f, 50.0f, 30.0f, 20.0f},
        oneui::Size{80.0f, 50.0f},
        oneui::Rect{0.0f, 0.0f, 220.0f, 160.0f},
        oneui::PopupPreferredPlacement::RightStart,
        8.0f
    });

    expectRect("PopupPlacement right-start rect", result.rect, oneui::Rect{78.0f, 50.0f, 80.0f, 50.0f});
    expectEqual("PopupPlacement right-start placement", static_cast<int>(result.placement), static_cast<int>(oneui::PopupPreferredPlacement::RightStart));
    expectEqual("PopupPlacement right-start not flipped", result.flipped ? 1 : 0, 0);
}

void testPopupPlacementRightStartFlipsWhenRightOverflows() {
    const auto result = oneui::PopupPlacement::resolve(oneui::PopupPlacementRequest{
        oneui::Rect{160.0f, 40.0f, 40.0f, 20.0f},
        oneui::Size{70.0f, 50.0f},
        oneui::Rect{0.0f, 0.0f, 220.0f, 160.0f},
        oneui::PopupPreferredPlacement::RightStart,
        8.0f
    });

    expectRect("PopupPlacement right-start flipped left-start rect", result.rect, oneui::Rect{82.0f, 40.0f, 70.0f, 50.0f});
    expectEqual("PopupPlacement right-start flipped placement", static_cast<int>(result.placement), static_cast<int>(oneui::PopupPreferredPlacement::LeftStart));
    expectEqual("PopupPlacement right-start flipped flag", result.flipped ? 1 : 0, 1);
}

void testPopupPlacementLeftStartFlipsWhenLeftOverflows() {
    const auto result = oneui::PopupPlacement::resolve(oneui::PopupPlacementRequest{
        oneui::Rect{10.0f, 40.0f, 40.0f, 20.0f},
        oneui::Size{70.0f, 50.0f},
        oneui::Rect{0.0f, 0.0f, 220.0f, 160.0f},
        oneui::PopupPreferredPlacement::LeftStart,
        8.0f
    });

    expectRect("PopupPlacement left-start flipped right-start rect", result.rect, oneui::Rect{58.0f, 40.0f, 70.0f, 50.0f});
    expectEqual("PopupPlacement left-start flipped placement", static_cast<int>(result.placement), static_cast<int>(oneui::PopupPreferredPlacement::RightStart));
    expectEqual("PopupPlacement left-start flipped flag", result.flipped ? 1 : 0, 1);
}

void testPopupPlacementClampsOversizedPopup() {
    const auto result = oneui::PopupPlacement::resolve(oneui::PopupPlacementRequest{
        oneui::Rect{10.0f, 10.0f, 40.0f, 20.0f},
        oneui::Size{400.0f, 260.0f},
        oneui::Rect{5.0f, 7.0f, 180.0f, 90.0f},
        oneui::PopupPreferredPlacement::BottomEnd,
        4.0f
    });

    expectRect("PopupPlacement oversized clamped rect", result.rect, oneui::Rect{5.0f, 7.0f, 180.0f, 90.0f});
}

void testPopupDrawsBoxShadowWhenElevated() {
    oneui::Popup popup;
    popup.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    popup.setAnchorRect(oneui::Rect{20.0f, 20.0f, 80.0f, 30.0f});
    popup.setViewport(oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    popup.setAnchor(std::make_shared<oneui::Button>(L"Menu"));
    popup.anchor()->setPreferredSize(oneui::Size{80.0f, 30.0f});
    popup.setContent(std::make_shared<LayoutProbe>(oneui::Size{120.0f, 60.0f}));
    popup.setOpen(true);

    oneui::PopupStyleOverride style;
    style.elevation = 2.0f;
    style.radius = 10.0f;
    style.offset = 6.0f;
    popup.setStyleOverride(style);

    RecordingCanvas canvas;
    popup.paint(canvas);

    expectEqual("Popup paints one box shadow", static_cast<int>(canvas.boxShadows.size()), 1);
    if (!canvas.boxShadows.empty()) {
        expectRect("Popup shadow uses resolved content rect", canvas.boxShadows[0].rect, oneui::Rect{20.0f, 56.0f, 120.0f, 60.0f});
        expectNear("Popup shadow offset follows elevation", canvas.boxShadows[0].shadow.offset.y, 4.0f);
        expectNear("Popup shadow blur follows elevation", canvas.boxShadows[0].shadow.blurRadius, 20.0f);
        expectNear("Popup shadow radius uses style", canvas.boxShadows[0].radius, 10.0f);
    }
}

void testPopupOutsideHitTestFollowsClosePolicy() {
    oneui::Popup popup;
    popup.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    popup.setAnchorRect(oneui::Rect{20.0f, 20.0f, 80.0f, 30.0f});
    popup.setViewport(oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    popup.setAnchor(std::make_shared<oneui::Button>(L"Menu"));
    popup.anchor()->setPreferredSize(oneui::Size{80.0f, 30.0f});
    popup.setContent(std::make_shared<LayoutProbe>(oneui::Size{120.0f, 60.0f}));
    popup.setOpen(true);

    const oneui::Point anchorPoint{24.0f, 24.0f};
    const oneui::Point contentPoint{24.0f, 60.0f};
    const oneui::Point outsidePoint{220.0f, 160.0f};

    popup.setCloseOnOutsideClick(false);
    expectEqual("Popup non-closing outside hit-test keeps anchor active", popup.hitTest(anchorPoint) ? 1 : 0, 1);
    expectEqual("Popup non-closing outside hit-test keeps content active", popup.hitTest(contentPoint) ? 1 : 0, 1);
    expectEqual("Popup non-closing outside hit-test does not swallow outside", popup.hitTest(outsidePoint) ? 1 : 0, 0);

    popup.setCloseOnOutsideClick(true);
    expectEqual("Popup closing outside hit-test captures outside", popup.hitTest(outsidePoint) ? 1 : 0, 1);
}

void testPopupOutsidePointerPolicyCanBlockWithoutClosing() {
    oneui::Popup popup;
    popup.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    popup.setAnchorRect(oneui::Rect{20.0f, 20.0f, 80.0f, 30.0f});
    popup.setViewport(oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    popup.setAnchor(std::make_shared<oneui::Button>(L"Menu"));
    popup.anchor()->setPreferredSize(oneui::Size{80.0f, 30.0f});
    popup.setContent(std::make_shared<LayoutProbe>(oneui::Size{120.0f, 60.0f}));
    popup.setOpen(true);

    const oneui::Point outsidePoint{220.0f, 160.0f};

    popup.setOutsidePointerPolicy(oneui::PopupOutsidePointerPolicy::Block);
    expectEqual("Popup block outside policy getter",
                static_cast<int>(popup.outsidePointerPolicy()),
                static_cast<int>(oneui::PopupOutsidePointerPolicy::Block));
    expectEqual("Popup block outside hit-test captures outside", popup.hitTest(outsidePoint) ? 1 : 0, 1);
    expectEqual("Popup block outside mouse-down handled", popup.onMouseDown(oneui::MouseEvent{outsidePoint}) ? 1 : 0, 1);
    expectEqual("Popup block outside keeps popup open", popup.isOpen() ? 1 : 0, 1);
    expectEqual("Popup block outside wheel handled", popup.onMouseWheel(oneui::MouseWheelEvent{outsidePoint, 1.0f}) ? 1 : 0, 1);

    popup.setOutsidePointerPolicy(oneui::PopupOutsidePointerPolicy::Close);
    expectEqual("Popup close outside mouse-down handled", popup.onMouseDown(oneui::MouseEvent{outsidePoint}) ? 1 : 0, 1);
    expectEqual("Popup close outside closes popup", popup.isOpen() ? 1 : 0, 0);

    popup.setOpen(true);
    popup.setOutsidePointerPolicy(oneui::PopupOutsidePointerPolicy::PassThrough);
    expectEqual("Popup pass-through outside hit-test skips outside", popup.hitTest(outsidePoint) ? 1 : 0, 0);
    expectEqual("Popup pass-through outside mouse-down ignored", popup.onMouseDown(oneui::MouseEvent{outsidePoint}) ? 1 : 0, 0);
    expectEqual("Popup pass-through outside keeps popup open", popup.isOpen() ? 1 : 0, 1);
}

void testPopupInteractionModesMapToPointerPolicyAndOverlayOptions() {
    oneui::Popup popup;

    popup.setInteractionMode(oneui::PopupInteractionMode::Modeless);
    oneui::OverlayOptions modeless = popup.overlayOptions(3);
    expectEqual("Popup modeless mode getter",
                static_cast<int>(popup.interactionMode()),
                static_cast<int>(oneui::PopupInteractionMode::Modeless));
    expectEqual("Popup modeless pointer policy",
                static_cast<int>(popup.outsidePointerPolicy()),
                static_cast<int>(oneui::PopupOutsidePointerPolicy::PassThrough));
    expectEqual("Popup modeless overlay layer", modeless.layer, 3);
    expectEqual("Popup modeless overlay does not trap focus", modeless.trapsFocus ? 1 : 0, 0);
    expectEqual("Popup modeless overlay does not block outside pointer", modeless.blocksOutsidePointer ? 1 : 0, 0);

    popup.setInteractionMode(oneui::PopupInteractionMode::LightDismiss);
    oneui::OverlayOptions lightDismiss = popup.overlayOptions(5);
    expectEqual("Popup light-dismiss pointer policy",
                static_cast<int>(popup.outsidePointerPolicy()),
                static_cast<int>(oneui::PopupOutsidePointerPolicy::Close));
    expectEqual("Popup light-dismiss overlay layer", lightDismiss.layer, 5);
    expectEqual("Popup light-dismiss overlay does not trap focus", lightDismiss.trapsFocus ? 1 : 0, 0);
    expectEqual("Popup light-dismiss overlay does not block outside pointer", lightDismiss.blocksOutsidePointer ? 1 : 0, 0);

    popup.setInteractionMode(oneui::PopupInteractionMode::Modal);
    oneui::OverlayOptions modal = popup.overlayOptions(7);
    expectEqual("Popup modal pointer policy",
                static_cast<int>(popup.outsidePointerPolicy()),
                static_cast<int>(oneui::PopupOutsidePointerPolicy::Block));
    expectEqual("Popup modal overlay layer", modal.layer, 7);
    expectEqual("Popup modal overlay traps focus", modal.trapsFocus ? 1 : 0, 1);
    expectEqual("Popup modal overlay blocks outside pointer", modal.blocksOutsidePointer ? 1 : 0, 1);
}

void testPopupEscapeCloseFollowsClosePolicy() {
    oneui::Popup popup;
    popup.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    popup.setAnchor(std::make_shared<oneui::Button>(L"Menu"));
    popup.anchor()->setPreferredSize(oneui::Size{80.0f, 30.0f});
    popup.setContent(std::make_shared<LayoutProbe>(oneui::Size{120.0f, 60.0f}));
    popup.setOpen(true);

    popup.setCloseOnEscape(false);
    expectEqual("Popup non-closing Escape is not handled", popup.onKeyDown(oneui::KeyEvent{oneui::Key::Escape}) ? 1 : 0, 0);
    expectEqual("Popup non-closing Escape keeps popup open", popup.isOpen() ? 1 : 0, 1);

    popup.setCloseOnEscape(true);
    expectEqual("Popup closing Escape is handled", popup.onKeyDown(oneui::KeyEvent{oneui::Key::Escape}) ? 1 : 0, 1);
    expectEqual("Popup closing Escape closes popup", popup.isOpen() ? 1 : 0, 0);
}

void testPopupMouseFocusHandoffDelegatesKeyboardAndRestoresChildFocus() {
    int anchorClicks = 0;
    int contentClicks = 0;

    auto anchor = std::make_shared<oneui::Button>(L"Menu");
    anchor->setPreferredSize(oneui::Size{80.0f, 30.0f});
    anchor->setOnClick([&] {
        ++anchorClicks;
    });

    auto content = std::make_shared<oneui::Button>(L"Item");
    content->setPreferredSize(oneui::Size{120.0f, 60.0f});
    content->setOnClick([&] {
        ++contentClicks;
    });

    oneui::Popup popup;
    popup.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 180.0f});
    popup.setAnchor(anchor);
    popup.setContent(content);
    popup.setOpen(true);

    expectEqual("Popup content mouse down handled", popup.onMouseDown(oneui::MouseEvent{oneui::Point{16.0f, 50.0f}}) ? 1 : 0, 1);
    expectEqual("Popup content receives mouse focus", content->focused() ? 1 : 0, 1);
    expectEqual("Popup anchor is not focused after content focus", anchor->focused() ? 1 : 0, 0);
    expectEqual("Popup mouse focus is not focus-visible", content->focusVisible() ? 1 : 0, 0);

    expectEqual("Popup delegates keyboard to focused content", popup.onKeyDown(oneui::KeyEvent{oneui::Key::Space}) ? 1 : 0, 1);
    expectEqual("Popup focused content keyboard click count", contentClicks, 1);
    expectEqual("Popup focused content does not click anchor", anchorClicks, 0);

    expectEqual("Popup anchor mouse down handled", popup.onMouseDown(oneui::MouseEvent{oneui::Point{16.0f, 16.0f}}) ? 1 : 0, 1);
    expectEqual("Popup anchor receives mouse focus", anchor->focused() ? 1 : 0, 1);
    expectEqual("Popup content loses focus after anchor focus", content->focused() ? 1 : 0, 0);

    expectEqual("Popup delegates keyboard to focused anchor", popup.onKeyDown(oneui::KeyEvent{oneui::Key::Space}) ? 1 : 0, 1);
    expectEqual("Popup focused anchor keyboard click count", anchorClicks, 1);
    expectEqual("Popup focused anchor leaves content click count unchanged", contentClicks, 1);

    popup.onFocusChanged(false);
    expectEqual("Popup blur clears focused child state", anchor->focused() ? 1 : 0, 0);

    popup.onFocusChanged(true);
    expectEqual("Popup refocus restores last focused child", anchor->focused() ? 1 : 0, 1);

    popup.setOpen(false);
    expectEqual("Popup closing keeps anchor focus because anchor remains visible", anchor->focused() ? 1 : 0, 1);
}

void testLogViewSelectionAndCopy() {
    oneui::LogView view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 400.0f, 200.0f});
    view.appendLine(L"alpha", oneui::Color{26, 29, 34, 255});
    view.appendLine(L"beta", oneui::Color{229, 72, 77, 255});
    view.appendLine(L"gamma", oneui::Color{36, 203, 141, 255});
    auto clipboard = std::make_shared<oneui::MemoryClipboard>();
    view.setClipboard(clipboard);

    expectEqual("LogView line count", static_cast<int>(view.lineCount()), 3);
    // 默认 padding(6,10)/lineHeight 20：内容高 = 6+6+3x20 = 72。
    expectEqual("LogView content height", static_cast<int>(view.contentHeight()), 72);

    // 从第 0 行行首拖到第 2 行行尾：跨行选区，复制内容按行拼接（\r\n）。
    view.onMouseDown(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});
    view.onMouseMove(oneui::MouseEvent{oneui::Point{300.0f, 56.0f}});
    view.onMouseUp(oneui::MouseEvent{oneui::Point{300.0f, 56.0f}});
    expectEqual("LogView drag creates selection", view.hasSelection() ? 1 : 0, 1);
    expectWideEqual("LogView cross-line selected text", view.selectedText(), L"alpha\r\nbeta\r\ngamma");

    expectEqual("LogView Ctrl+C handled", view.onKeyDown(oneui::KeyEvent{oneui::Key::C, false, true}) ? 1 : 0, 1);
    expectWideEqual("LogView Ctrl+C copies selection", clipboard->text(), L"alpha\r\nbeta\r\ngamma");

    // 第 1 行内部分选：近似字宽 12*0.6=7.2，x=18→列1、x=32→列3，选中 "et"。
    view.onMouseDown(oneui::MouseEvent{oneui::Point{18.0f, 36.0f}});
    view.onMouseMove(oneui::MouseEvent{oneui::Point{32.0f, 36.0f}});
    view.onMouseUp(oneui::MouseEvent{oneui::Point{32.0f, 36.0f}});
    expectWideEqual("LogView partial line selection", view.selectedText(), L"et");

    expectEqual("LogView Ctrl+A handled", view.onKeyDown(oneui::KeyEvent{oneui::Key::A, false, true}) ? 1 : 0, 1);
    expectWideEqual("LogView Ctrl+A selects all", view.selectedText(), L"alpha\r\nbeta\r\ngamma");

    expectEqual("LogView Escape clears selection", view.onKeyDown(oneui::KeyEvent{oneui::Key::Escape, false, false}) ? 1 : 0, 1);
    expectEqual("LogView selection cleared", view.hasSelection() ? 1 : 0, 0);

    // 点击不拖动不产生选区；clear 后组件回到空状态。
    view.onMouseDown(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});
    view.onMouseUp(oneui::MouseEvent{oneui::Point{10.0f, 10.0f}});
    expectEqual("LogView click without drag keeps no selection", view.hasSelection() ? 1 : 0, 0);
    view.clearLines();
    expectEqual("LogView clear empties lines", static_cast<int>(view.lineCount()), 0);
    expectEqual("LogView empty not focusable", view.isFocusable() ? 1 : 0, 0);
}

} // namespace

int main() {
    testSingleLineTextEllipsizesByMeasuredWidth();
    testWidgetAccessibilityInfoReflectsSemanticAndDynamicState();
    testCommonControlsExposeDefaultAccessibilityInfo();
    testSelectionAndDataControlsExposeDefaultAccessibilityInfo();
    testCheckboxNoopOnChanged();
    testBoundCheckboxNoopOnChanged();
    testSwitchNoopOnChanged();
    testSliderNoopOnChanged();
    testTabsNoopOnChanged();
    testRadioGroupNoopOnChanged();
    testSelectNoopOnChanged();
    testListNoopOnChanged();
    testListSupportsOptionalSelection();
    testBoundListSelectedIndex();
    testBoundSelectEffectiveNoopOnChanged();
    testSelectSetItemsShrinkClampsAndEmitsOnce();
    testBoundSelectSetItemsShrinkUpdatesStateAndEmitsOnce();
    testSelectSetItemsSameEffectiveIndexDoesNotEmit();
    testSelectSetItemsEmptyClosesAndEmitsWhenSelectionChanges();
    testSelectFieldClickOpensWithoutCycling();
    testSelectOptionClickSelectsExactlyOnce();
    testSelectSameOptionClickDoesNotEmitOnChanged();
    testSelectSameHighlightedOptionCommitDoesNotEmitOnChanged();
    testDisabledSelectIgnoresMouseAndKeyboardEvents();
    testBoundDisabledSelectIgnoresMouseAndKeyboardEvents();
    testSelectOptionHitRegionsSelectExpectedItems();
    testOpenSelectReceivesOptionClickAboveLaterSibling();
    testOpenSelectClosesOnBlankViewClick();
    testOpenSelectClosesWhenAnotherSelectOpens();
    testOpenSelectOutsideControlClickPassesThrough();
    testSelectKeyboardOpenDoesNotChangeSelection();
    testSelectKeyboardHighlightThenCommitOnce();
    testSelectEscapeDismissesWithoutChangingSelection();
    testBoundSelectEscapeDismissesWithoutChangingSelection();
    testSelectKeyboardOpenClosesWhenFocusLost();
    testSelectLightDismissModelClosesWithoutCommit();
    testSelectClosesWhenDisabled();
    testSelectDisabledDuringOptionPressClearsPendingCommit();
    testSelectClosesWhenBoundDisabled();
    testSelectClosesWhenHidden();
    testSelectClosesWhenBoundHidden();
    testSelectClosesWhenFocusLost();
    testSelectClosesWhenItemsEmptied();
    testPressedButtonDisabledBeforeMouseUpDoesNotClick();
    testPressedButtonHiddenBeforeMouseUpDoesNotClick();
    testDisabledPressedChildDoesNotReceiveMouseUp();
    testHiddenPressedChildDoesNotReceiveMouseUp();
    testViewClearChildrenClearsFocusedAndPressedChild();
    testViewCanRequestFocusForNestedDescendant();
    testViewMouseMoveDoesNotInvalidateSiblingsWhenHoverUnchanged();
    testViewMouseMoveKeepsSingleHoveredChild();
    testViewMouseMoveSweepInvalidatesOnlyExitAndEnter();
    testViewPropagatesChildDirtyRectWithoutFullInvalidation();
    testViewPaintSkipsChildrenOutsideClipBounds();
    testViewCursorDelegatesToTopmostInteractiveChild();
    testFormFieldCursorDelegatesToEditableChild();
    testOverlayHostCursorDelegatesToContentAndOverlay();
    testClearInteractionStateSkipsIdleInteractiveControls();
    testViewPropagatesAnimationSchedulerToInteractiveChildren();
    testCompositeControlsUpdateBaseFocusState();
    testButtonMouseFocusIsNotFocusVisible();
    testButtonKeyboardFocusVisibleAndActivates();
    testButtonStyleOverridePaintsCustomColors();
    testButtonEmptyStyleOverrideKeepsDefaultPaint();
    testButtonStyleOverrideCanHideFocusRing();
    testCheckboxMouseFocusIsNotFocusVisible();
    testCheckboxKeyboardFocusVisible();
    testCheckboxStyleOverridePaintsCustomColors();
    testCheckboxStyleOverrideCanHideFocusRing();
    testRadioGroupStyleOverridePaintsCustomColorsAndGeometry();
    testRadioGroupHorizontalOrientationLaysOutColumns();
    testRadioGroupEmptyStyleOverrideKeepsDefaultPaint();
    testRadioGroupStyleOverrideCanHideFocusRingAndStylePressed();
    testRadioGroupDisabledStyleOverrideWinsAndClearRestoresDefault();
    testTabsStyleOverridePaintsCustomColorsAndGeometry();
    testTabsEmptyStyleOverrideKeepsDefaultPaint();
    testTabsStyleOverrideCanHideFocusRingAndStylePressed();
    testTabsDisabledStyleOverrideWinsAndClearRestoresDefault();
    testListStyleOverridePaintsCustomColorsAndGeometry();
    testListEmptyStyleOverrideKeepsDefaultPaint();
    testListStyleOverrideCanHideFocusRingAndStylePressed();
    testListDisabledStyleOverrideWinsAndClearRestoresDefault();
    testVirtualListPaintsOnlyViewportRowsAndMaintainsScrollSelection();
    testVirtualListUsesStandardMultipleSelectionSemantics();
    testVirtualListExposesStandardRowCommands();
    testVirtualListReportsReorderRequestsWithoutMutatingSelection();
    testVirtualListEmitsStableExternalItemDragWithoutBreakingReorder();
    testVirtualListReorderIndicatorUsesCssFocusStyle();
    testReorderableGridOwnsLayoutGestureAndCssIndicator();
    testReorderableGridEmitsExternalItemDragWithoutInternalReorder();
    testPointerActivationUsesSystemClickCountAndSeparateContextAction();
    testVirtualListCssControlsCompactTypographyAndScrollbar();
    testTreeViewStyleAdapterSharesListContract();
    testTreeViewReportsStableReorderIdsAndPreservesToggleBehavior();
    testTreeViewOwnsTransientExternalDropTargetState();
    testTableStyleOverridePaintsCustomColorsAndGeometry();
    testTableEmptyStyleOverrideKeepsDefaultPaint();
    testTableDisabledStyleAndClearRestoresDefault();
    testBadgeStyleOverridePaintsCustomColorsAndGeometry();
    testBadgeEmptyStyleOverrideKeepsVariantPaintAndClearRestoresDefault();
    testProgressBarStyleOverridePaintsCustomColorsAndGeometry();
    testProgressBarDisabledStyleAndClearRestoresDefault();
    testSeparatorStyleOverridePaintsCustomColorAndThickness();
    testSeparatorEmptyStyleOverrideAndClearRestoresDefault();
    testSliderMouseFocusIsNotFocusVisible();
    testSliderKeyboardFocusVisible();
    testSliderStyleOverridePaintsCustomColorsAndGeometry();
    testSliderEmptyStyleOverrideKeepsDefaultPaint();
    testSliderStyleOverrideCanHideFocusRingAndStylePressed();
    testSliderDisabledStyleOverrideWinsAndClearRestoresDefault();
    testNestedButtonKeyboardFocusVisible();
    testFieldMouseFocusIsNotFocusVisible();
    testFieldKeyboardFocusVisible();
    testSelectStyleOverridePaintsCustomColorsAndPopupGeometry();
    testSelectPopupGeometryUsesPopupPlacementAdapter();
    testSelectEmptyStyleOverrideKeepsDefaultPaint();
    testSelectStyleOverrideCanHideFocusRingAndStylePressed();
    testSelectDisabledStyleOverrideWinsAndClearRestoresDefault();
    testTextFieldCaretEditingKeys();
    testTextFieldSelectionEditingKeys();
    testTextFieldMouseDragSelection();
    testTextFieldClipboardOperations();
    testTextFieldClipboardKeyboardShortcuts();
    testTextFieldUndoRedoEditingPaths();
    testTextFieldUndoRedoTextInputAndBinding();
    testTextAreaSupportsMultilineEditingAndLineNavigation();
    testTextFieldDisabledDoesNotEditOrCut();
    testTextFieldReadOnlyAllowsSelectionCopyAndNavigationButNotMutation();
    testTextFieldPasswordModeMasksDisplayOnly();
    testTextFieldHorizontalScrollClipsAndFollowsCaret();
    testTextFieldCaretBlinkPaintsAndHidesOnSchedule();
    testTextFieldFocusedEmptyHidesPlaceholderAndOffsetsCaret();
    testTextFieldStartsCaretBlinkWhenSchedulerArrivesAfterFocus();
    testTextFieldPasswordHorizontalScrollMasksAndClips();
    testTextFieldUsesMeasuredTextWidthsForCaretHitTesting();
    testTextFieldStyleOverridePaintsCustomColors();
    testTextFieldEmptyStyleOverrideKeepsDefaultPaint();
    testTextFieldStyleOverrideCanHideFocusRingAndStylePlaceholder();
    testTextFieldReadOnlyStyleOverridePaintsStateWithoutCaret();
    testCardDrawsConfiguredShadow();
    testFormFieldHelperErrorAndRequiredMarker();
    testFormFieldPropagatesAccessibilityToChild();
    testFormFieldStyleOverridePaintsCustomColors();
    testValidationMessageStyleOverridePaintsCustomColors();
    testFormFieldChildLayoutUsesLabelPaddingAndControlWidth();
    testWrapLaysOutRowsWithPaddingAndGaps();
    testDockViewLaysOutRegionsWithPaddingGapAndHiddenChild();
    testAppShellLaysOutProductRegionsAndCollapsibleSidebar();
    testProductShellComputesReusableRemoteClientLayout();
    testMaterial3TokensResolveStateLayersAndElevation();
    testAnimationTransitionsInterpolateAndComplete();
    testStyleSheetResolvesCssLikeSelectorsAndStates();
    testStyleSheetParsesCssLikeRules();
    testStyleAdapterBuildsButtonAndTextFieldOverrides();
    testTextInputBridgeComputesHostEditorGeometryAndStates();
    testTitleBarBridgeComputesChromeGeometryAndStates();
    testSidebarNavBridgeComputesItemGeometryAndStates();
    testButtonBridgeComputesButtonGeometryAndStates();
    testStyleBoxPainterDrawsShadowFillBorderAndInset();
    testStyleBoxTransitionInterpolatesCommonVisualProperties();
    testCardCanPaintStyleBox();
    testToastPaintsAndDispatchesActions();
    testStatusStripActionStyleComesFromCss();
    testStateViewPaintsSemanticContentAndDispatchesAction();
    testCardLaysOutContentWithPadding();
    testIconPrimitivesProvideReusableNativeShapes();
    testButtonSupportsLeadingContentAndTrailingMetadata();
    testIconViewPaintsRegistryPrimitives();
    testWindowTitleBarPaintsAndDispatchesChromeActions();
    testNavItemPaintsSelectionAndDispatchesClick();
    testNavItemHoverKeepsSemanticForegroundFallback();
    testSwitchStyleSheetOverridePaintsCheckedState();
    testTextFieldAffixIconsPaintAndOffsetText();
    testSidebarNavItemsRemainVisibleAfterHoverSweep();
    testSplitViewRatioGapPaddingAndHiddenChild();
    testSplitViewResizableDividerHonorsMinimumExtents();
    testScrollViewWheelClampsToContentBounds();
    testScrollViewNoOverflowDoesNotScrollOrPaintThumb();
    testScrollViewIgnoresWheelOutsideBounds();
    testPopupPlacementBottomStartUsesAnchorAndOffset();
    testPopupPlacementFlipsWhenBottomOverflows();
    testPopupPlacementShiftsInsideViewport();
    testPopupPlacementRightStartUsesAnchorAndOffset();
    testPopupPlacementRightStartFlipsWhenRightOverflows();
    testPopupPlacementLeftStartFlipsWhenLeftOverflows();
    testPopupPlacementClampsOversizedPopup();
    testPopupDrawsBoxShadowWhenElevated();
    testPopupOutsideHitTestFollowsClosePolicy();
    testPopupOutsidePointerPolicyCanBlockWithoutClosing();
    testPopupInteractionModesMapToPointerPolicyAndOverlayOptions();
    testPopupEscapeCloseFollowsClosePolicy();
    testPopupMouseFocusHandoffDelegatesKeyboardAndRestoresChildFocus();
    testLogViewSelectionAndCopy();

    if (failures != 0) {
        std::cerr << failures << " control behavior test(s) failed.\n";
        return 1;
    }

    std::cout << "Control behavior tests passed.\n";
    return 0;
}
