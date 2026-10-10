#include "validation.h"
#include <cmath>

namespace oneview
{
void validateChildren(const Node &n, const ComponentContract &c, const Location &at)
{
    auto rule = c.childRule;
    auto count = n.children.size();
    if (rule == "pane-collection" || rule == "dock-collection") {
        const auto expected = rule == "pane-collection" ? "Pane" : "DockItem";
        for (const auto &child : n.children)
            if (child.tag != expected)
                at.fail("WORKSPACE_CHILDREN: expected direct stable item");
    }
    if (rule == "leaf" && count)
        at.fail("CHILDREN: leaf component");
    if (rule == "one" && count != 1)
        at.fail("CHILDREN: exactly one child required");
    if ((rule == "two" || rule == "two-sidebar-first") && count != 2)
        at.fail("CHILDREN: exactly two children required");
    if (rule == "two-sidebar-first" && n.children[0].tag != "Sidebar")
        at.fail("CHILDREN: Sidebar must be first");
    if (rule == "workspace-regions" || rule == "dock-regions")
    {
        auto allowed = rule == "workspace-regions" ? std::set<std::string>{"TitleBar", "NavigationRail", "SessionBar",
                                                                           "WorkspaceBody", "StatusBar"}
                                                   : std::set<std::string>{"PanelHeader", "PanelBody", "PanelFooter"};
        std::set<std::string> seen;
        for (auto &child : n.children)
            if (!allowed.count(child.tag) || !seen.insert(child.tag).second)
                at.fail("CHILDREN: invalid or duplicate region");
        if (rule == "workspace-regions" ? !seen.count("WorkspaceBody")
                                        : !seen.count("PanelHeader") || !seen.count("PanelBody"))
            at.fail("CHILDREN: required region missing");
    }
    if (rule == "form-rows")
        for (auto &child : n.children)
            if (child.tag != "FormRow" || child.attrs.count("v-for"))
                at.fail("CHILDREN: FormGrid requires direct non-repeated FormRow");
    if (rule == "page-actions" &&
        std::count_if(n.children.begin(), n.children.end(), [](auto &child) { return child.tag == "ActionBar"; }) > 1)
        at.fail("CHILDREN: duplicate ActionBar");
}
void validateStatic(const std::string &component, const std::string &key, const std::string &text)
{
    const auto &p = registry().at(component).property(key);
    require(p.flags & 1, "STATIC_TYPE: property requires binding");
    if (p.tag == 1)
        require(text == "true" || text == "false", "STATIC_TYPE: expected bool");
    if (p.tag == 2)
    {
        require(std::regex_match(text, std::regex("-1|[0-9]+")), "STATIC_TYPE: selectedIndex >= -1");
        size_t end = 0;
        auto n = std::stoll(text, &end);
        require(end == text.size() && n <= std::numeric_limits<int>::max(), "STATIC_TYPE: integer range");
    }
    if (p.tag == 3)
    {
        require(std::regex_match(text, std::regex("[0-9]+(\\.[0-9]+)?")), "STATIC_TYPE: finite nonnegative number");
        float f = std::stof(text);
        oneui::ui::syntax::validateLayoutNumber(key, f);
        require(std::isfinite(f), "STATIC_TYPE: finite number");
    }
    static const std::map<std::string, std::set<std::string>> enums = {
        {"orientation", {"horizontal", "vertical"}},
        {"presentation", {"document", "segmented"}},
        {"variant", {"normal", "primary", "danger", "ghost"}},
        {"justify", {"start", "center", "end", "space-between"}},
        {"preset", {"fade", "expand"}},
        {"tone", {"neutral", "success", "warning", "error", "pending"}},
        {"appearance", {"flat", "outlined", "raised", "tinted"}},
        {"item-key", {"id"}}};
    if (enums.count(key))
        require(enums.at(key).count(text) != 0, "STATIC_ENUM: invalid " + key);
    if (key == "align")
        require(text == "start" || text == "center" || text == "end" ||
                    (text == "stretch" && (component == "Column" || component == "Row")),
                "STATIC_ENUM: align");
    if (key == "icon" || key == "symbol")
        require(oneui::ui::namedIcon(text).has_value(), "STATIC_ICON: unknown icon");
    require(utf8(text), "STATIC_UTF8: invalid UTF-8");
}
} // namespace oneview
