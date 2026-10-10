#include "runtime.h"
#include "internal/view_profile.h"
#include <unordered_set>
namespace oneui::views {
bool Runtime::refreshDelta(const std::shared_ptr<Scope>& scope, uint32_t property,
                          ListState& state, const OneUiViewListInfo& info) {
    // Validate the complete sequence before acquiring or publishing anything.
    auto keys = state.keys;
    auto items = state.items;
    auto revision = state.revision;
    std::vector<std::shared_ptr<Scope>> removed, created;
    std::vector<std::pair<std::shared_ptr<Scope>, std::vector<uint32_t>>> dirty;
    try {
        for (const auto& d : state.pending) {
            check(d.before == revision && d.index <= keys.size(), ONEUI_VIEW_STALE_CHANGE,
                  "Delta revision/index");
            const auto index = size_t(d.index), count = size_t(d.count);
            if (d.kind == ONEUI_LIST_INSERT) {
                for (const auto& key : d.keys)
                    check(std::find(keys.begin(), keys.end(), key) == keys.end(),
                          ONEUI_VIEW_BAD_KEY, "Inserted key exists");
                keys.insert(keys.begin()+index, d.keys.begin(), d.keys.end());
                items.insert(items.begin()+index, count, nullptr);
            } else {
                check(count <= keys.size()-index &&
                      std::equal(d.keys.begin(), d.keys.end(), keys.begin()+index),
                      ONEUI_VIEW_STALE_CHANGE, "Delta range/key mismatch");
                if (d.kind == ONEUI_LIST_UPDATE) {
                    for (size_t i=index;i<index+count;++i) if (items[i])
                        dirty.emplace_back(items[i], d.properties);
                } else {
                    std::vector<std::shared_ptr<Scope>> moving(items.begin()+index, items.begin()+index+count);
                    items.erase(items.begin()+index, items.begin()+index+count);
                    keys.erase(keys.begin()+index, keys.begin()+index+count);
                    if (d.kind == ONEUI_LIST_MOVE) {
                        check(d.to <= keys.size(), ONEUI_VIEW_STALE_CHANGE, "MOVE destination");
                        keys.insert(keys.begin()+size_t(d.to), d.keys.begin(), d.keys.end());
                        items.insert(items.begin()+size_t(d.to), moving.begin(), moving.end());
                    } else for (auto& item : moving) if (item) removed.push_back(item);
                }
            }
            revision = d.after;
        }
        check(revision == info.revision && keys.size() == info.length,
              ONEUI_VIEW_STALE_CHANGE, "Delta final snapshot mismatch");
#ifndef NDEBUG
        auditListIdentity(scope, property, state, keys);
#endif
    } catch (const Failure& e) {
        fail(e.result,e.what()); state.reset = true; return false;
    }
    const auto model = image.schema.model(scope->table.model_id).property(property).item;
    try {
        for (size_t i=0;i<items.size();++i) if (!items[i]) {
            OneUiViewModelRef ref{};
            int result;
            { Callback guard(*this,Phase::Read); ONEUI_VIEW_TIME(Vm);
              result=scope->table.list_acquire(scope->vm,property,i,&ref); }
            if (result) throw Failure(ONEUI_VIEW_FOREIGN_FAILURE,"list_acquire failed");
            bool adopted=false;
            try {
                alive();
                auto child=acquire(ref,model,true); adopted=true; created.push_back(child);
                check(read(child,image.schema.model(model).key).text==keys[i],
                      ONEUI_VIEW_BAD_KEY,"Item schema key differs from list key");
                connect(child); items[i]=std::move(child);
            } catch (...) {
                if (!adopted && ref.vm && ref.vtable && ref.vtable->release) {
                    Callback guard(*this,Phase::Cleanup); ref.vtable->release(ref.vm);
                }
                throw;
            }
        }
        for (auto& [child,fields] : dirty) {
            for (const auto& [_,p] : image.schema.model(model).properties)
                if (fields.empty() || std::find(fields.begin(),fields.end(),p.id)!=fields.end()) {
                    child->dirty.insert(p.id); mark(child,p.id);
                    if (p.tag==5) child->lists[p.id].reset=true;
                }
        }
        if (staging) {
            auto previous=state; previous.reset=true; previous.pending.clear();
            rollbacks.push_back([scope,property,previous=std::move(previous)]() mutable {
                scope->lists[property]=std::move(previous);
            });
        }
    } catch (...) {
        for (auto& child : created) closeScope(child);
        throw;
    }
    for (auto& child : removed) {
        if (staging) retired.push_back(child); else closeScope(child);
    }
    state.keys=std::move(keys); state.items=std::move(items);
    state.applied=std::move(state.pending); state.pending.clear();
    state.lastReset=false; ++state.serial;
    state.revision=info.revision; state.queuedBytes=0;
    scope->dirty.erase(property);
    return true;
}
} // namespace oneui::views
