#include "oneui/layout/virtual_template_list.h"
#include "internal/scroll_phase_trace.h"
#include <limits>

namespace oneui::views {
std::shared_ptr<Instance> Runtime::buildVirtual(uint32_t node, const Scopes& context) {
    const auto& n = image.records[oneview::NODE][node];
    auto instance = std::make_shared<Instance>();
    instance->node = node;
    instance->scopes = context;
    instance->owner = weak_from_this();
    instance->element = factory.make("VirtualTemplateList", {}, scopeClasses.count(n[8]) ? scopeClasses.at(n[8]) : "");
    auto list = instance->element.as<VirtualTemplateList>();
    const auto repeatNode = image.records[oneview::EDGE][n[3]][1];
    const auto repeat = image.records[oneview::REPT][image.records[oneview::NODE][repeatNode][9]];
    auto weak = std::weak_ptr<Instance>(instance);
    list->setFactory([weak, repeat](size_t index, const std::string& key) -> std::shared_ptr<Widget> {
        auto owner = weak.lock();
        auto runtime = owner ? owner->owner.lock() : nullptr;
        check(runtime && !runtime->closing, ONEUI_VIEW_CLOSED, "Virtual template owner closed");
        auto found = owner->rows.find(key);
        if (found != owner->rows.end()) return found->second->element.widget;
        std::optional<timing::Measurements> measurements;
        std::optional<timing::Scope> mountTiming;
        if (traceRepeatBuild() && internal::scrollTraceEnabled()) {
            measurements.emplace();
            mountTiming.emplace(timing::Instantiate, &*measurements);
        }
        internal::ScrollPhaseTrace<5> phases(internal::tablePhaseTraceEnabled());
        const auto source = owner->repeatSource->items.at(index);
        auto scope = runtime->acquire({source->identity, source->vm}, source->table.model_id, false);
        try {
            runtime->connect(scope);
            const auto nested = owner->scopes.withRow(scope);
            phases.mark();
            auto row = runtime->build(runtime->image.records[oneview::TMPL][repeat[5]][1], nested);
            row->parent = owner;
            phases.mark();
            {
                std::optional<timing::Scope> updateTiming;
                if (measurements) updateTiming.emplace(timing::InitialUpdate);
                runtime->update(row);
            }
            phases.mark();
            // These descendants are assembled after the initial stylesheet pass.
            // Resolve dimensions into their new parents before measuring the row.
            runtime->factory.styles()->applyDetachedBaseLayouts(row->element.widget);
            phases.mark();
            if (owner->published) activate(row);
            owner->rows.emplace(key, row);
            phases.mark();
            if (measurements) {
                mountTiming.reset();
                internal::ScrollTraceEvent event;
                event.component = "view_virtual_row_build";
                event.phase = "total";
                event.object = reinterpret_cast<std::uintptr_t>(owner->element.widget.get());
                event.durationMs = std::chrono::duration<double, std::milli>(measurements->total).count();
                event.detailB = static_cast<double>(index);
                internal::writeScrollTrace(event);
                writeRepeatBuildTrace(event, *measurements);
            }
            phases.write("view_virtual_row_phases",
                reinterpret_cast<std::uintptr_t>(owner->element.widget.get()), static_cast<double>(index),
                {"scope", "build_tree", "update_bindings", "base_layout", "activate"});
            return row->element.widget;
        } catch (...) {
            runtime->closeScope(scope);
            throw;
        }
    }, [weak](const std::string& key) {
        auto owner = weak.lock();
        auto runtime = owner ? owner->owner.lock() : nullptr;
        if (!runtime) return;
        auto found = owner->rows.find(key);
        if (found == owner->rows.end()) return;
        runtime->closeScope(found->second->scopes.front());
        owner->rows.erase(found);
        if (!runtime->staging) {
            auto& scopes = runtime->scopes;
            scopes.erase(std::remove_if(scopes.begin(), scopes.end(), [](const auto& s) { return s->closed; }), scopes.end());
            runtime->factory.collectUnusedOwned();
        }
    });
    // Install before the first frame: no native rows exist until layout has a
    // real viewport, so mounting an unopened tool pane remains cheap.
    install(instance, node, context);
    wire(instance);
    virtualRepeat(instance);
    return instance;
}

void Runtime::virtualRepeat(const std::shared_ptr<Instance>& instance) {
    Observation observation(*this, instance);
    const auto& n = image.records[oneview::NODE][instance->node];
    const auto child = image.records[oneview::EDGE][n[3]][1];
    const auto& r = image.records[oneview::REPT][image.records[oneview::NODE][child][9]];
    auto& items = list(instance->scopes.at(r[3] & 0xffff), r[1]);
    auto widget = instance->element.as<VirtualTemplateList>();
    const auto& heightField = widget->tableRowHeightField();
    std::vector<float> rowHeights;
    if (!heightField.empty()) {
        const auto& model = image.schema.model(r[4]);
        const auto property = model.properties.find(heightField);
        check(property != model.properties.end() && property->second.tag == 3,
              ONEUI_VIEW_TYPE, "Virtual table height field must be a numeric row property");
        rowHeights.reserve(items.items.size());
        for (const auto& source : items.items) {
            // Offscreen models have no mounted binding context. Read their
            // sizes from the latest record whenever the owning list refreshes.
            source->dirty.insert(property->second.id);
            const auto height = read(source, property->second.id).number;
            check(height >= 0 && height <= std::numeric_limits<float>::max(),
                  ONEUI_VIEW_TYPE, "Invalid virtual table row height");
            rowHeights.push_back(static_cast<float>(height));
        }
    }
    if (instance->repeatSource == &items && instance->repeatSerial == items.serial) {
        for (const auto& [_, row] : instance->rows) update(row);
        if (!heightField.empty()) {
            auto publish = [widget, keys = items.keys, heights = std::move(rowHeights)]() mutable {
                widget->setTableRows(std::move(keys), std::move(heights), false);
            };
            if (staging) commits.push_back(std::move(publish)); else publish();
        }
        return;
    }
    std::map<std::string, std::shared_ptr<Scope>> sources;
    for (size_t i = 0; i < items.keys.size(); ++i) sources.emplace(items.keys[i], items.items[i]);
    std::vector<std::string> replaced;
    for (const auto& [key, row] : instance->rows) {
        auto source = sources.find(key);
        auto scope = row->scopes.front();
        if (source == sources.end() || source->second->vm != scope->vm || source->second->identity != scope->identity)
            replaced.push_back(key);
    }
    // Only mounted row scopes need presentation notifications. Offscreen data
    // will be read fresh when its template enters the viewport.
    if (instance->repeatSerial != items.serial) {
        for (const auto& [key, row] : instance->rows) {
            if (std::find(replaced.begin(), replaced.end(), key) != replaced.end()) continue;
            bool reset = items.lastReset;
            std::set<uint32_t> changed;
            for (const auto& delta : items.applied)
                if (delta.kind == ONEUI_LIST_UPDATE && std::find(delta.keys.begin(), delta.keys.end(), key) != delta.keys.end()) {
                    reset = reset || delta.properties.empty();
                    changed.insert(delta.properties.begin(), delta.properties.end());
                }
            auto scope = row->scopes.front();
            for (const auto& [_, property] : image.schema.model(scope->table.model_id).properties)
                if (reset || changed.count(property.id)) {
                    scope->dirty.insert(property.id);
                    if (property.tag == 5) scope->lists[property.id].reset = true;
                    mark(scope, property.id);
                }
        }
    }
    for (const auto& [key, row] : instance->rows)
        if (std::find(replaced.begin(), replaced.end(), key) == replaced.end()) update(row);
    auto publish = [instance, source = &items, serial = items.serial, keys = items.keys,
                    replaced, heights = std::move(rowHeights)]() mutable {
        auto list = instance->element.as<VirtualTemplateList>();
        for (const auto& key : replaced) list->invalidateKey(key);
        instance->repeatSource = source;
        instance->repeatSerial = serial;
        // Native child creation happens after the transaction's commits, from
        // layout/paint; never append commits while iterating the commit queue.
        list->setTableRows(std::move(keys), std::move(heights), false);
    };
    if (staging) commits.push_back(std::move(publish)); else publish();
}
} // namespace oneui::views
