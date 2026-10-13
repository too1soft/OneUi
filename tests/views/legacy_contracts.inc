// Included by the runtime test driver after fixture.h. Real old compiler output
// is checked before any normalization, then malformed historical contracts are
// rewritten with valid outer hashes so rejection cannot be a checksum accident.
#include "views/format/legacy_baseline_controls.h"
#include "views/format/legacy_baseline_workspace.h"
#include "views/format/legacy_branch_contracts.h"
#include "legacy_image.h"

namespace legacytest {
inline size_t componentIndex(const oneview::Image& image, const std::string& name) {
    for (size_t i = 0; i < image.components.size(); ++i)
        if (image.components[i].name == name) return i;
    throw std::runtime_error("missing legacy component");
}
inline void rejected(const oneview::Image& image) {
    bool failed = false;
    try { oneview::readImage(oneview::writeImage(image)); }
    catch (const oneview::Error&) { failed = true; }
    expect(failed, "malformed historical contract/graph was admitted");
}
inline void descriptorFailures(const oneview::Image& image, size_t index) {
    using Contract = oneview::ComponentContract;
    const std::vector<std::function<void(Contract&)>> changes = {
        [](auto& c) { c.version += 100; },
        [](auto& c) { c.childRule += "-forged"; },
        [](auto& c) { c.modelProperty = 999; },
        [](auto& c) { c.properties.pop_back(); },
        [](auto& c) { c.properties[0].id = 999; },
        [](auto& c) { c.properties[0].name += "-forged"; },
        [](auto& c) { c.properties[0].tag = 2; },
        [](auto& c) { c.properties[0].flags ^= 2; },
        [](auto& c) { c.properties[0].record = "@forged"; },
        [](auto& c) { c.events.push_back({999, "forged", {}}); },
    };
    for (const auto& change : changes) {
        auto bad = image;
        change(bad.components[index]);
        rejected(bad);
    }
    const auto& contract = image.components[index];
    if (!contract.events.empty()) {
        auto bad = image;
        bad.components[index].events[0].id = 999;
        rejected(bad);
        bad = image;
        bad.components[index].events[0].name += "-forged";
        rejected(bad);
        for (size_t i = 0; i < contract.events.size(); ++i) {
            if (contract.events[i].args.empty()) continue;
            bad = image;
            bad.components[index].events[i].args[0].name += "-forged";
            rejected(bad);
            bad = image;
            bad.components[index].events[i].args[0].tag = 999;
            rejected(bad);
            break;
        }
    }
}
inline void contracts(const std::filesystem::path& directory) {
    // Descriptor-level tests for both same-version branch variants. These are
    // generated unit inputs, explicitly distinct from frozen compiler assets.
    for (const auto& contract : oneview::legacyBranchContracts()) {
        const auto encoded = oneview::encodeComponents({contract});
        expect(oneview::encodeComponents(oneview::decodeComponents(encoded)) == encoded,
               "exact historical branch descriptor was rejected");
        auto altered = contract;
        altered.properties.back().flags ^= 1;
        bool failed = false;
        try { oneview::decodeComponents(oneview::encodeComponents({altered})); }
        catch (const oneview::Error&) { failed = true; }
        expect(failed, "same-version branch match accepted a different descriptor");
    }
    auto expected = oneview::legacyBaselineControls();
    const auto& workspace = oneview::legacyBaselineWorkspace();
    expected.insert(expected.end(), workspace.begin(), workspace.end());
    for (const auto& contract : expected) {
        const auto data = bytes(directory / ("Baseline" + contract.name + ".oneview"));
        auto decoded = frozenContracts(data);
        const auto raw = std::find_if(decoded.begin(), decoded.end(),
            [&](const auto& c) { return c.name == contract.name; });
        expect(raw != decoded.end() && oneview::encodeComponents({*raw}) ==
               oneview::encodeComponents({contract}), "real compiler COMP differs from frozen descriptor");
        auto image = oneview::readImage(data);
        const auto index = componentIndex(image, contract.name);
        expect(oneview::encodeComponents({image.components[index]}) ==
               oneview::encodeComponents({oneview::registry().at(contract.name)}),
               "historical image must normalize only after successful validation");
        image.components[index] = contract;
        descriptorFailures(image, index);
        // All baseline property/event IDs are preserved. A valid current
        // descriptor cannot legitimize a newer ID under the old descriptor.
        const auto& current = oneview::registry().at(contract.name);
        if (current.properties.size() > contract.properties.size()) {
            auto bad = image;
            auto binding = std::find_if(bad.records[oneview::BIND].begin(),
                bad.records[oneview::BIND].end(), [&](const auto& record) {
                    return bad.records[oneview::NODE][oneview::componentRoot(bad, record[0])][1] == index;
                });
            if (binding != bad.records[oneview::BIND].end()) {
                binding->at(1) = current.properties.back().id;
                rejected(bad);
            }
        }
    }
    const auto tabs = bytes(directory / "TabsV3.oneview");
    const auto actual = frozenContracts(tabs);
    const auto found = std::find_if(actual.begin(), actual.end(),
        [](const auto& c) { return c.name == "Tabs"; });
    const auto& branches = oneview::legacyBranchContracts();
    const auto expectedTabs = std::find_if(branches.begin(), branches.end(),
        [](const auto& c) { return c.name == "Tabs" && c.version == 3; });
    expect(found != actual.end() && expectedTabs != branches.end() &&
           oneview::encodeComponents({*found}) == oneview::encodeComponents({*expectedTabs}),
           "retained real Tabs v3 contract must match every frozen field");
    oneview::readImage(tabs);
}
} // namespace legacytest
