#include "internal/view_profile.h"
#include "internal/unicode.h"
#include "runtime.h"
#include "data_menu_records.h"
#include "oneui/controls/data_grid.h"
#include "oneui/controls/tree_view.h"
#include "oneui/ui_icon_names.h"
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
    } else if (e.component == "TreeView" && property == "tree-items") {
        // Highlight ranges arrive in Unicode scalar values; the tree uses UTF-16 offsets.
        auto utf16 = [](const std::wstring &text, std::int64_t scalars) {
            std::size_t at = 0;
            for (std::int64_t n = 0; n < scalars && at < text.size(); ++n)
                at += (text[at] >= 0xd800 && text[at] <= 0xdbff && at + 1 < text.size()) ? 2 : 1;
            return at;
        };
        std::vector<TreeItem> items;
        items.reserve(state.items.size());
        for (const auto &item : state.items) {
            TreeItem node;
            node.id = wide(field(item, "id").text);
            node.parentId = wide(field(item, "parent").text);
            node.title = wide(field(item, "text").text);
            node.detail = wide(field(item, "detail").text);
            const auto &icon = field(item, "icon").text;
            if (!icon.empty()) {
                node.icon = ui::namedIcon(icon);
                check(node.icon.has_value(), ONEUI_VIEW_TYPE, "Tree node icon name");
            }
            node.badge = wide(field(item, "badge").text);
            node.classes = field(item, "classes").text;
            node.hasChildren = bool(field(item, "has_children").integer);
            node.expanded = bool(field(item, "expanded").integer);
            node.loading = bool(field(item, "loading").integer);
            const auto start = field(item, "highlight_start").integer, length = field(item, "highlight_len").integer;
            check(start >= 0 && length >= 0, ONEUI_VIEW_TYPE, "Tree node highlight range");
            node.highlightStart = utf16(node.title, start);
            node.highlightLength = utf16(node.title, start + length) - node.highlightStart;
            items.push_back(std::move(node));
        }
        e.as<TreeView>()->setItems(std::move(items));
    } else if (e.component == "DataGrid" && property == "grid-columns") {
        std::vector<DataGridColumn> columns;
        for (const auto &item : state.items) {
            DataGridColumn c;
            c.id = wide(field(item, "id").text);
            c.title = wide(field(item, "title").text);
            c.detail = wide(field(item, "detail").text);
            c.width = float(field(item, "width").number);
            const auto align = field(item, "align").integer, sort = field(item, "sort").integer;
            check(c.width >= 0 && align >= 0 && align <= 2 && sort >= 0 && sort <= 2, ONEUI_VIEW_TYPE,
                  "Grid column value range");
            c.alignment = TextAlign(align);
            c.sort = int(sort);
            c.key = bool(field(item, "key").integer);
            columns.push_back(std::move(c));
        }
        e.as<DataGrid>()->setColumns(std::move(columns));
    } else if (e.component == "DataGrid" && property == "grid-rows") {
        // The VM supplies only the materialized window; its first row index is window-start.
        std::vector<DataGridRow> rows;
        rows.reserve(state.items.size());
        for (const auto &item : state.items) {
            DataGridRow row;
            row.number = wide(field(item, "number").text);
            row.classes = field(item, "classes").text;
            auto &cells = nested(item, "cells");
            row.cells.reserve(cells.items.size());
            for (const auto &cell : cells.items)
                row.cells.push_back({wide(field(cell, "text").text), field(cell, "classes").text});
            rows.push_back(std::move(row));
        }
        auto grid = e.as<DataGrid>();
        grid->setWindow(grid->windowStart(), std::move(rows));
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
