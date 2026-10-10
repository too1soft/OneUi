#include "internal/view_profile.h"
#include "internal/unicode.h"
#include "runtime.h"
namespace oneui::views {
void Runtime::applyList(Instance &instance, const std::string &property, ListState &state) {
    ONEUI_VIEW_TIME(Rows);
    if(workspaceList(instance,property,state))return;
    auto &e = instance.element;
    auto wide = [](const std::string &s) { ONEUI_VIEW_TIME(Conversion); return unicode::fromUtf8(s); };
    auto field = [&](const std::shared_ptr<Scope> &scope, const char *name) {
        return read(scope, image.schema.model(scope->table.model_id).properties.at(name).id);
    };
    auto nested = [&](const std::shared_ptr<Scope> &scope, const char *name) -> ListState & {
        return list(scope, image.schema.model(scope->table.model_id).properties.at(name).id);
    };
    // Removing the selected stable key normalizes the model after the read
    // transaction, matching the existing keyed Table/Tabs binding behavior.
    if(property=="items" && (e.table || e.tabs)) {
        for(const auto &binding:instance.bindings) {
            if(binding.kind!=oneview::BIND || (binding.record[3]>>16)!=2)continue;
            auto scope=binding.scope.lock();
            auto selected=read(scope,binding.record[2]);
            if(selected.tag==4 && !selected.text.empty() &&
                std::find(state.keys.begin(),state.keys.end(),selected.text)==state.keys.end()) {
                Value empty;empty.tag=4;
                modelWrites.push_back({scope,binding.record[2],selected,empty});
            }
        }
    }
    if (e.component == "Select") {
        std::vector<std::wstring> values;
        for (const auto &item : state.items)
            values.push_back(wide(field(item, "value").text));
        auto found = instance.stringLists.find(property);
        if (found == instance.stringLists.end() || found->second != values) {
            factory.set(e, property, values);
            instance.stringLists[property] = std::move(values);
        }
    } else if (e.component == "Tabs") {
        std::vector<ui::TabItem> values;
        for (size_t i = 0; i < state.items.size(); ++i)
            values.push_back(
                {wide(state.keys[i]), wide(field(state.items[i], "title").text)});
        factory.set(e, property, values);
    } else if (e.component == "DataTable" && property == "items") {
        applyTableRows(instance, state);
    } else if (e.component == "DataTable" && property == "columns") {
        std::vector<TableColumn> columns;
        for (const auto &item : state.items) {
            TableColumn c;
            c.header = wide(field(item, "header").text);
            c.width = float(field(item, "width").number);
            auto align = field(item, "alignment").integer, kind = field(item, "kind").integer;
            check(c.width >= 0 && align >= 0 && align <= 2 && kind >= 0 && kind <= 1,
                  ONEUI_VIEW_TYPE, "Column value range");
            c.alignment = TextAlign(align);
            c.kind = TableCellKind(kind);
            c.action = bool(field(item, "action").integer);
            columns.push_back(std::move(c));
        }
        factory.set(e, property, columns);
    } else if (e.component == "TimeSeriesChart") {
        std::vector<TimeSeriesChartSeries> series;
        for (const auto &item : state.items) {
            TimeSeriesChartSeries s;
            s.name = wide(field(item, "name").text);
            auto rgba = uint32_t(field(item, "rgba").integer);
            s.color = {uint8_t(rgba >> 24), uint8_t(rgba >> 16), uint8_t(rgba >> 8), uint8_t(rgba)};
            for (const auto &value : nested(item, "values").items)
                s.values.push_back(field(value, "value").number);
            series.push_back(std::move(s));
        }
        factory.set(e, property, series);
    } else
        throw Failure(ONEUI_VIEW_TYPE, "No component list adapter");
}
} // namespace oneui::views
