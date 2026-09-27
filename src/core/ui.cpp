#include "oneui/ui.h"
#include "oneui/style_adapter.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/switch.h"
#include "ui_theme.h"
#include <sstream>
#include <stdexcept>
#include <cmath>

namespace oneui::ui {
const char* defaultCss() { return kOneUiFormTheme; }
Builder::Builder(StackEngine engine, std::string css, bool layoutCache) : sheet_(std::make_shared<StyleSheet>()), engine_(engine), layoutCache_(layoutCache) {
    std::string error;
    if (!sheet_->addRulesFromCss(css, &error)) throw std::invalid_argument(error);
    Stack probe;
    if (!probe.setEngine(engine)) throw std::runtime_error("OneUI was built without the selected layout engine");
}
void Builder::style(const std::shared_ptr<Widget>& widget, std::string classes) const {
    if (!widget) throw std::invalid_argument("Null component");
    StyleNode node;
    std::istringstream tokens(classes);
    for (std::string token; tokens >> token;) node.classes.push_back(token);
    if (auto button = std::dynamic_pointer_cast<Button>(widget)) {
        node.tag = "button"; button->setStyleOverride(buttonStyleOverrideFromStyleSheet(*sheet_, node));
    } else if (auto label = std::dynamic_pointer_cast<Label>(widget)) {
        node.tag = "label"; const auto box = sheet_->resolve(node);
        if (box.foreground) label->setColor(*box.foreground);
        if (box.fontSize) label->setFontSize(*box.fontSize);
        if (box.fontWeight) label->setFontWeight(*box.fontWeight);
    } else if (auto stack = std::dynamic_pointer_cast<Stack>(widget)) {
        node.tag = "stack"; const auto box = sheet_->resolve(node);
        stack->setStyleBox(box);
        if (box.gap) stack->setGap(*box.gap);
        if (box.padding) stack->setPadding(*box.padding);
    } else if (auto scroll = std::dynamic_pointer_cast<ScrollView>(widget)) {
        node.tag = "scroll-view"; scroll->setStyleBox(sheet_->resolve(node));
    } else if (auto field = std::dynamic_pointer_cast<TextField>(widget)) {
        node.tag = "input"; field->setStyleOverride(textFieldStyleOverrideFromStyleSheet(*sheet_, node));
    } else if (auto toggle = std::dynamic_pointer_cast<Switch>(widget)) {
        node.tag = "switch"; toggle->setStyleOverride(switchStyleOverrideFromStyleSheet(*sheet_, node));
    }
}
Node Builder::native(std::shared_ptr<Widget> value) const { if (!value) throw std::invalid_argument("Null component"); return Node{std::move(value)}; }
Node Builder::text(std::wstring value, std::string role) const {
    auto label = std::make_shared<Label>(std::move(value)); style(label, std::move(role)); return Node{label};
}
Node Builder::paragraph(std::wstring value, std::string role) const {
    auto node = text(std::move(value), std::move(role)); node.as<Label>()->setTextWrapping(true); return node.shrink(0);
}
Node Builder::button(std::wstring value, std::function<void()> action, std::string role) const {
    auto button = std::make_shared<Button>(std::move(value)); button->setOnClick(std::move(action));
    style(button, std::move(role)); return Node{button}.shrink(0);
}
Node Builder::stack(StackDirection direction, std::vector<Node> children, std::string role) const {
    auto stack = std::make_shared<Stack>(direction); stack->setEngine(engine_); stack->setLayoutCacheEnabled(layoutCache_);
    stack->setAlign(direction == StackDirection::Row ? StackAlign::Center : StackAlign::Stretch);
    style(stack, std::move(role));
    for (const auto& child : children) {
        const auto& f = child.flex;
        if (!child.widget || !std::isfinite(f.grow) || f.grow < 0 || !std::isfinite(f.shrink) || f.shrink < 0 ||
            !std::isfinite(f.min) || f.min < 0 || std::isnan(f.max) || f.max < f.min ||
            (f.basis && (!std::isfinite(*f.basis) || *f.basis < 0))) throw std::invalid_argument("Invalid component constraint");
        stack->add(child.widget); stack->setFlex(child.widget, f);
    }
    return Node{stack};
}
Node Builder::row(std::vector<Node> nodes, std::string role) const { return stack(StackDirection::Row, std::move(nodes), std::move(role)); }
Node Builder::column(std::vector<Node> nodes, std::string role) const { return stack(StackDirection::Column, std::move(nodes), std::move(role)); }
Node Builder::flow(std::vector<Node> nodes, std::string role) const {
    auto result = row(std::move(nodes), std::move(role));
    // Legacy is an explicitly selected comparison mode; it remains single-line.
    if (engine_ == StackEngine::Yoga) result.as<Stack>()->setWrap(true);
    return result;
}
Node Builder::surface(std::vector<Node> nodes) const { return column(std::move(nodes), "surface"); }
Node Builder::page(std::vector<Node> nodes) const { return column(std::move(nodes), "page"); }
Node Builder::toolbar(std::vector<Node> nodes) const { return flow(std::move(nodes), "toolbar").shrink(0); }
Node Builder::section(std::wstring title, Node metadata, std::vector<Node> content) const {
    content.insert(content.begin(), row({text(std::move(title), "section").grow(), std::move(metadata)}).shrink(0));
    return surface(std::move(content));
}
Node Builder::pageHeader(Node title, Node description, std::vector<Node> actions) const {
    return column({flow({title.grow(), toolbar(std::move(actions))}), std::move(description)}, "page-header");
}
Node Builder::metric(std::wstring name, Node value, std::wstring unit) const {
    style(value.widget, "metric-value");
    return column({text(std::move(name), "muted"), row({std::move(value), text(std::move(unit), "muted")})}, "metric");
}
Node Builder::formRow(std::wstring label, std::wstring hint, Node control) const {
    return flow({column({text(std::move(label)), paragraph(std::move(hint), "muted")}).basis(240).grow(), control.basis(220).grow()}, "form-row");
}
Node Builder::emptyState(std::wstring title, std::wstring description, std::vector<Node> actions) const {
    return surface({text(std::move(title), "section"), paragraph(std::move(description), "muted"), toolbar(std::move(actions))});
}
Node Builder::scroll(Node content) const {
    auto scroll = std::make_shared<ScrollView>(); scroll->setContent(content.widget); style(scroll); return Node{scroll};
}
} // namespace oneui::ui
