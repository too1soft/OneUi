#pragma once
#include "component_contract.h"

namespace oneview {
// Frozen COMP records emitted by terminal 9573041 and tools 374a15f.
// Both called their different contracts v2. Never infer a layout from version
// alone, or derive the historical rows from the mutable current registry.
inline const std::vector<ComponentContract>& legacyVirtualListContracts() {
    static const std::vector<ComponentContract> contracts = [] {
        ComponentContract terminal{"VirtualList", "leaf", 2, 10};
        terminal.properties = {
            {1, 3, 1, "basis", ""},
            {2, 4, 1, "class", ""},
            {3, 4, 3, "description", ""},
            {4, 1, 3, "disabled", ""},
            {5, 3, 1, "grow", ""},
            {6, 5, 2, "items", "@list.item"},
            {7, 3, 1, "max", ""},
            {8, 3, 1, "min", ""},
            {9, 4, 3, "name", ""},
            {10, 2, 7, "selectedIndex", ""},
            {11, 3, 1, "shrink", ""},
            {12, 1, 3, "visible", ""},
            {13, 3, 3, "row-height", ""},
            {14, 3, 3, "scroll-offset", ""},
        };
        terminal.events = {{1, "activate", {}}};
        auto tools = terminal;
        tools.properties.push_back({15, 3, 3, "trailing-width", ""});
        tools.properties.push_back({16, 3, 3, "trailing-gap", ""});
        tools.properties.push_back({17, 1, 3, "activate-on-single-click", ""});
        terminal.properties.push_back({15, 1, 3, "multiple", ""});
        terminal.events.push_back({2, "selectionChanged", {{"index", 2}, {"selected", 1}}});
        return std::vector<ComponentContract>{terminal, tools};
    }();
    return contracts;
}

} // namespace oneview
