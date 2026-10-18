#include "internal/view_profile.h"
#include "runtime.h"
namespace oneui::views {
void Runtime::observe(const std::shared_ptr<Scope> &scope, uint32_t id) {
    auto node = observing.lock();
    if (!node)
        return;
    auto &observers = scope->observers[id];
    observers.erase(std::remove_if(observers.begin(), observers.end(),
                                   [](auto &weak) { return weak.expired(); }),
                    observers.end());
    if (std::none_of(observers.begin(), observers.end(),
                     [&](auto &weak) { return weak.lock() == node; }))
        observers.push_back(node);
}
void Runtime::mark(const std::shared_ptr<Scope> &scope, uint32_t id) {
    ++scope->versions[id];
    auto found = scope->observers.find(id);
    if (found == scope->observers.end())
        return;
    for (auto &weak : found->second) {
        if (auto node=weak.lock(); node && (node->tableCache || node->richTableCache))
            node->recordDirty.insert(scope.get());
        for (auto node = weak.lock(); node; node = node->parent.lock())
            node->dirty = true;
    }
}
void Runtime::enqueue() {
    if (building || closing || scheduled)
        return;
    scheduled = true;
    if (schedule)
        schedule();
}
void Runtime::notify(const std::shared_ptr<Scope> &scope, uint32_t property) {
    onThread();
    alive();
    check(scope && !scope->closed, ONEUI_VIEW_CLOSED, "Scope closed");
    if (callbackPhase == Phase::Read || callbackPhase == Phase::Cleanup) {
        fail(ONEUI_VIEW_REENTRANT, "Notification during read/cleanup");
        throw Failure(ONEUI_VIEW_REENTRANT, "Notification during read/cleanup");
    }
    const auto &model = image.schema.model(scope->table.model_id);
    if (property) {
        try {
            model.property(property);
        } catch (...) {
            throw Failure(ONEUI_VIEW_UNKNOWN_ID, "Unknown property");
        }
    }
    for (const auto &entry : model.properties) {
        const auto &p = entry.second;
        if (property && p.id != property)
            continue;
        scope->dirty.insert(p.id);
        mark(scope, p.id);
        if (p.tag == 5) {
            auto &list = scope->lists[p.id];
            list.reset = true;
            list.pending.clear();
            list.queuedBytes = 0;
        }
    }
    enqueue();
}
void Runtime::notifyList(const std::shared_ptr<Scope> &scope, uint32_t property,
                         const OneUiViewListChange &change) {
    onThread();
    alive();
    check(scope && !scope->closed, ONEUI_VIEW_CLOSED, "Scope closed");
    if (callbackPhase == Phase::Read || callbackPhase == Phase::Cleanup) {
        fail(ONEUI_VIEW_REENTRANT, "List notification during read/cleanup");
        throw Failure(ONEUI_VIEW_REENTRANT, "List notification during read/cleanup");
    }
    const oneview::Property *p = nullptr;
    try {
        p = &image.schema.model(scope->table.model_id).property(property);
    } catch (...) {
        throw Failure(ONEUI_VIEW_UNKNOWN_ID, "Unknown list property");
    }
    check(p->tag == 5, ONEUI_VIEW_TYPE, "Property is not a list");
    check(change.struct_size == sizeof(change) && change.kind <= ONEUI_LIST_REORDER,
          ONEUI_VIEW_INVALID_ARGUMENT, "List change size/kind");
    check(change.after_revision > change.before_revision, ONEUI_VIEW_STALE_CHANGE,
          "List revision must increase");
    check(change.key_count <= 100000 && change.property_count <= 100000 &&
              (change.keys || !change.key_count) && (change.properties || !change.property_count),
          ONEUI_VIEW_INVALID_ARGUMENT, "List arrays");
    if (change.kind == ONEUI_LIST_RESET)
        check(!change.index && !change.count && !change.to_index && !change.key_count &&
                  !change.property_count,
              ONEUI_VIEW_INVALID_ARGUMENT, "RESET fields");
    else {
        check(change.count && change.key_count == change.count, ONEUI_VIEW_INVALID_ARGUMENT,
              "Delta keys/count");
        check(change.kind == ONEUI_LIST_MOVE || !change.to_index, ONEUI_VIEW_INVALID_ARGUMENT,
              "Delta destination");
        check(change.kind == ONEUI_LIST_UPDATE || !change.property_count,
              ONEUI_VIEW_INVALID_ARGUMENT, "Delta fields only for UPDATE");
        check(change.kind != ONEUI_LIST_REORDER || !change.index,
              ONEUI_VIEW_INVALID_ARGUMENT, "REORDER covers the complete list");
    }
    Delta delta{change.kind,  change.before_revision, change.after_revision,
                change.index, change.count,           change.to_index};
    size_t bytes = sizeof(Delta);
    std::set<std::string> unique;
    for (size_t i = 0; i < change.key_count; ++i) {
        OneUiValue value{};
        value.tag = 4;
        value.as.utf8 = {change.keys[i].data, change.keys[i].length};
        auto key = Value::copy(value).text;
        check(!key.empty() && unique.insert(key).second, ONEUI_VIEW_BAD_KEY,
              "Duplicate/empty delta key");
        bytes += key.size();
        if (bytes > 1024 * 1024) {
            auto &state = scope->lists[property];
            state.reset = true;
            state.pending.clear();
            state.queuedBytes = 0;
            scope->dirty.insert(property);
            mark(scope, property);
            enqueue();
            return;
        }
        delta.keys.push_back(std::move(key));
    }
    for (size_t i = 0; i < change.property_count; ++i) {
        try {
            image.schema.model(p->item).property(change.properties[i]);
        } catch (...) {
            throw Failure(ONEUI_VIEW_UNKNOWN_ID, "Unknown item field");
        }
        delta.properties.push_back(change.properties[i]);
        bytes += 4;
    }
    auto &state = scope->lists[property];
    if (delta.kind == ONEUI_LIST_RESET || state.pending.size() >= 4096 ||
        bytes + state.queuedBytes > 1024 * 1024) {
        state.reset = true;
        state.pending.clear();
        state.queuedBytes = 0;
    } else if (!state.reset) {
        state.pending.push_back(std::move(delta));
        state.queuedBytes += bytes;
    }
    scope->dirty.insert(property);
    mark(scope, property);
    enqueue();
}
void Runtime::flush() {
    ONEUI_VIEW_TIME(Flush);
    for(unsigned pass=0;pass<32;++pass) {
        flushPass();
        auto writes=std::move(modelWrites);modelWrites.clear();
        bool changed=false;
        for(auto &write:writes)if(auto scope=write.scope.lock()) {
            if(!scope->closed && read(scope,write.property)==write.expected) {
                set(scope,write.property,write.replacement);changed=true;
            }
        }
        if(!changed)return;
    }
    throw Failure(ONEUI_VIEW_LIMIT,"Selection normalization failed to converge");
}
void Runtime::flushPass() {
    onThread();
    alive();
    check(!depth && !flushing && callbackPhase == Phase::Idle, ONEUI_VIEW_REENTRANT,
          "Flush during callback/transaction");
    flushing = true;
    scheduled = false;
    auto sequence = error.sequence;
    auto previousScopes = scopes.size();
    bool committed = false;
    staging = true;
    try {
        {
            Callback transaction(*this, Phase::Read);
            update(root);
            prepareWorkspaceIndex();
            alive();
            for (auto &commit : commits)
                commit();
            commits.clear();
            rollbacks.clear();
            committed = true;
            for (auto &scope : retired)
                closeScope(scope);
            retired.clear();
        }
        staging = false;
        styles->applyBaseLayouts();
        scopes.erase(
            std::remove_if(scopes.begin(), scopes.end(), [](const auto &s) { return s->closed; }),
            scopes.end());
        factory.collectUnusedOwned();
        flushing = false;
        alive();
        if (error.sequence != sequence)
            throw Failure(error.result, "Reentrant callback rejected during flush");
    } catch (...) {
        modelWrites.clear();
        commits.clear();
        for (auto it = rollbacks.rbegin(); it != rollbacks.rend(); ++it)
            (*it)();
        rollbacks.clear();
        retired.clear();
        if (!committed)
            for (size_t i = previousScopes; i < scopes.size(); ++i)
                closeScope(scopes[i]);
        staging = false;
        if (root) {
            root->dirty = true;
            for (auto &scope : scopes)
                for (auto &property : scope->observers)
                    mark(scope, property.first);
        }
        flushing = false;
        if (closing)
            finishClose();
        throw;
    }
}
void Runtime::set(const std::shared_ptr<Scope> &scope, uint32_t property, const Value &value) {
    alive();
    check(scope && !scope->closed, ONEUI_VIEW_CLOSED, "Set scope closed");
    check(callbackPhase != Phase::Read && !scope->mutating, ONEUI_VIEW_REENTRANT,
          "Nested mutable callback");
    const auto &p = image.schema.model(scope->table.model_id).property(property);
    check(p.access == 3 && p.tag == value.tag, ONEUI_VIEW_TYPE, "Set type/access");
    scope->mutating = true;
    int result = ONEUI_VIEW_FOREIGN_FAILURE;
    try {
        Callback guard(*this, Phase::Mutate);
        auto borrowed = value.borrow();
        result = scope->table.set(scope->vm, property, &borrowed);
    } catch (...) {
        scope->mutating = false;
        throw;
    }
    scope->mutating = false;
    alive();
    check(result == 0, result < 0 ? result : ONEUI_VIEW_FOREIGN_FAILURE, "VM set failed");
    notify(scope, property);
}
void Runtime::invoke(const std::shared_ptr<Instance> &instance, const oneview::Record &event,
                     std::vector<Value> args) {
    alive();
    auto scope = instance->scopes.at(event[3] & 0xffff);
    check(!scope->closed, ONEUI_VIEW_CLOSED, "Event scope closed");
    check(callbackPhase != Phase::Read && !scope->mutating, ONEUI_VIEW_REENTRANT,
          "Nested mutable event");
    const auto &cmd = image.schema.model(event[4]).command(event[2]);
    if ((cmd.when && !read(scope, cmd.when).integer) || (cmd.busy && read(scope, cmd.busy).integer))
        return;
    std::vector<OneUiValue> borrowed;
    check(args.size() == cmd.args.size(), ONEUI_VIEW_TYPE, "Event argument count");
    for (size_t i = 0; i < args.size(); ++i) {
        check(args[i].tag == cmd.args[i].tag, ONEUI_VIEW_TYPE, "Event argument type");
        borrowed.push_back(args[i].borrow());
    }
    scope->mutating = true;
    int result = ONEUI_VIEW_FOREIGN_FAILURE;
    try {
        Callback guard(*this, Phase::Mutate);
        result = scope->table.invoke(scope->vm, cmd.id, borrowed.data(), borrowed.size());
    } catch (...) {
        scope->mutating = false;
        throw;
    }
    scope->mutating = false;
    if (!closing)
        check(result == 0, result < 0 ? result : ONEUI_VIEW_FOREIGN_FAILURE, "VM invoke failed");
}
} // namespace oneui::views
