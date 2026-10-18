#pragma once
#include "data_menu.h"
#include "runtime.h"
#include "internal/unicode.h"

namespace oneui::views {
inline bool applyDataMenu(Runtime& runtime, Instance& instance, ListState& state) {
    if (instance.element.component != "DataMenu") return false;
    std::vector<DataMenuEntry> entries;
    auto field = [&](const std::shared_ptr<Scope>& scope, const char* name) {
        return runtime.read(scope, runtime.image.schema.model(scope->table.model_id).properties.at(name).id);
    };
    auto optionalField = [&](const std::shared_ptr<Scope>& scope, const char* name) -> Value {
        const auto& properties = runtime.image.schema.model(scope->table.model_id).properties;
        const auto found = properties.find(name);
        if (found == properties.end()) return {};
        return runtime.read(scope, found->second.id);
    };
    for (size_t i = 0; i < state.items.size(); ++i) {
        const auto& item = state.items[i];
        DataMenuEntry entry;
        entry.id = state.keys[i];
        entry.parent = field(item, "parent").text;
        entry.text = unicode::fromUtf8(field(item, "text").text);
        entry.shortcut = unicode::fromUtf8(field(item, "shortcut").text);
        const auto icon = field(item, "icon").text;
        if (!icon.empty()) {
            entry.icon = ui::namedIcon(icon);
            check(entry.icon.has_value(), ONEUI_VIEW_TYPE, "Unknown menu icon");
        }
        entry.disabled = bool(field(item, "disabled").integer);
        entry.checked = bool(field(item, "checked").integer);
        entry.danger = bool(field(item, "danger").integer);
        entry.separator = bool(field(item, "separator").integer);
        entry.header = bool(optionalField(item, "header").integer);
        entries.push_back(std::move(entry));
    }
    try { DataMenu::validate(entries); }
    catch (const std::invalid_argument& error) { throw Failure(ONEUI_VIEW_TYPE, error.what()); }
    auto apply = [menu = instance.element.as<DataMenu>(), entries = std::move(entries)]() mutable {
        menu->setItems(std::move(entries));
    };
    if (runtime.staging && instance.published) runtime.commits.push_back(std::move(apply));
    else apply();
    return true;
}
} // namespace oneui::views
