#pragma once
#include <string_view>
// Included after ui_declarative.h's definitions. Query application belongs to
// declarative composition; the parser/evaluator and isolated layout are core.
namespace oneui::ui {
inline void StyleRegistry::updateAuthoredFlex(const std::shared_ptr<Widget>& widget,const StackFlex& flex) {
    invalidateBaseLayouts();
    if (!hasCapturedLayoutDefaults_) return;
    if (entryIndices_) {
        if (const auto* entry = indexedEntry(widget); entry && entry->layoutDefaults)
            entry->layoutDefaults->flex = flex;
        return;
    }
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        if (it->widget.lock() != widget) continue;
        if (it->layoutDefaults) it->layoutDefaults->flex = flex;
        return;
    }
}
inline void StyleRegistry::applyBaseLayouts() {
    applyBaseLayouts(true);
}
inline void StyleRegistry::refreshBaseLayouts() {
    applyBaseLayouts(false);
}
inline StyleRegistry::BaseLayoutStyle StyleRegistry::resolveBaseLayout(const Entry& entry) const {
    StyleNode node{syntax::components().at(entry.component), {"one-"+entry.component}, 0};
    oneui::detail::appendStyleClasses(node.classes, entry.classes);
    BaseLayoutStyle base{sheet_.resolve(node), sheet_.resolveLayout(node)};
    const auto& box = base.box;
    const auto& layout = base.layout;
    base.applies = box.width || box.height || box.minWidth || box.minHeight ||
        layout.maxWidth || layout.maxHeight || box.flexBasis || layout.flexBasisPercent ||
        layout.flexOrder || box.flexGrow || box.flexShrink || box.display ||
        box.flexDirection || box.flexWrap;
    return base;
}

inline void StyleRegistry::applyDetachedBaseLayouts(const std::shared_ptr<Widget>& root) {
    const oneui::detail::view_mount_timing::Scope measure(oneui::detail::view_mount_timing::BaseLayout);
    auto view = std::dynamic_pointer_cast<View>(root);
    if (!view) return;

    // The root has no registered parent yet. Resolve the new relationships
    // within this row without restoring the dimensions of the existing tree.
    std::vector<std::shared_ptr<View>> pending{std::move(view)};
    bool applied = false;
    while (!pending.empty()) {
        auto parent = std::move(pending.back());
        pending.pop_back();
        const auto stack = indexedEntry(parent) ? std::dynamic_pointer_cast<Stack>(parent) : nullptr;
        const auto children = parent->children();
        for (const auto& child : children) {
            if (stack) {
                if (const auto* entry = indexedEntry(child)) {
                    const auto base = resolveBaseLayout(*entry);
                    if (base.applies) {
                        applyLayout(*entry, base.box, stack, base.layout);
                        applied = true;
                    }
                }
            }
            if (auto nested = std::dynamic_pointer_cast<View>(child)) pending.push_back(std::move(nested));
        }
    }
    if (applied) for (auto& query : queries_) query.second.version = ~size_t{};
}

