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

// Match every serialized field before accepting a historical contract. The
// returned contract stays in Image, so PROP/BIND/CALL/event IDs retain their
// original meaning during validation, mutation and reload.
inline bool consumeComponentContract(Cursor& cursor, const ComponentContract& expected) {
    auto c = cursor;
    try {
        require(c.u32() == expected.version, "VERSION: component contract");
        const auto np = c.u32(), ne = c.u32();
        require(np == expected.properties.size() && ne == expected.events.size(), "BAD_IR: component counts");
        for (const auto& p : expected.properties)
            require(c.u32() == p.id && c.str() == p.name && c.u32() == p.tag &&
                    c.u32() == p.flags && c.str() == p.record, "BAD_IR: component property contract");
        for (const auto& e : expected.events) {
            require(c.u32() == e.id && c.str() == e.name && c.u32() == e.args.size(), "BAD_IR: event contract");
            for (const auto& a : e.args)
                require(c.str() == a.name && c.u32() == a.tag, "BAD_IR: event argument");
        }
        require(c.str() == expected.childRule && c.u32() == expected.modelProperty, "BAD_IR: child/model contract");
    } catch (const Error&) {
        return false;
    }
    cursor.pos = c.pos;
    return true;
}

inline const ComponentContract& decodeComponentContract(Cursor& c, const ComponentContract& current) {
    if (consumeComponentContract(c, current)) return current;
    if (current.name == "VirtualList")
        for (const auto& legacy : legacyVirtualListContracts())
            if (consumeComponentContract(c, legacy)) return legacy;
    auto peek = c;
    const auto version = peek.u32();
    require(version == current.version || (current.name == "VirtualList" && version == 2),
            "VERSION: component contract");
    throw Error("BAD_IR: component contract");
}
} // namespace oneview
