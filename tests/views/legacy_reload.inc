// Kept separate from record/pointer tests: these cover the two T34 contracts
// and prove new IDs stay illegal while an image advertises a frozen descriptor.
namespace legacytest {
inline const oneview::ComponentContract& baseline(const std::string& name) {
    for (const auto& contract : oneview::legacyBaselineControls())
        if (contract.name == name) return contract;
    for (const auto& contract : oneview::legacyBaselineWorkspace())
        if (contract.name == name) return contract;
    throw std::runtime_error("missing baseline descriptor");
}
inline void forgedNewIds(const std::filesystem::path& directory) {
    // The source fixtures contain well-typed current events/bindings. Replacing
    // only COMP with a valid old descriptor must reject before normalization.
    for (const auto* name : {"TimeSeriesChart", "InteractiveSurface", "Scroll", "Tabs"}) {
        auto newer = image(directory, (std::string("Baseline") + name + "New").c_str());
        newer.components[componentIndex(newer, name)] = baseline(name);
        rejected(newer);
    }
}
inline void retainedScroll(const std::filesystem::path& directory) {
    const auto old = image(directory, "BaselineScroll");
    const auto newer = image(directory, "BaselineScrollNew");
    Vm vm(old, "Root");
    initializeValues(vm, old);
    auto runtime = std::make_shared<Runtime>(old);
    runtime->initialize(&vm.table, &vm);
    const auto scroll = runtime->root->element.as<oneui::ScrollView>();
    scroll->setFrame({0, 0, 300, 200});
    expect(vm.invokes == 0, "old Scroll without viewport event dispatched");
    runtime->reload(newer);
    expect(runtime->root->element.widget == scroll, "Scroll reload replaced native widget");
    vm.invokes = 0;
    scroll->setFrame({4, 6, 360, 240});
    expect(vm.invokes == 1 && vm.invokedArgs.size() == 2 &&
           vm.invokedArgs[0].number == 360 && vm.invokedArgs[1].number == 240 &&
           vm.invokedArgs[0].number == scroll->frame().width &&
           vm.invokedArgs[1].number == scroll->frame().height,
           "unified Scroll reports actual viewport dimensions exactly once");
    scroll->setFrame({8, 10, 360, 240});
    expect(vm.invokes == 1, "position-only Scroll change emitted size event");
    runtime->reload(old);
    const auto removed = vm.invokes;
    scroll->setFrame({0, 0, 400, 280});
    expect(vm.invokes == removed, "old Scroll reload retained removed event");
    runtime->close();
    scroll->setFrame({0, 0, 410, 280});
    expect(vm.invokes == removed, "closed Scroll emitted event");
}
inline void retainedTabs(const std::filesystem::path& directory) {
    const auto old = image(directory, "BaselineTabs");
    const auto newer = image(directory, "BaselineTabsNew");
    Vm vm(old, "Root");
    initializeValues(vm, old);
    vm.text(id(old, "Root", "text"), "compact");
    auto runtime = std::make_shared<Runtime>(old);
    runtime->initialize(&vm.table, &vm);
    const auto tabs = runtime->root->element.as<oneui::Tabs>();
    expect(tabs && tabs->sizingMode() == oneui::TabsSizingMode::Compact,
           "old document Tabs sizing changed");
    runtime->reload(newer);
    expect(runtime->root->element.widget == tabs && tabs->sizingMode() == oneui::TabsSizingMode::Compact,
           "old Tabs did not retain identity and explicit compact sizing");
    vm.text(id(old, "Root", "text"), "equal");
    runtime->notify(runtime->rootScope, id(old, "Root", "text"));
    runtime->flush();
    expect(tabs->sizingMode() == oneui::TabsSizingMode::Equal,
           "new sizing binding failed on retained legacy Tabs");
    runtime->reload(old);
    expect(tabs->sizingMode() == oneui::TabsSizingMode::Compact,
           "old Tabs reload did not remove explicit sizing override");
    runtime->close();
    // The independently retained real v3 asset has its original application
    // schema. Exercise its load and same-schema reload without inventing a hash.
    const auto v3 = image(directory, "TabsV3");
    Vm original(v3, v3.schema.model(v3.schema.root).name.c_str());
    const auto& originalModel = v3.schema.model(v3.schema.root);
    original.values[originalModel.properties.at("minimum").id].number = 50;
    original.values[originalModel.properties.at("maximum").id].number = 96;
    const auto& itemModel = v3.schema.model(originalModel.properties.at("items").item);
    Vm equalItem(v3, itemModel.name.c_str()), compactItem(v3, itemModel.name.c_str());
    equalItem.text(itemModel.key, "equal");
    compactItem.text(itemModel.key, "compact");
    equalItem.text(itemModel.properties.at("title").id, "Equal tab");
    compactItem.text(itemModel.properties.at("title").id, "Compact tab");
    original.lists[originalModel.properties.at("items").id] = {&equalItem, &compactItem};
    original.text(originalModel.properties.at("selected").id, "equal");
    auto retained = std::make_shared<Runtime>(v3);
    retained->initialize(&original.table, &original);
    const auto widget = retained->root->element.widget;
    retained->reload(v3);
    expect(retained->root->element.widget == widget, "real Tabs v3 reload replaced root");
    // Add a current sizing binding to the real v3 graph, reusing its actual
    // selected-key utf8 VM property. This is a generated current test graph;
    // the permanent old input and its provenance are never rewritten.
    auto bound = v3;
    const auto tabsIndex = componentIndex(bound, "Tabs");
    const auto selected = std::find_if(bound.records[oneview::BIND].begin(),
        bound.records[oneview::BIND].end(), [&](const auto& record) {
            const auto node = oneview::componentRoot(bound, record[0]);
            return bound.records[oneview::NODE][node][1] == tabsIndex &&
                   bound.components[tabsIndex].property(record[1]).name == "selectedKey";
        });
    expect(selected != bound.records[oneview::BIND].end(), "real v3 selected-key binding missing");
    auto sizing = *selected;
    sizing[1] = oneview::registry().at("Tabs").property("sizing").id;
    sizing[3] = (sizing[3] & 0xffffu) | (1u << 16);
    const auto property = sizing[2];
    original.text(property, "equal");
    bound.records[oneview::BIND].push_back(sizing);
    std::stable_sort(bound.records[oneview::BIND].begin(), bound.records[oneview::BIND].end(),
                     [](const auto& left, const auto& right) { return left[0] < right[0]; });
    bound = oneview::readImage(oneview::writeImage(bound));
    retained->reload(bound);
    const auto nativeTabs = retained->root->element.as<oneui::Tabs>();
    expect(retained->root->element.widget == widget && nativeTabs &&
           nativeTabs->sizingMode() == oneui::TabsSizingMode::Equal,
           "real Tabs v3 failed to retain widget and add current sizing binding");
    original.text(property, "compact");
    retained->notify(retained->rootScope, property);
    retained->flush();
    expect(nativeTabs->sizingMode() == oneui::TabsSizingMode::Compact &&
           original.values[property].text == "compact" && nativeTabs->selectedIndex() == 1,
           "new binding on real v3 widget failed dynamic update");
    auto forged = bound;
    const auto rawContracts = frozenContracts(bytes(directory / "TabsV3.oneview"));
    const auto rawTabs = std::find_if(rawContracts.begin(), rawContracts.end(),
        [](const auto& contract) { return contract.name == "Tabs"; });
    expect(rawTabs != rawContracts.end(), "real Tabs v3 COMP missing");
    forged.components[tabsIndex] = *rawTabs;
    rejected(forged);
    retained->reload(v3);
    expect(retained->root->element.widget == widget, "real v3 reload after new binding replaced root");
    retained->close();
    expect(original.retains == original.releases && equalItem.retains == equalItem.releases &&
           compactItem.retains == compactItem.releases, "real Tabs v3 leaked root or item VM");
}
inline void all(const std::filesystem::path& directory) {
    contracts(directory);
    forgedNewIds(directory);
    baselineRuntime(directory);
    oldChartRecords(directory);
    retainedPointerEvents(directory);
    nativeHostCallbacks(directory);
    slottedPropertyMapping(directory);
    retainedScroll(directory);
    retainedTabs(directory);
}
} // namespace legacytest
