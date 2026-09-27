#pragma once
#include "oneui/controls/button.h"
#include "oneui/controls/label.h"
#include "oneui/layout/stack.h"
#include "oneui/layout/scroll_view.h"
#include <stdexcept>

namespace oneui::ui {

ONEUI_API const char* defaultCss();

struct Node {
    std::shared_ptr<Widget> widget;
    StackFlex flex{0, 1, {}, 0, std::numeric_limits<float>::infinity(), true};
    Node(std::shared_ptr<Widget> widget) : widget(std::move(widget)) {}
    Node& grow(float weight = 1) { flex.grow = weight; return *this; }
    Node& shrink(float weight) { flex.shrink = weight; return *this; }
    Node& basis(float extent) { flex.basis = extent; return *this; }
    Node& min(float extent) { flex.min = extent; return *this; }
    Node& max(float extent) { flex.max = extent; return *this; }
    template<class T> std::shared_ptr<T> as() const { return std::dynamic_pointer_cast<T>(widget); }
};

// Eager native construction. Views own their children and buttons own callbacks;
// capture application state weakly if it itself owns the root.
class ONEUI_API Builder {
public:
    explicit Builder(StackEngine engine = StackEngine::Yoga, std::string css = defaultCss(), bool layoutCache = true);
    Node text(std::wstring value, std::string role = {}) const;
    Node paragraph(std::wstring value, std::string role = {}) const;
    Node button(std::wstring value, std::function<void()> action, std::string role = {}) const;
    Node native(std::shared_ptr<Widget> value) const;
    Node row(std::vector<Node> children, std::string role = {}) const;
    Node column(std::vector<Node> children, std::string role = {}) const;
    Node flow(std::vector<Node> children, std::string role = {}) const;
    Node surface(std::vector<Node> children) const;
    Node page(std::vector<Node> children) const;
    Node toolbar(std::vector<Node> children) const;
    Node section(std::wstring title, Node metadata, std::vector<Node> content) const;
    Node pageHeader(Node title, Node description, std::vector<Node> actions) const;
    Node metric(std::wstring name, Node value, std::wstring unit) const;
    Node formRow(std::wstring label, std::wstring hint, Node control) const;
    Node emptyState(std::wstring title, std::wstring description, std::vector<Node> actions = {}) const;
    Node scroll(Node content) const;
    void style(const std::shared_ptr<Widget>& widget, std::string classes = {}) const;
private:
    Node stack(StackDirection direction, std::vector<Node> children, std::string role) const;
    std::shared_ptr<StyleSheet> sheet_;
    StackEngine engine_;
    bool layoutCache_;
};

} // namespace oneui::ui
