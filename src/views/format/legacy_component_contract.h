#pragma once
#include "legacy_baseline_controls.h"
#include "legacy_baseline_workspace.h"
#include "legacy_branch_contracts.h"
#include "legacy_input_contract.h"
#include "legacy_virtual_list_contract.h"

namespace oneview {
inline const std::vector<ComponentContract>& historicalComponentContracts() {
    static const auto contracts = [] {
        auto result = legacyBaselineControls();
        for (const auto* group : {&legacyBaselineWorkspace(), &legacyBranchContracts(),
                                 &legacyVirtualListContracts(), &legacyInputContracts()})
            result.insert(result.end(), group->begin(), group->end());
        return result;
    }();
    return contracts;
}

// Match every serialized field before accepting a historical contract. The
// original contract stays in Image through complete Image/Graph validation.
// Only then may name-based normalization enable current runtime capabilities.
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

inline const ComponentContract& decodeComponentContract(Cursor& cursor, const ComponentContract& current) {
    if (consumeComponentContract(cursor, current)) return current;
    auto peek = cursor;
    const auto version = peek.u32();
    bool knownVersion = version == current.version;
    for (const auto& legacy : historicalComponentContracts()) {
        if (legacy.name != current.name) continue;
        knownVersion = knownVersion || version == legacy.version;
        if (consumeComponentContract(cursor, legacy)) return legacy;
    }
    require(knownVersion, "VERSION: component contract");
    throw Error("BAD_IR: component contract");
}
} // namespace oneview
