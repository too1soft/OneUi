#pragma once
#include "ast.h"
#include "ir.h"
#include <string_view>
namespace oneview {
struct ChildShape
{
    std::string_view tag;
    bool repeated;
};

template <class ChildAt>
void validateChildSequence(size_t count, ChildAt childAt, const ComponentContract &c, const Location &at)
{
    const auto &rule = c.childRule;
    if (rule == "virtual-rows" && (count != 1 || !childAt(0).repeated))
        at.fail("VIRTUAL_ROWS: exactly one keyed v-for child required");
    if (rule == "header-slots")
        for (size_t i = 0; i < count; ++i)
            if (childAt(i).tag != "HeaderSlot")
                at.fail("HEADER_SLOT: invalid native slot");
    if (rule == "pane-collection" || rule == "dock-collection" || rule == "grid-collection")
    {
        const auto expected = rule == "pane-collection" ? "Pane" : rule == "grid-collection" ? "GridItem" : "DockItem";
        for (size_t i = 0; i < count; ++i)
            if (childAt(i).tag != expected)
                at.fail("WORKSPACE_CHILDREN: expected direct stable item");
    }
    if (rule == "leaf" && count)
        at.fail("CHILDREN: leaf component");
    if (rule == "one" && count != 1)
        at.fail("CHILDREN: exactly one child required");
    if (rule == "optional-one" && count > 1)
        at.fail("CHILDREN: at most one child required");
    if ((rule == "two" || rule == "two-sidebar-first") && count != 2)
        at.fail("CHILDREN: exactly two children required");
    if (rule == "two-sidebar-first" && childAt(0).tag != "Sidebar")
        at.fail("CHILDREN: Sidebar must be first");
    if (rule == "workspace-regions" || rule == "dock-regions")
    {
        static const std::set<std::string_view> workspace = {
            "TitleBar", "NavigationRail", "SessionBar", "WorkspaceBody", "StatusBar"};
        static const std::set<std::string_view> dock = {"PanelHeader", "PanelBody", "PanelFooter"};
        const auto &allowed = rule == "workspace-regions" ? workspace : dock;
        std::set<std::string_view> seen;
        for (size_t i = 0; i < count; ++i)
        {
            const auto tag = childAt(i).tag;
            if (!allowed.count(tag) || !seen.insert(tag).second)
                at.fail("CHILDREN: invalid or duplicate region");
        }
        if (rule == "workspace-regions" ? !seen.count("WorkspaceBody")
                                        : !seen.count("PanelHeader") || !seen.count("PanelBody"))
            at.fail("CHILDREN: required region missing");
    }
    if (rule == "form-rows")
        for (size_t i = 0; i < count; ++i)
        {
            auto child = childAt(i);
            if (child.tag != "FormRow" || child.repeated)
                at.fail("CHILDREN: FormGrid requires direct non-repeated FormRow");
        }
    if (rule == "page-actions")
    {
        size_t bars = 0;
        for (size_t i = 0; i < count; ++i)
            if (childAt(i).tag == "ActionBar" && ++bars > 1)
                at.fail("CHILDREN: duplicate ActionBar");
    }
}

void validateChildren(const Node &, const ComponentContract &, const Location &);
void validateStatic(const std::string &, const std::string &, const std::string &);
}