inline void StyleRegistry::applyBaseLayouts(bool force) {
    const oneui::detail::view_mount_timing::Scope measure(oneui::detail::view_mount_timing::BaseLayout);
    pruneEntries();
    const auto revision = baseLayoutRevision_;
    std::unordered_map<const Widget*,std::shared_ptr<Stack>> parents;
    parents.reserve(entries_.size());
    for (const auto& entry : entries_)
        if (auto stack = std::dynamic_pointer_cast<Stack>(entry.widget.lock()))
            for (const auto& child : stack->children()) parents[child.get()] = stack;

    // Ordinary data changes leave the current container-query overrides in
    // place. Restoring base style here would reapply the same query in paint.
    if (!force && appliedBaseLayoutRevision_ == revision && baseLayoutParents_.size() == parents.size() &&
        std::all_of(parents.begin(), parents.end(), [&](const auto& item) {
            const auto old = baseLayoutParents_.find(item.first);
            return old != baseLayoutParents_.end() && old->second.lock() == item.second;
        })) return;

    // Repeated rows have many controls with identical unconditional styles.
    // Keep this cache within the pass: both the sheet and the entry strings
    // already belong to the registry, and layout application stays per child.
    std::map<std::pair<std::string_view, std::string_view>, BaseLayoutStyle> resolved;
    bool applied = false;
    for (const auto& entry : entries_) if (auto widget = entry.widget.lock()) {
        auto parent = parents.find(widget.get());
        if (parent == parents.end()) continue;
        auto [style, fresh] = resolved.try_emplace({entry.component, entry.classes});
        auto& base = style->second;
        if (fresh) base = resolveBaseLayout(entry);
        if (base.applies) {
            applyLayout(entry, base.box, parent->second, base.layout);
            applied = true;
        }
    }
    // Base dimensions can replace an active query override without changing
    // the measured size or matched tier. Reapply it on the next layout pass.
    if(applied)for(auto& entry:queries_)entry.second.version=~size_t{};
    baseLayoutParents_.clear();
    baseLayoutParents_.reserve(parents.size());
    for (const auto& item : parents) baseLayoutParents_.emplace(item.first, item.second);
    appliedBaseLayoutRevision_ = revision;
}
inline void StyleRegistry::restoreQueryLayouts() {
    for (const auto& entry : entries_) if (entry.layoutDefaults && !entry.widget.expired()) {
        StyleNode node{syntax::components().at(entry.component),{"one-"+entry.component},0};
        oneui::detail::appendStyleClasses(node.classes, entry.classes);
        applyLayout(entry,sheet_.resolve(node),entry.layoutDefaults->parent.lock(),sheet_.resolveLayout(node));
    }
}
inline void StyleRegistry::configureQuery(const std::shared_ptr<Widget>& widget,const StyleBox& box) {
    auto stack=std::dynamic_pointer_cast<Stack>(widget);
    if(!box.containerName&&!box.sizeContainment) {
        if(stack){stack->setSizeContainment(false);stack->setContainerQuery({});}
        return;
    }
    if(!stack||!box.containerName||box.containerName->empty()||!box.sizeContainment.value_or(false))
        throw std::invalid_argument("CONTAINER_SIZE: a named Stack requires container-type:size");
    stack->setSizeContainment(true);
    queries_[widget.get()].version=~size_t{};
    std::weak_ptr<Stack> weak=stack;std::weak_ptr<int> life=queryLifetime_;
    stack->setContainerQuery([this,weak,life]{if(life.expired())return false;auto w=weak.lock();return w&&evaluateQuery(w);});
}
inline void StyleRegistry::applyLayout(const Entry& entry,const StyleBox& box,const std::shared_ptr<Stack>& parent,const StyleLayout& layout) {
    auto widget=entry.widget.lock();if(!widget)return;
    if (!entry.layoutDefaults) {
        auto& defaults = entry.layoutDefaults.emplace();
        hasCapturedLayoutDefaults_ = true;
        defaults.size = widget->preferredSize();
        defaults.parent = parent;
        if(parent)defaults.flex=parent->explicitFlex(widget);
        if(auto stack=std::dynamic_pointer_cast<Stack>(widget)){defaults.direction=stack->direction();defaults.wrap=stack->wrap();}
    }
    const auto defaults = *entry.layoutDefaults;
    widget->setPreferredSize({box.width.value_or(defaults.size.width),box.height.value_or(defaults.size.height)});
    if(auto page=std::dynamic_pointer_cast<ContainerPage>(widget))page->setDisplay(box.display);
    else widget->setLayoutVisible(box.display.value_or(true));
    if(auto stack=std::dynamic_pointer_cast<Stack>(widget)) {
        stack->setDirection(box.flexDirection?(*box.flexDirection=="row"?StackDirection::Row:StackDirection::Column):defaults.direction);
        if(!stack->setWrap(box.flexWrap.value_or(defaults.wrap)))throw std::invalid_argument("CONTAINER_WRAP: Yoga required");
    } else if(box.flexDirection||box.flexWrap||box.containerName)
        throw std::invalid_argument("CONTAINER_CONSUMPTION: flex direction/wrap/container requires Stack");
    if(parent) {
        auto flex=defaults.flex.value_or(StackFlex{});
        if(box.flexBasis)flex.basis=box.flexBasis;
        if(layout.flexBasisPercent)flex.basis.reset();
        if(box.flexGrow)flex.grow=*box.flexGrow;
        if(box.flexShrink)flex.shrink=*box.flexShrink;
        flex.width=box.width;flex.height=box.height;
        flex.minWidth=box.minWidth.value_or(0);flex.minHeight=box.minHeight.value_or(0);
        // max-width/max-height clamp the parent main axis (StackFlex::max); the
        // cross axis keeps the authored or unbounded default.
        const bool row=parent->direction()==StackDirection::Row;
        if(layout.maxWidth||layout.maxHeight) {
            const auto main=row?layout.maxWidth:layout.maxHeight;
            if(!main)throw std::invalid_argument(row
                ?"CONTAINER_CONSUMPTION: max-height requires a column parent"
                :"CONTAINER_CONSUMPTION: max-width requires a row parent");
            flex.max=*main;
        }
        if(defaults.flex||box.flexBasis||layout.flexBasisPercent||layout.flexOrder||box.flexGrow||box.flexShrink||box.width||box.height||box.minWidth||box.minHeight||layout.maxWidth||layout.maxHeight)parent->setFlex(widget,flex);
        else parent->clearFlex(widget);
        parent->setFlexPresentation(widget,{layout.flexBasisPercent,layout.flexOrder.value_or(0)});
    }
}
inline bool StyleRegistry::evaluateQuery(const std::shared_ptr<Stack>& container) {
    // Invalid query dependencies are recoverable layout diagnostics, not
    // exceptions that may unwind the native window event loop on Cocoa.
    const auto reject=[&](const std::string& message) {
        container->setSizeContainment(false);
        if(queryDiagnostics_.empty()||queryDiagnostics_.back()!=message) {
            if(queryDiagnostics_.size()==32)queryDiagnostics_.erase(queryDiagnostics_.begin());
            queryDiagnostics_.push_back(message);
        }
        return false;
    };
    try {
        auto& state=queries_[container.get()];auto frame=container->frame();
        if(state.version==sheet_.version()&&state.treeVersion==treeVersion_&&
           state.measured.width==frame.width&&state.measured.height==frame.height)return false;
        auto nodeFor=[](const Entry& entry) {
            StyleNode node{syntax::components().at(entry.component),{"one-"+entry.component},0};
            oneui::detail::appendStyleClasses(node.classes, entry.classes);
            return node;
        };
        const auto* containerEntry = indexedEntry(container);
        if (!containerEntry) return reject("CONTAINER_SIZE: container is no longer registered");
        const auto containerNode = nodeFor(*containerEntry);
        auto base=sheet_.resolve(containerNode);
        for(const auto& rule:sheet_.rules())if(rule.query&&rule.query->name==base.containerName &&
            selectorMatches(rule.selector,containerNode))
            return reject("CONTAINER_DEPENDENCY: a container cannot query its own size");

        // Style registrations already have a retained index. Read live parent
        // relationships separately so reparenting still applies immediately.
        std::unordered_map<const Widget*, std::shared_ptr<View>> parents;
        parents.reserve(entries_.size());
        for (const auto& entry : entries_)
            if (auto view = std::dynamic_pointer_cast<View>(entry.widget.lock()))
                for (const auto& child : view->children()) parents[child.get()] = view;
        const auto parentOf = [&parents](const Widget* widget) {
            const auto found = parents.find(widget);
            return found == parents.end() ? nullptr : found->second;
        };
        auto parent=std::dynamic_pointer_cast<Stack>(parentOf(container.get()));
        if(parent) {
            auto flex=parent->explicitFlex(container);bool column=parent->direction()==StackDirection::Column;
            bool main=bool(column?base.height:base.width)||(flex&&(flex->basis.has_value()||flex->grow>0));
            bool cross=parent->align()==StackAlign::Stretch||bool(column?base.width:base.height);
            if(!main||!cross)return reject("CONTAINER_SIZE: parent must allocate both axes independently");
        }
        std::map<std::string,Size> sizes;
        std::vector<std::shared_ptr<Widget>> ancestors;
        for (std::shared_ptr<Widget> widget = container; widget; widget = parentOf(widget.get()))
            ancestors.push_back(widget);
        for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
            if (const auto* entry = indexedEntry(*it)) {
                const auto box = it->get() == container.get() ? base : sheet_.resolve(nodeFor(*entry));
                if (box.containerName) {
                    const auto bounds = (*it)->frame();
                    const auto border = std::max(0.f, box.borderWidth.value_or(0));
                    sizes[*box.containerName] = {std::max(0.f, bounds.width-2*border),
                                                 std::max(0.f, bounds.height-2*border)};
                }
            }
        }
        std::vector<bool> matches;
        matches.reserve(state.matches.size());
        const ContainerQuery* previous = nullptr;
        bool matched = false;
        for (const auto& rule : sheet_.rules()) if (const auto* query = rule.query.get()) {
            if (query != previous) {
                previous = query;
                const auto size = sizes.find(query->name);
                matched = size != sizes.end() && query->matches(size->second);
            }
            matches.push_back(matched);
        }
        state.measured={frame.width,frame.height};
        if(state.version==sheet_.version()&&state.treeVersion==treeVersion_&&matches==state.matches)return false;
        if (!state.effective || state.effectiveVersion != sheet_.version() || state.effectiveMatches != matches) {
            state.effective = sheet_.forContainers(sizes);
            state.effectiveVersion = sheet_.version();
            state.effectiveMatches = matches;
        }
        const auto& effective = *state.effective;
        struct Pending {const Entry* entry;StyleBox box;std::shared_ptr<Stack> parent;StyleLayout layout;};
        std::vector<Pending> pending;
        std::vector<std::shared_ptr<Widget>> work(container->children().begin(),container->children().end());
        while(!work.empty()) {
            auto widget=work.back();work.pop_back();bool nested=false;
            if (const auto* entry = indexedEntry(widget)) {
                auto node=nodeFor(*entry);auto box=effective.resolve(node);auto unconditional=sheet_.resolve(node);
                nested=bool(unconditional.containerName);
                if(nested&&!(unconditional.width&&unconditional.height))
                    return reject("CONTAINER_DEPENDENCY: nested query containers require fixed independent width and height");
                if(nested)for(const auto& rule:sheet_.rules())if(rule.query&&selectorMatches(rule.selector,node)) {
                    const auto& b=rule.box;
                    if(b.width||b.height||b.minWidth||b.minHeight||b.flexBasis||b.flexGrow||b.flexShrink||b.padding||b.display||
                       std::any_of(rule.declarations.begin(),rule.declarations.end(),[](const auto& declaration){return declaration.first=="flex-basis"||declaration.first=="order"||declaration.first=="max-width"||declaration.first=="max-height";}))
                        return reject("CONTAINER_DEPENDENCY: ancestor query changes a measured container");
                }
                if((box.flexDirection||box.flexWrap)&&!std::dynamic_pointer_cast<Stack>(widget))
                    return reject("CONTAINER_CONSUMPTION: layout output requires Stack");
                pending.push_back({entry,std::move(box),std::dynamic_pointer_cast<Stack>(parentOf(widget.get())),effective.resolveLayout(node)});
            }
            if(!nested)if(auto view=std::dynamic_pointer_cast<View>(widget))for(auto& child:view->children())work.push_back(child);
        }
        // All conditions use the same immutable measurement snapshot. No
        // condition is evaluated again after this batch changes child layout.
        auto previousFocus=container->activeFocusChild();
        while(previousFocus) {auto next=previousFocus->activeFocusChild();if(!next)break;previousFocus=std::move(next);}
        for(auto& change:pending) {
            applyLayout(*change.entry,change.box,change.parent,change.layout);
            apply(*change.entry,effective);
        }
        if(previousFocus&&!previousFocus->focused())container->focusFirstLeaf();
        state.matches=std::move(matches);state.version=sheet_.version();state.treeVersion=treeVersion_;
        container->setSizeContainment(true);
        return !pending.empty();
    }catch(const std::exception& error) {
        return reject(error.what());
    }
}
} // namespace oneui::ui
