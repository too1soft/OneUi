#pragma once
#include "ir.h"
#include <algorithm>

namespace oneview {
// Call only after both validateImage and validateGraph accepted the historical
// descriptors. In particular, a newer property/event must not become legal just
// because its component is about to gain newer runtime capabilities.
inline void normalizeLegacyContracts(Image& image) {
    const auto historical = image.components;
    auto component = [&](uint32_t owner) -> size_t {
        const auto root = componentRoot(image, owner);
        return image.records[NODE].at(root).at(1);
    };
    for (const auto section : {PROP, BIND}) {
        for (auto& record : image.records[section]) {
            const auto index = component(record.at(0));
            const auto& old = historical.at(index);
            const auto& property = old.property(record.at(1));
            const auto& current = registry().at(old.name).property(property.name);
            const bool chartRecord = old.name == "TimeSeriesChart" && old.version == 1 &&
                property.name == "series" && property.record == "oneui.v1.Series" &&
                current.record == "@chart.series";
            require(property.tag == current.tag && property.flags == current.flags &&
                    (property.record == current.record || chartRecord),
                    "BAD_IR: incompatible historical property mapping");
            record[1] = current.id;
        }
    }
    for (auto& record : image.records[EVNT]) {
        const auto index = component(record.at(0));
        const auto& old = historical.at(index);
        const auto& event = old.event(record.at(1));
        const auto& events = registry().at(old.name).events;
        const auto found = std::find_if(events.begin(), events.end(),
            [&](const auto& current) { return current.name == event.name; });
        require(found != events.end() && event.args.size() == found->args.size(),
                "BAD_IR: incompatible historical event mapping");
        for (size_t i = 0; i < event.args.size(); ++i)
            require(event.args[i].name == found->args[i].name &&
                    event.args[i].tag == found->args[i].tag,
                    "BAD_IR: incompatible historical event arguments");
        record[1] = found->id;
    }
    for (auto& contract : image.components)
        contract = registry().at(contract.name);
    // PROP/BIND owners may be CALL nodes or imported/slot component roots; using
    // componentRoot above gives all of them the same name-based mapping. This
    // includes tools VirtualList v2 IDs 15..17 -> current IDs 16..18.
    validateImage(image);
    validateGraph(image);
}
} // namespace oneview
