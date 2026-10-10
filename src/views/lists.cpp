#include "runtime.h"
namespace oneui::views {
namespace {
void applyDelta(std::vector<std::string> &keys, const Delta &d) {
    check(d.index <= keys.size(), ONEUI_VIEW_STALE_CHANGE, "Delta index");
    if (d.kind == ONEUI_LIST_INSERT) {
        for (const auto &key : d.keys)
            check(std::find(keys.begin(), keys.end(), key) == keys.end(), ONEUI_VIEW_BAD_KEY,
                  "Inserted key exists");
        keys.insert(keys.begin() + size_t(d.index), d.keys.begin(), d.keys.end());
        return;
    }
    check(d.count <= keys.size() - size_t(d.index), ONEUI_VIEW_STALE_CHANGE, "Delta range");
    check(std::equal(d.keys.begin(), d.keys.end(), keys.begin() + size_t(d.index)),
          ONEUI_VIEW_STALE_CHANGE, "Delta key range mismatch");
    if (d.kind == ONEUI_LIST_UPDATE)
        return;
    keys.erase(keys.begin() + size_t(d.index), keys.begin() + size_t(d.index + d.count));
    if (d.kind == ONEUI_LIST_MOVE) {
        check(d.to <= keys.size(), ONEUI_VIEW_STALE_CHANGE, "MOVE destination");
        keys.insert(keys.begin() + size_t(d.to), d.keys.begin(), d.keys.end());
    }
}
} // namespace
void Runtime::auditListIdentity(const std::shared_ptr<Scope> &scope, uint32_t property,
                                const ListState &state, const std::vector<std::string> &keys) {
    std::map<std::string, std::shared_ptr<Scope>> old;
    for (size_t i = 0; i < state.keys.size(); ++i)
        old.emplace(state.keys[i], state.items[i]);
    for (size_t i = 0; i < keys.size(); ++i) {
        OneUiValue borrowed{};
        Value key;
        int result;
        {
            Callback guard(*this, Phase::Read);
            result = scope->table.list_key(scope->vm, property, i, &borrowed);
            if (!result)
                key = Value::copy(borrowed);
        }
        alive();
        check(result == 0 && key.tag == 4 && key.text == keys[i], ONEUI_VIEW_STALE_CHANGE,
              "Debug list key differs from delta snapshot");
        auto found = old.find(keys[i]);
        if (found == old.end())
            continue;
        OneUiViewModelRef ref{};
        {
            Callback guard(*this, Phase::Read);
            result = scope->table.list_acquire(scope->vm, property, i, &ref);
        }
        const bool equal = ref.vm == found->second->vm && ref.vtable == found->second->identity;
        if (!result && ref.vm && ref.vtable && ref.vtable->release) {
            Callback guard(*this, Phase::Cleanup);
            ref.vtable->release(ref.vm);
        }
        alive();
        check(result == 0 && equal, ONEUI_VIEW_STALE_CHANGE,
              "Debug precise delta changed VM identity; RESET required");
    }
}
ListState &Runtime::list(const std::shared_ptr<Scope> &scope, uint32_t property) {
    observe(scope, property);
    auto &state = scope->lists[property];
    if (!state.initialized || state.reset || scope->dirty.count(property))
        refreshList(scope, property, state);
    return state;
}
void Runtime::refreshList(const std::shared_ptr<Scope> &scope, uint32_t property,
                          ListState &state) {
    alive();
    const auto &p = image.schema.model(scope->table.model_id).property(property);
    OneUiViewListInfo info{};
    int result;
    {
        Callback guard(*this, Phase::Read);
        result = scope->table.list_info(scope->vm, property, &info);
    }
    alive();
    check(result == 0, ONEUI_VIEW_FOREIGN_FAILURE, "list_info failed");
    check(info.length <= 100000, ONEUI_VIEW_LIMIT, "List item limit");
    check(!state.initialized || info.revision >= state.revision, ONEUI_VIEW_STALE_CHANGE,
          "List revision decreased");
    auto keys = state.keys;
    bool reset = state.reset || !state.initialized;
    if (!reset) {
        auto revision = state.revision;
        try {
            for (const auto &d : state.pending) {
                check(d.before == revision, ONEUI_VIEW_STALE_CHANGE, "Delta revision gap");
                applyDelta(keys, d);
                revision = d.after;
            }
            check(revision == info.revision && keys.size() == info.length, ONEUI_VIEW_STALE_CHANGE,
                  "Delta final snapshot mismatch");
#ifndef NDEBUG
            auditListIdentity(scope, property, state, keys);
#endif
        } catch (const Failure &e) {
            fail(e.result, e.what());
            reset = true;
        }
    }
    if (reset) {
        keys.clear();
        std::set<std::string> unique;
        for (uint64_t i = 0; i < info.length; ++i) {
            OneUiValue borrowed{};
            Value key;
            {
                Callback guard(*this, Phase::Read);
                result = scope->table.list_key(scope->vm, property, i, &borrowed);
                if (!result)
                    key = Value::copy(borrowed);
            }
            alive();
            check(result == 0, ONEUI_VIEW_FOREIGN_FAILURE, "list_key failed");
            check(key.tag == 4 && !key.text.empty() && unique.insert(key.text).second,
                  ONEUI_VIEW_BAD_KEY, "Invalid/duplicate stable key");
            keys.push_back(std::move(key.text));
        }
    }
    std::map<std::string, std::shared_ptr<Scope>> old;
    for (size_t i = 0; i < state.keys.size(); ++i)
        old.emplace(state.keys[i], state.items[i]);
    std::vector<std::shared_ptr<Scope>> next, created;
    try {
        for (size_t i = 0; i < keys.size(); ++i) {
            auto found = old.find(keys[i]);
            if (!reset && found != old.end()) {
                next.push_back(found->second);
                continue;
            }
            OneUiViewModelRef ref{};
            {
                Callback guard(*this, Phase::Read);
                result = scope->table.list_acquire(scope->vm, property, i, &ref);
            }
            if (closing && result == 0 && ref.vm && ref.vtable && ref.vtable->release) {
                Callback guard(*this, Phase::Cleanup);
                ref.vtable->release(ref.vm);
            }
            alive();
            check(result == 0, ONEUI_VIEW_FOREIGN_FAILURE, "list_acquire failed");
            bool adopted = false;
            try {
                checkTable(ref.vtable, p.item);
                if (found != old.end() && found->second->vm == ref.vm &&
                    found->second->identity == ref.vtable)
                    next.push_back(found->second);
                else {
                    auto child = acquire(ref, p.item, true);
                    adopted = true;
                    created.push_back(child);
                    const auto &model = image.schema.model(p.item);
                    check(read(child, model.key).text == keys[i], ONEUI_VIEW_BAD_KEY,
                          "Item schema key differs from list key");
                    connect(child);
                    next.push_back(std::move(child));
                }
            } catch (...) {
                if (!adopted && ref.vm && ref.vtable && ref.vtable->release) {
                    Callback guard(*this, Phase::Cleanup);
                    ref.vtable->release(ref.vm);
                }
                throw;
            }
            if (!adopted) {
                Callback guard(*this, Phase::Cleanup);
                ref.vtable->release(ref.vm);
            }
        }
    } catch (...) {
        for (auto &child : created)
            closeScope(child);
        throw;
    }
    if (reset) {
        for (const auto &child : next) {
            for (const auto &entry : image.schema.model(child->table.model_id).properties) {
                child->dirty.insert(entry.second.id);
                mark(child, entry.second.id);
                if (entry.second.tag == 5)
                    child->lists[entry.second.id].reset = true;
            }
        }
    }
    for (const auto &d : state.pending)
        if (d.kind == ONEUI_LIST_UPDATE) {
            for (const auto &key : d.keys) {
                auto at = std::find(keys.begin(), keys.end(), key);
                if (at == keys.end())
                    continue;
                auto child = next[size_t(at - keys.begin())];
                const auto &model = image.schema.model(child->table.model_id);
                for (const auto &entry : model.properties)
                    if (d.properties.empty() || std::find(d.properties.begin(), d.properties.end(),
                                                          entry.second.id) != d.properties.end()) {
                        child->dirty.insert(entry.second.id);
                        mark(child, entry.second.id);
                        if (entry.second.tag == 5)
                            child->lists[entry.second.id].reset = true;
                    }
            }
        }
    // Cache publication is staged too: tree construction can still fail below.
    if (staging) {
        auto previous = state;
        previous.reset = true;
        previous.pending.clear();
        rollbacks.push_back([scope, property, previous = std::move(previous)]() mutable {
            scope->lists[property] = std::move(previous);
        });
    }
    // RESET with identical key but changed VM is a replacement, not BAD_KEY.
    for (auto &oldItem : state.items)
        if (std::find(next.begin(), next.end(), oldItem) == next.end())
            if (staging)
                retired.push_back(oldItem);
            else
                closeScope(oldItem);
    state.keys = std::move(keys);
    state.items = std::move(next);
    state.revision = info.revision;
    state.initialized = true;
    state.reset = false;
    state.pending.clear();
    state.queuedBytes = 0;
    scope->dirty.erase(property);
}
} // namespace oneui::views
