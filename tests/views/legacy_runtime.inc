#include "views/format/legacy_virtual_list_contract.h"
// Included after legacy_contracts.inc; the directory contains permanent old IR
// and newly compiled *New/*Click inputs sharing the exact same schema.
namespace legacytest {
inline oneview::Image image(const std::filesystem::path& directory, const char* name) {
    return oneview::readImage(bytes(directory / (std::string(name) + ".oneview")));
}
inline void initializeValues(Vm& vm, const oneview::Image& im) {
    vm.text(id(im, "Root", "text"), "legacy text");
    vm.values[id(im, "Root", "size")].number = 48;
    vm.values[id(im, "Root", "checked")].integer = 1;
    vm.values[id(im, "Root", "index")].integer = -1;
}
inline void baselineRuntime(const std::filesystem::path& directory) {
    for (const auto* name : {"Input", "Checkbox", "TimeSeriesChart", "Scroll", "VirtualList",
                            "Tabs", "DockWorkspace", "DragHandle", "PaneHeader", "InteractiveSurface"}) {
        const auto old = image(directory, (std::string("Baseline") + name).c_str());
        Vm vm(old, "Root");
        initializeValues(vm, old);
        auto runtime = std::make_shared<Runtime>(old);
        runtime->initialize(&vm.table, &vm);
        const auto widget = runtime->root->element.widget;
        // Re-encoding here is explicitly a current-runtime round trip, never
        // provenance for the permanent, independently frozen old compiler IR.
        runtime->reload(oneview::readImage(oneview::writeImage(runtime->image)));
        expect(runtime->root->element.widget == widget, "current round trip replaced legacy native widget");
        runtime->reload(old);
        expect(runtime->root->element.widget == widget, "reload back to old IR replaced native widget");
        if (const auto input = runtime->root->element.as<oneui::TextField>())
            expect(input->text() == L"legacy text", "legacy Input model text changed");
        if (const auto checkbox = runtime->root->element.as<oneui::Checkbox>())
            expect(checkbox->checked(), "legacy Checkbox model changed");
        runtime->close();
        expect(vm.retains == vm.releases && vm.connects == vm.disconnects,
               "legacy runtime root lease did not close once");
    }
}
inline void oldChartRecords(const std::filesystem::path& directory) {
    const auto old = image(directory, "BaselineTimeSeriesChart");
    const auto newer = image(directory, "BaselineTimeSeriesChartNew");
    Vm vm(old, "Root"), series(old, "oneui.v1.Series"), first(old, "oneui.v1.NumberItem"),
       second(old, "oneui.v1.NumberItem");
    initializeValues(vm, old);
    series.text(series.key, "cpu");
    series.text(id(old, "oneui.v1.Series", "name"), "CPU old record");
    series.values[id(old, "oneui.v1.Series", "rgba")].integer = 0x12345678;
    first.text(first.key, "first");
    second.text(second.key, "second");
    const auto value = id(old, "oneui.v1.NumberItem", "value");
    first.values[value].number = -4.5;
    second.values[value].number = 0;
    series.items = {&first, &second};
    const auto rootSeries = id(old, "Root", "series");
    vm.lists[rootSeries] = {&series};
    auto runtime = std::make_shared<Runtime>(old);
    runtime->initialize(&vm.table, &vm);
    const auto chart = runtime->root->element.as<oneui::TimeSeriesChart>();
    auto values = [&] {
        expect(chart && chart->series().size() == 1, "old Series list missing");
        const auto& actual = chart->series()[0];
        expect(actual.name == L"CPU old record" && actual.color.r == 0x12 && actual.color.g == 0x34 &&
               actual.color.b == 0x56 && actual.color.a == 0x78, "old Series name/color changed");
        expect(actual.values == std::vector<double>({first.values[value].number, 0}),
               "old NumberItem values changed, including negative and zero samples");
    };
    values();
    auto seriesScope = runtime->list(runtime->rootScope, rootSeries).items.at(0);
    auto pointScope = runtime->list(seriesScope, id(old, "oneui.v1.Series", "values")).items.at(0);
    first.values[value].number = 9.25;
    runtime->notify(pointScope, value);
    runtime->flush();
    values();
    chart->setFrame({0, 0, 240, 80});
    chart->onMouseMove({{30, 30}});
    expect(vm.invokes == 0, "old chart without inspection binding dispatched a command");
    runtime->reload(newer);
    expect(runtime->root->element.widget == chart, "chart reload replaced native widget");
    values();
    chart->onMouseMove({{210, 30}});
    expect(vm.invokes == 1 && vm.invokedArgs.size() == 2 && vm.invokedArgs[0].integer == 1,
           "retained chart failed to connect newly added inspection event");
    runtime->reload(old);
    chart->onMouseMove({{30, 30}});
    expect(vm.invokes == 1, "removed chart event still dispatches");
    runtime->close();
    chart->onMouseMove({{210, 30}});
    expect(vm.invokes == 1 && series.retains == series.releases &&
           first.retains == first.releases && second.retains == second.releases,
           "old chart nested leases/callback survived close");
}
inline void retainedPointerEvents(const std::filesystem::path& directory) {
    const auto old = image(directory, "BaselineInteractiveSurface");
    const auto drag = image(directory, "BaselineInteractiveSurfaceNew");
    const auto click = image(directory, "BaselineInteractiveSurfaceClick");
    Vm vm(old, "Root");
    initializeValues(vm, old);
    auto runtime = std::make_shared<Runtime>(old);
    runtime->initialize(&vm.table, &vm);
    const auto surface = runtime->root->element.as<oneui::InteractiveSurface>();
    surface->setFrame({0, 0, 200, 60});
    const oneui::MouseEvent down{{10, 10}, oneui::MouseButton::Left};
    const oneui::MouseEvent moved{{40, 20}, oneui::MouseButton::Left};
    const auto activate = old.schema.model("Root").commands.at("activate").id;
    const auto pointer = old.schema.model("Root").commands.at("pointer").id;
    const auto dragCommand = old.schema.model("Root").commands.at("drag").id;
    auto tap = [&] {
        expect(surface->onMouseDown(down) && surface->onMouseUp(down),
               "native pointer consumption changed");
    };
    tap();
    expect(vm.invokes == 1 && vm.invokedCommand == pointer, "old pointer event changed");
    runtime->reload(click);
    expect(runtime->root->element.widget == surface, "pointer reload replaced native widget");
    tap();
    expect(vm.invokes == 2 && vm.invokedCommand == activate,
           "removed pointer callback swallowed default semantic click");
    for (int i = 0; i < 2; ++i) {
        runtime->reload(drag);
        const auto before = vm.invokes;
        surface->onMouseDown(down);
        expect(surface->onMouseMove(moved) && surface->onMouseUp(moved),
               "new drag binding did not consume native drag");
        expect(vm.invokes == before + 2 && vm.invokedCommand == dragCommand &&
               vm.invokedArgs.size() == 8 && vm.invokedArgs[0].integer == 2,
               "reload must connect drag exactly once, with no extra click");
        runtime->reload(click);
        const auto after = vm.invokes;
        surface->onMouseDown(down);
        surface->onMouseMove(moved);
        surface->onMouseUp(moved);
        expect(vm.invokes == after + 1 && vm.invokedCommand == activate,
               "removed drag binding must restore click gesture semantics");
    }
    runtime->reload(drag);
    surface->onMouseDown(down);
    const auto started = vm.invokes;
    expect(surface->onMouseMove(moved) && vm.invokes == started + 1,
           "drag did not begin before unchanged reload");
    runtime->reload(drag);
    expect(surface->onMouseUp(moved) && vm.invokes == started + 2 &&
           vm.invokedCommand == dragCommand && vm.invokedArgs[0].integer == 2,
           "unchanged event reload cancelled active drag or duplicated delivery");
    runtime->reload(old);
    tap();
    expect(vm.invokedCommand == pointer, "old pointer binding failed after current reload");
    runtime->close();
    const auto closed = vm.invokes;
    tap();
    expect(vm.invokes == closed, "retained pointer dispatched after close");
}
inline void nativeHostCallbacks(const std::filesystem::path& directory) {
    const auto authored = image(directory, "BaselineNativeHost");
    Vm vm(authored, "Root");
    initializeValues(vm, authored);
    auto runtime = std::make_shared<Runtime>(authored);
    runtime->initialize(&vm.table, &vm);
    const auto host = std::dynamic_pointer_cast<oneui::ui::NativeHostView>(runtime->hosts.at("foreign"));
    auto native = std::make_shared<oneui::InteractiveSurface>();
    int clicks = 0, drags = 0;
    native->setOnClick([&] { ++clicks; });
    native->setOnDrag([&](const oneui::PointerDragEvent&) { ++drags; });
    host->attach(native, {});
    native->setFrame({0, 0, 200, 60});
    const oneui::MouseEvent down{{10, 10}, oneui::MouseButton::Left};
    const oneui::MouseEvent moved{{40, 20}, oneui::MouseButton::Left};
    auto removed = authored;
    removed.records[oneview::EVNT].clear();
    removed.records[oneview::EARG].clear();
    removed = oneview::readImage(oneview::writeImage(removed));
    for (const auto* graph : std::array<const oneview::Image*, 2>{&removed, &authored}) {
        native->onMouseDown(down);
        const auto before = drags;
        expect(native->onMouseMove(moved) && drags == before + 1,
               "native host drag did not begin");
        runtime->reload(*graph);
        expect(runtime->hosts.at("foreign") == host && host->children().front() == native,
               "reload replaced native host content");
        expect(native->onMouseUp(moved) && drags == before + 2,
               "template reload reset NativeHost active drag callback");
        const auto beforeClick = clicks;
        native->onMouseDown(down);
        native->onMouseUp(down);
        expect(clicks == beforeClick + 1 && vm.invokes == 0,
               "template reload overwrote or duplicated NativeHost callback");
    }
    host->detach();
    runtime->close();
    expect(vm.retains == vm.releases, "NativeHost isolation leaked VM");
}
inline void slottedPropertyMapping(const std::filesystem::path& directory) {
    auto current = image(directory, "BaselineVirtualListSlot");
    expect(!current.records[oneview::CALL].empty() && !current.records[oneview::SLOT].empty(),
           "slot mapping fixture must contain an imported single-root outlet");
    auto historical = current;
    const auto index = componentIndex(historical, "VirtualList");
    const auto& branches = oneview::legacyVirtualListContracts();
    const auto tools = std::find_if(branches.begin(), branches.end(), [](const auto& contract) {
        return contract.property(15).name == "trailing-width";
    });
    expect(tools != branches.end(), "tools historical contract missing");
    // This generated graph exercises slot ownership independently of the real
    // old compiler fixtures. It intentionally makes no compiler provenance claim.
    for (const auto section : {oneview::PROP, oneview::BIND})
        for (auto& record : historical.records[section]) {
            const auto node = oneview::componentRoot(historical, record[0]);
            if (historical.records[oneview::NODE][node][1] == index)
                record[1] = tools->property(current.components[index].property(record[1]).name).id;
        }
    historical.components[index] = *tools;
    const auto normalized = oneview::readImage(oneview::writeImage(historical));
    Vm vm(normalized, "Root");
    initializeValues(vm, normalized);
    auto runtime = std::make_shared<Runtime>(normalized);
    runtime->initialize(&vm.table, &vm);
    std::shared_ptr<oneui::VirtualList> list;
    std::function<void(const std::shared_ptr<Instance>&)> find = [&](const auto& instance) {
        if (instance->element.component == "VirtualList") {
            expect(!list, "slotted VirtualList must be unique");
            list = instance->element.as<oneui::VirtualList>();
        }
        for (const auto& child : instance->children) find(child);
    };
    find(runtime->root);
    expect(list && list->richMetrics().trailingWidth == 48 && list->richMetrics().trailingGap == 6 &&
           list->selectionMode() == oneui::SelectionMode::Single,
           "slot property mapping confused old trailing-width with current multiple");
    vm.values[id(normalized, "Root", "size")].number = 42;
    runtime->notify(runtime->rootScope, id(normalized, "Root", "size"));
    runtime->flush();
    expect(list->richMetrics().trailingWidth == 42, "slotted old binding lost lexical VM scope");
    runtime->close();
    expect(vm.retains == vm.releases, "slotted normalization leaked VM");
}
} // namespace legacytest
