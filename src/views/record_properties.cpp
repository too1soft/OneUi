#include "internal/view_profile.h"
#include "internal/unicode.h"
#include "runtime.h"
#include "data_menu_records.h"
namespace oneui::views {
void Runtime::applyList(Instance &instance, const std::string &property, ListState &state) {
    ONEUI_VIEW_TIME(Rows);
    if(applyDataMenu(*this,instance,state))return;
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
    if (e.component == "Text" && property == "spans") {
        auto label = e.as<Label>();
        const auto bytes = unicode::toUtf8(label->text());
        auto boundary = [&](int64_t offset) {
            return offset >= 0 && uint64_t(offset) <= bytes.size() &&
                (size_t(offset) == bytes.size() || (uint8_t(bytes[size_t(offset)]) & 0xc0u) != 0x80u);
        };
        std::vector<TextStyleSpan> spans;
        int64_t previousEnd = 0;
        for (const auto& item : state.items) {
            const auto start = field(item, "start").integer, end = field(item, "end").integer;
            const auto rgba = field(item, "rgba").integer;
            check(start >= previousEnd && end > start && boundary(start) && boundary(end) &&
                  rgba >= 0 && uint64_t(rgba) <= 0xffffffffu, ONEUI_VIEW_TYPE, "Text span range/color");
            TextStyleSpan span;
            span.start = wide(bytes.substr(0, size_t(start))).size();
            span.end = wide(bytes.substr(0, size_t(end))).size();
            span.foreground = {uint8_t(rgba >> 24), uint8_t(rgba >> 16), uint8_t(rgba >> 8), uint8_t(rgba)};
            span.monospace = bool(field(item, "monospace").integer);
            spans.push_back(span); previousEnd = end;
        }
        label->setRichText(label->text(), std::move(spans));
    } else if (e.component == "Menu") {
        // Validate the entire replacement before touching native interaction state.
        struct Entry {
            std::string kind;
            std::wstring title, detail;
            std::optional<IconSymbol> icon;
            bool danger, disabled;
        };
        std::vector<Entry> entries;
        for (const auto &item : state.items) {
            auto kind = field(item, "kind").text;
            check(kind == "item" || kind == "header" || kind == "separator",
                  ONEUI_VIEW_TYPE, "Menu entry kind");
            const auto name = field(item, "icon").text;
            const auto icon = name.empty() ? std::optional<IconSymbol>{} : ui::namedIcon(name);
            check(name.empty() || icon.has_value(), ONEUI_VIEW_TYPE, "Menu entry icon");
            entries.push_back({kind, wide(field(item, "title").text), wide(field(item, "detail").text),
                icon, bool(field(item, "danger").integer), bool(field(item, "disabled").integer)});
        }
        auto menu = e.as<Menu>();
        menu->clearItems();
        for (auto &entry : entries) {
            if (entry.kind == "header") menu->addHeader(entry.title, entry.detail);
            else if (entry.kind == "separator") menu->addSeparator();
            else menu->setItemDisabled(menu->addItem(entry.title, entry.icon, entry.danger), entry.disabled);
        }
    } else if (e.component == "VirtualList") {
        std::vector<VirtualListItem> values;
        for (const auto &item : state.items) {
            VirtualListItem row{wide(field(item, "title").text), wide(field(item, "detail").text)};
            const auto &properties = image.schema.model(item->table.model_id).properties;
            auto optionalText = [&](const char *name) {
                auto found = properties.find(name);
                if (found == properties.end()) return std::wstring{};
                check(found->second.tag == 4, ONEUI_VIEW_TYPE, "VirtualList rich text type");
                return wide(field(item, name).text);
            };
            row.badge = optionalText("badge");
            row.trailing = optionalText("trailing");
            auto color = properties.find("indicator_rgba");
            if (color != properties.end()) {
                check(color->second.tag == 2, ONEUI_VIEW_TYPE, "VirtualList indicator type");
                const auto rgba = field(item, "indicator_rgba").integer;
                check(rgba >= 0 && uint64_t(rgba) <= 0xffffffffu, ONEUI_VIEW_TYPE, "VirtualList indicator range");
                row.indicatorColor = {uint8_t(rgba >> 24), uint8_t(rgba >> 16), uint8_t(rgba >> 8), uint8_t(rgba)};
                row.indicatorVisible = (rgba & 255) != 0;
            }
            values.push_back(std::move(row));
        }
        e.as<VirtualList>()->setRichItems(std::move(values));
    } else if (e.component == "List") {
        std::vector<ListItem> values;
        for (const auto &item : state.items) values.push_back({wide(field(item, "title").text), wide(field(item, "detail").text)});
        factory.set(e, property, values);
    } else if (e.component == "Select" || e.component == "SegmentedControl") {
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
