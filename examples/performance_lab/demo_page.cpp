#include "demo_page.hpp"
#include <oneui/style_adapter.h>
#include <oneui/ui_native_theme.h>

namespace lab {
using namespace oneui;
void applyDemoButton(Button &button, const StyleSheet &sheet, bool selected) {
    auto style = buttonStyleOverrideFromStyleSheet(
        sheet, {"button",
                selected ? std::vector<std::string>{"one-Button", "primary"}
                         : std::vector<std::string>{"one-Button"},
                0});
    auto initial = style;
    for (auto *state : {&initial.normal, &initial.hovered, &initial.pressed, &initial.disabled,
                        &initial.focusVisible})
        if (*state)
            (*state)->transition = TransitionSpec{0};
    button.setStyleOverride(initial);
    button.setStyleOverride(style);
}

ContentPage::ContentPage()
    : body_(std::make_shared<Stack>()), scroll_(std::make_shared<ScrollView>()),
      styles_(std::make_shared<StyleSheet>()) {
    body_->setEngine(StackEngine::Yoga);
    body_->setGap(16);
    body_->setPadding(Insets{24});
    scroll_->setContent(body_);
    setTheme(true, false);
}
void ContentPage::setTheme(bool dark, bool compact) {
    styles_ = std::make_shared<StyleSheet>();
    styles_->addRulesFromCss(
        ui::declarativeTheme(dark, compact ? ui::Density::Compact : ui::Density::Comfortable));
    StyleBox box;
    box.background.color = dark ? Color{23, 30, 33} : Color{255, 255, 255};
    body_->setStyleBox(box);
    scroll_->setStyleBox(box);
    scroll_->setScrollbarStyle(dark ? Color{102, 137, 115} : Color{112, 145, 123}, 4);
    body_->setStyleScope(styles_);
    body_->setPadding(Insets{compact ? 16.f : 24.f});
    body_->setGap(compact ? 10.f : 16.f);
    for (auto &label : labels_)
        label->setColor(dark ? Color{229, 236, 232} : Color{28, 43, 35});
    std::function<void(Widget &)> apply = [&](Widget &widget) {
        ui::applyNativeTheme(widget, *styles_, false);
        if (auto button = dynamic_cast<Button *>(&widget))
            applyDemoButton(*button, *styles_);
        if (auto view = dynamic_cast<View *>(&widget))
            for (auto &child : view->children())
                apply(*child);
    };
    apply(*body_);
}
std::shared_ptr<Label> ContentPage::text(std::wstring value, float size) {
    auto result = std::make_shared<Label>(std::move(value));
    result->setFontSize(size);
    result->setColor({229, 236, 232});
    labels_.push_back(result);
    add(result, size + 16);
    return result;
}
std::shared_ptr<Button> ContentPage::action(std::wstring title, std::function<void()> callback) {
    auto result = std::make_shared<Button>(std::move(title));
    applyDemoButton(*result, *styles_);
    result->setOnClick(std::move(callback));
    add(result);
    return result;
}
void ContentPage::add(const std::shared_ptr<Widget> &widget, float height) {
    body_->add(widget);
    body_->setFlex(widget, StackFlex{0, 0, height, height});
}
} // namespace lab
