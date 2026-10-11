#include "oneui/ui_theme.h"
#include "runtime.h"
#include "oneui/layout/pane_grid.h"
#include "oneui/layout/dock_workspace_view.h"
namespace oneui::views {
using namespace oneview;
namespace {
void activate(const std::shared_ptr<Instance> &node) noexcept {
    node->published = true;
    for (auto &child : node->children)
        activate(child);
    for (auto &row : node->rows)
        activate(row.second);
}
} // namespace
void Runtime::initialize(const OneUiViewModelVTable *table, void *vm, bool sharedStyles) {
    try {
        checkTable(table, image.schema.root); // No foreign callback before schema verification.
        rootScope = acquire({table, vm}, image.schema.root, false);
        std::string css = sharedStyles ? "" :
            (externalCss.empty() ? ui::declarativeTheme() : externalCss);
        for (const auto &r : image.records[CSS])
            css += '\n' + image.strings[r[2]];
        if (sharedStyles) styles->appendScoped(css);
        else styles->replace(css, density);
        root = build(image.records[TMPL][image.root][1], {rootScope});
        update(root);
        styles->applyBaseLayouts();
        prepareWorkspaceIndex();
        activate(root);
        connect(rootScope);
        building = false;
        alive();
        enqueue();
    } catch (...) {
        building = false;
        close();
        throw;
    }
}
std::shared_ptr<Instance> Runtime::build(uint32_t node, const Scopes &context, const Slots &slots) {
    alive();
    const auto n = image.records[NODE][node];
    std::shared_ptr<Instance> result;
    if (n[0] == 3) {
        const auto &call = image.records[CALL][n[9]];
        Slots supplied;
        for (uint32_t i = 0; i < call[3]; ++i) {
            const auto &slot = image.records[SLOT][call[2] + i];
            supplied.emplace(image.strings[slot[1]], SlotBody{slot[2], context});
        }
        auto savedPrefix = callPrefix;
        if (image.count(WSC1)) callPrefix += image.strings[n[11]] + "/";
        try { result = build(image.records[TMPL][call[1]][1], context, supplied); }
        catch (...) { callPrefix = std::move(savedPrefix); throw; }
        callPrefix = std::move(savedPrefix);
        if (scopeClasses.count(n[8]))
            factory.set(result->element, "class", scopeClasses.at(n[8]));
        install(result, node, context);
        wire(result);
        return result;
    }
    if (n[0] == 4) {
        const auto &slot = image.records[SLOT][n[9]];
        const auto found = slots.find(image.strings[slot[1]]);
        if (found != slots.end())
            return build(image.records[TMPL][found->second.body][1], found->second.scopes);
        if (slot[2] != none)
            return build(image.records[TMPL][slot[2]][1], context, slots);
        result = std::make_shared<Instance>();
        result->node = node;
        result->scopes = context;
        result->element = factory.make("Column");
        return result;
    }
    result = std::make_shared<Instance>();
    result->node = node;
    result->scopes = context;
    if (n[0] == 2) {
        result->transparent = image.records[REPT][n[9]][3] & repeatTransparent;
        result->repeatId = callPrefix + image.strings[n[11]];
        result->element = factory.make("Column");
        repeat(result);
        return result;
    }
    std::vector<ui::Element> children;
    for (uint32_t i = 0; i < n[4]; ++i) {
        auto child = build(image.records[EDGE][n[3] + i][1], context, slots);
        if (!child->transparent) children.push_back(child->element);
        child->parent = result;
        result->children.push_back(child);
    }
    auto type = image.components[n[1]].name;
    const std::string classes = scopeClasses.count(n[8]) ? scopeClasses.at(n[8]) : "";
    if (type == "NativeHost") {
        auto host = std::make_shared<ui::NativeHostView>();
        result->element = ui::Element(ui::Node(host), type);
        result->element.classes = classes;
        styles->add(result->element);
        const auto &record = image.records[HOST][n[9]];
        if (!(record[2] & 2)) hosts.emplace(image.strings[record[1]], host);
    } else if (type == "PaneGrid" || type == "DockWorkspace" || type == "ReorderableGrid") {
        std::shared_ptr<View> container;
        if(type=="PaneGrid")container=std::make_shared<PaneGrid>();
        else if(type=="ReorderableGrid")container=std::make_shared<ReorderableGrid>();
        else container=std::make_shared<DockWorkspaceView>();
        result->element = ui::Element(ui::Node(container), type);
        result->element.classes = classes;
        styles->add(result->element);
    } else if (type == "Pane") {
        auto pane=std::make_shared<PaneView>();
        for(auto& child:children) {
            pane->add(child.widget);
            if(child.component=="PaneHeader")pane->header=child.widget;
        }
        result->element=ui::Element(ui::Node(pane),type);
        result->element.classes=classes;
        styles->add(result->element);
    } else if (type == "DockItem") {
        auto item=std::make_shared<DockItemView>();
        for(auto& child:children) {
            item->add(child.widget);
            if(child.component=="PanelHeader")item->header=child.widget;
            if(child.component=="PanelBody")item->body=child.widget;
            if(child.component=="PanelFooter")item->footer=child.widget;
        }
        result->element=ui::Element(ui::Node(item),type);
        result->element.classes=classes;styles->add(result->element);
    } else
        result->element = factory.make(type, children, classes);
    if (n[10] != none) {
        const auto &span = image.records[SMAP][n[10]];
        factory.locate(result->element, image.strings[span[0]], int(span[1]));
    }
    install(result, node, context);
    wire(result);
    return result;
}
void Runtime::install(const std::shared_ptr<Instance> &instance, uint32_t owner,
                      const Scopes &context) {
    instance->owners.push_back(owner);
    auto overrideProperty = [&](uint32_t property) {
        instance->bindings.erase(
            std::remove_if(
                instance->bindings.begin(), instance->bindings.end(),
                [&](const Binding &b) { return b.kind == BIND && b.record[1] == property; }),
            instance->bindings.end());
    };
    for (const auto &r : image.records[PROP])
        if (r[0] == owner) {
            const auto &contract =
                image.components[image.records[NODE][componentRoot(image, owner)][1]];
            overrideProperty(r[1]);
            if(!workspaceScalar(*instance,contract.property(r[1]).name,constant(r[2])))
                setScalar(factory, instance->element, contract.property(r[1]).name, constant(r[2]));
            if (contract.property(r[1]).name == "disabled")
                instance->authoredDisabled = bool(constant(r[2]).integer);
        }
    for (const auto &r : image.records[BIND])
        if (r[0] == owner) {
            overrideProperty(r[1]);
            Binding binding{r, context.at(r[3] & 0xffff), BIND};
            const auto& contract = image.components[image.records[NODE][componentRoot(image, owner)][1]];
            if (!(instance->element.component == "Text" && contract.property(r[1]).name == "spans"))
                apply(instance, binding);
            instance->bindings.push_back(std::move(binding));
        }
    for (const auto &r : image.records[COND])
        if (r[0] == owner) {
            Binding binding{r, context.at(r[3] & 0xffff), COND};
            apply(instance, binding);
            instance->bindings.push_back(std::move(binding));
        }
    for (const auto &r : image.records[EVNT])
        if (r[0] == owner) {
            instance->events.erase(std::remove_if(instance->events.begin(), instance->events.end(),
                                                  [&](const Record &e) { return e[1] == r[1]; }),
                                   instance->events.end());
            // CALL is invoked with the same scope vector; records retain caller lexical depth.
            instance->events.push_back(r);
            auto &command = image.schema.model(r[4]).command(r[2]);
            for (auto id : {command.when, command.busy})
                if (id)
                    read(context.at(r[3] & 0xffff), id);
        }
}
void Runtime::apply(const std::shared_ptr<Instance> &instance, Binding &binding) {
    Observation observation(*this, instance);
    auto scope = binding.scope.lock();
    if (!scope || scope->closed)
        return;
    const auto &r = binding.record;
    if (binding.kind == COND) {
        auto mode = r[3] >> 16;
        auto value = mode & 2 ? constant(r[1]) : read(scope, r[1]);
        if (!binding.previous || !(value == *binding.previous))
            instance->element.widget->setVisible(bool(value.integer) != bool(mode & 1));
        binding.previous = value;
        return;
    }
    const auto &property = image.schema.model(scope->table.model_id).property(r[2]);
    auto c = componentRoot(image, r[0]);
    const auto &name = image.components[image.records[NODE][c][1]].property(r[1]).name;
    instance->applying = true;
    try {
        if (property.tag == 5)
            applyList(*instance, name, list(scope, property.id));
        else {
            auto value = read(scope, property.id);
            if (name == "disabled")
                instance->authoredDisabled = bool(value.integer);
            if (!binding.previous || !(value == *binding.previous)) {
                if (name == "class") {
                    if (!binding.baseClasses) binding.baseClasses = instance->element.classes;
                    instance->element.classes = *binding.baseClasses;
                    factory.set(instance->element, "class", value.text);
                } else if(!workspaceScalar(*instance,name,value))
                    setScalar(factory, instance->element, name, value);
                if (name == "basis")
                    if (auto parent = instance->parent.lock())
                        if (auto stack = parent->element.as<Stack>())
                            stack->setFlex(instance->element.widget, instance->element.flex);
            }
            binding.previous = std::move(value);
        }
    } catch (const Failure &error) {
        fail(error.result, error.what(), r[6]);
        this->error.section_kind = BIND;
        instance->applying = false;
        throw;
    } catch (const std::invalid_argument &) {
        fail(ONEUI_VIEW_TYPE, "Component property value rejected", r[6]);
        this->error.section_kind = BIND;
        instance->applying = false;
        throw Failure(ONEUI_VIEW_TYPE, "Component property value rejected");
    } catch (...) {
        instance->applying = false;
        throw;
    }
    instance->applying = false;
}
void Runtime::update(const std::shared_ptr<Instance> &instance) {
    alive();
    if (!instance->dirty)
        return;
    instance->dirty = false;
    Observation observation(*this, instance);
    // Rich spans refer to the final text of this transaction, independent of
    // authored attribute order. Resolve scalar text before its UTF-8 ranges.
    const auto isSpanList = [&](const Binding& b) {
        if (b.kind != BIND || instance->element.component != "Text") return false;
        auto c = componentRoot(image, b.record[0]);
        return image.components[image.records[NODE][c][1]].property(b.record[1]).name == "spans";
    };
    for (auto &b : instance->bindings) if (!isSpanList(b)) apply(instance, b);
    for (auto &b : instance->bindings) if (isSpanList(b)) apply(instance, b);
    bool conditionVisible = true;
    for (auto &b : instance->bindings)
        if (b.kind == COND && b.previous)
            conditionVisible =
                conditionVisible && (bool(b.previous->integer) != bool((b.record[3] >> 16) & 1));
    if (std::any_of(instance->bindings.begin(), instance->bindings.end(),
                    [](const Binding &b) { return b.kind == COND; }))
        instance->element.widget->setVisible(conditionVisible);
    if (image.records[NODE][instance->node][0] == 2) {
        repeat(instance);
        return;
    }
    // A table's disabled command must not disable row selection itself.
    // Button clicks use command availability as their enabled state; other
    // events are gated individually in invoke(), matching Mount::tableEvent.
    if (!instance->events.empty() && (instance->element.component=="Button" ||
                                     instance->element.component=="ToolButton" ||
                                     instance->element.component=="InteractiveSurface")) {
        bool gated = instance->authoredDisabled;
        for (auto &e : instance->events) {
            const auto &cmd = image.schema.model(e[4]).command(e[2]);
            auto scope = instance->scopes.at(e[3] & 0xffff);
            gated = gated || (cmd.when && !read(scope, cmd.when).integer) ||
                    (cmd.busy && read(scope, cmd.busy).integer);
        }
        for (auto &b : instance->bindings)
            if (b.kind == BIND && b.previous) {
                auto c = componentRoot(image, b.record[0]);
                if (image.components[image.records[NODE][c][1]].property(b.record[1]).name ==
                    "disabled")
                    gated = gated || bool(b.previous->integer);
            }
        instance->element.widget->setDisabled(gated);
    }
    for (auto &child : instance->children)
        update(child);
    for (auto &row : instance->rows)
        update(row.second);
    workspaceChildren(instance);
    preparePicker(instance);
}
} // namespace oneui::views
