#include "runtime.h"
namespace oneui::views {
using namespace oneview;
namespace {
void activate(const std::shared_ptr<Instance> &node) noexcept {
    node->published = true;
    for (auto &child : node->children)
        activate(child);
    for (auto &[_, row] : node->rows)
        activate(row);
}
} // namespace
void Runtime::repeat(const std::shared_ptr<Instance> &instance) {
    Observation observation(*this, instance);
    const auto &r = image.records[REPT][image.records[NODE][instance->node][9]];
    auto &items = list(instance->scopes.at(r[3] & 0xffff), r[1]);
    // Child-property notifications do not change list identity or order.
    // Keep their widget tree and only visit dirty descendants. A shared list
    // consumer may have refreshed first, so compare serials, not dirty bits.
    if (instance->repeatSource == &items && instance->repeatSerial == items.serial) {
        for (auto &row : instance->rows)
            update(row.second);
        return;
    }
    std::map<std::string, std::shared_ptr<Instance>> next;
    std::vector<std::shared_ptr<Widget>> widgets;
    auto stack = instance->element.as<Stack>();
    for (size_t i = 0; i < items.keys.size(); ++i) {
        auto found = instance->rows.find(items.keys[i]);
        std::shared_ptr<Instance> row;
        if (found != instance->rows.end() &&
            (instance->transparent
                 ? found->second->scopes.front()->sourceScope.lock() == items.items[i]
                 : found->second->scopes.front() == items.items[i]))
            row = found->second;
        else {
            auto nested = instance->scopes;
            auto rowScope = instance->transparent
                                ? renderScope(items.items[i], instance, items.keys[i])
                                : items.items[i];
            nested.insert(nested.begin(), rowScope);
            auto prefix = std::move(callPrefix);
            callPrefix.clear();
            try {
                row = build(image.records[TMPL][r[5]][1], nested);
            } catch (...) {
                callPrefix = std::move(prefix);
                throw;
            }
            callPrefix = std::move(prefix);
        }
        widgets.push_back(row->element.widget);
        row->parent = instance;
        if (instance->transparent) {
            auto markProperties = [&](const std::vector<uint32_t> &properties) {
                const auto &scope = row->scopes.front();
                for (const auto &[_, p] : image.schema.model(scope->table.model_id).properties)
                    if (properties.empty() ||
                        std::find(properties.begin(), properties.end(), p.id) != properties.end()) {
                        scope->dirty.insert(p.id);
                        if (p.tag == 5)
                            scope->lists[p.id].reset = true;
                        mark(scope, p.id);
                    }
            };
            if (items.lastReset)
                markProperties({});
            else
                for (const auto &d : items.applied)
                    if (d.kind == ONEUI_LIST_UPDATE &&
                        std::find(d.keys.begin(), d.keys.end(), items.keys[i]) != d.keys.end())
                        markProperties(d.properties);
        }
        update(row);
        next.emplace(items.keys[i], std::move(row));
    }
    if (instance->transparent) {
        auto pending = std::make_shared<decltype(next)>(std::move(next));
        instance->pendingRows = pending;
        auto commit = [instance, pending, source = &items, serial = items.serial] {
            instance->rows.swap(*pending);
            instance->pendingRows.reset();
            instance->repeatSource = source;
            instance->repeatSerial = serial;
            if (instance->published)
                activate(instance);
        };
        if (staging && instance->published) {
            rollbacks.push_back([instance] { instance->pendingRows.reset(); });
            commits.push_back(std::move(commit));
        } else
            commit();
        return;
    }
    if (stack->children() == widgets && instance->rows == next) {
        instance->repeatSource = &items;
        instance->repeatSerial = items.serial;
        return;
    }
    auto cleanupFlex = [stack, widgets] {
        for (const auto &widget : widgets)
            if (std::find(stack->children().begin(), stack->children().end(), widget) ==
                stack->children().end())
                stack->clearFlex(widget);
    };
    if (staging)
        rollbacks.push_back(cleanupFlex);
    for (const auto &row : next)
        if (std::find(stack->children().begin(), stack->children().end(),
                      row.second->element.widget) == stack->children().end())
            stack->setFlex(row.second->element.widget, row.second->element.flex);
    auto prepared = stack->prepareChildren(std::move(widgets));
    auto commit = [instance, stack, prepared, source = &items, serial = items.serial,
                   next = std::move(next)]() mutable {
        for (const auto &row : instance->rows)
            if (!next.count(row.first) || next.at(row.first) != row.second)
                try {
                    stack->clearFlex(row.second->element.widget);
                } catch (...) {
                }
        prepared->commit();
        instance->rows.swap(next);
        instance->repeatSource = source;
        instance->repeatSerial = serial;
        if (instance->published)
            activate(instance);
    };
    if (staging && instance->published)
        commits.push_back(std::move(commit));
    else
        commit();
}
} // namespace oneui::views
