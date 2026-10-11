#include "runtime.h"
#include "internal/view_profile.h"
#include "internal/unicode.h"
namespace oneui::views {
struct Instance::TableCache {
    struct Row {
        ui::TableRow value;
        std::shared_ptr<Scope> scope;
        std::vector<std::shared_ptr<Scope>> cells;
        std::vector<uint64_t> versions;
    };
    const ListState* source = nullptr;
    uint64_t serial = 0;
    std::unordered_map<std::string, Row> rows;
    std::unordered_map<const Scope*, std::string> owners;
};
void Runtime::applyTableRows(Instance& instance,ListState& state) {
    ONEUI_VIEW_TIME(Rows);
    using Cache=Instance::TableCache;
    auto cache=instance.tableCache;
    bool reset=!cache || cache->source!=&state ||
        (cache->serial!=state.serial && (state.lastReset || cache->serial+1!=state.serial ||
         std::any_of(state.applied.begin(),state.applied.end(),[](const Delta& d) {
             return d.kind==ONEUI_LIST_REORDER;
         })));
    if (!cache) cache=std::make_shared<Cache>();
    const bool changed=reset || cache->serial!=state.serial;
    std::set<std::string> dirty;
    if (reset) dirty.insert(state.keys.begin(),state.keys.end());
    else {
        if (changed) for(const auto& d:state.applied)
            if(d.kind==ONEUI_LIST_INSERT || d.kind==ONEUI_LIST_UPDATE)
                dirty.insert(d.keys.begin(),d.keys.end());
        for(auto scope:instance.recordDirty) {
            auto found=cache->owners.find(scope);
            if(found!=cache->owners.end()) dirty.insert(found->second);
        }
    }
    if (!changed && dirty.empty()) return;
    // Coalesced insert/remove can have no final VM to acquire. Reconcile its
    // final snapshot instead of manufacturing an intermediate foreign record.
    if(changed && !reset) for(const auto& d:state.applied)
        if(d.kind==ONEUI_LIST_INSERT) for(const auto& key:d.keys)
            if(std::find(state.keys.begin(),state.keys.end(),key)==state.keys.end()) reset=true;
    std::unordered_map<std::string,Cache::Row> prepared;
    for(const auto& key:dirty) {
        auto at=std::find(state.keys.begin(),state.keys.end(),key);
        if(at==state.keys.end()) continue;
        auto scope=state.items[size_t(at-state.keys.begin())];
        auto old=cache->rows.find(key);
        const bool same=old!=cache->rows.end() && old->second.scope==scope;
        Cache::Row row;
        row.scope=scope;
        if(same) row.value.id=old->second.value.id;
        else { ONEUI_VIEW_TIME(Conversion); row.value.id=unicode::fromUtf8(key); }
        const auto field=image.schema.model(scope->table.model_id).properties.at("cells").id;
        auto& cells=list(scope,field);
        row.cells=cells.items;
        for(size_t i=0;i<cells.items.size();++i) {
            const auto& cell=cells.items[i];
            const auto value=image.schema.model(cell->table.model_id).properties.at("value").id;
            row.versions.push_back(cell->versions[value]);
            if(same && i<old->second.cells.size() && old->second.cells[i]==cell &&
               old->second.versions[i]==row.versions.back()) {
                row.value.cells.push_back(old->second.value.cells[i]);
                observe(cell,value);
            } else {
                auto text=read(cell,value).text;
                ONEUI_VIEW_TIME(Conversion);
                row.value.cells.push_back(unicode::fromUtf8(text));
            }
        }
        prepared.emplace(key,std::move(row));
    }
    // All VM reads/conversions precede native writes. Cache publication travels
    // with the native commit, so a later failed binding leaves the old cache.
    auto delta=changed ? state.applied : std::vector<Delta>{};
    auto keys=state.keys;
    auto serial=state.serial;
    auto table=instance.element.table;
    auto commit=[cache,table,prepared=std::move(prepared),delta=std::move(delta),
                 keys=std::move(keys),serial,source=&state,reset,&instance]() mutable {
        ONEUI_VIEW_TIME(Keyed);
        struct Applying { Instance& node; bool previous; Applying(Instance& n):node(n),previous(n.applying){n.applying=true;}
            ~Applying(){node.applying=previous;} } applying(instance);
        auto unmap=[&](const Cache::Row& row) {
            cache->owners.erase(row.scope.get());
            for(auto& cell:row.cells) cache->owners.erase(cell.get());
        };
        for(auto& [key,row]:prepared) {
            auto old=cache->rows.find(key);
            if(old!=cache->rows.end()) unmap(old->second);
            cache->owners[row.scope.get()]=key;
            for(auto& cell:row.cells) cache->owners[cell.get()]=key;
            cache->rows[key]=std::move(row);
        }
        if(reset) {
            std::vector<ui::TableRow> rows;
            for(auto& key:keys) rows.push_back(cache->rows.at(key).value);
            table->update(rows);
            std::set<std::string> live(keys.begin(),keys.end());
            for(auto it=cache->rows.begin();it!=cache->rows.end();)
                if(!live.count(it->first)) {unmap(it->second);it=cache->rows.erase(it);} else ++it;
        } else {
            for(const auto& d:delta) {
                if(d.kind==ONEUI_LIST_INSERT) {
                    std::vector<ui::TableRow> rows;
                    for(auto& key:d.keys) rows.push_back(cache->rows.at(key).value);
                    table->insert(size_t(d.index),rows);
                } else if(d.kind==ONEUI_LIST_REMOVE) {
                    table->erase(size_t(d.index),size_t(d.count));
                    for(auto& key:d.keys) if(std::find(keys.begin(),keys.end(),key)==keys.end()) {
                        auto found=cache->rows.find(key);
                        if(found!=cache->rows.end()) {unmap(found->second);cache->rows.erase(found);}
                    }
                } else if(d.kind==ONEUI_LIST_MOVE)
                    table->move(size_t(d.index),size_t(d.count),size_t(d.to));
            }
            for(const auto& [key,_]:prepared) {
                auto at=std::find(keys.begin(),keys.end(),key);
                if(at!=keys.end()) table->updateRow(size_t(at-keys.begin()),cache->rows.at(key).value);
            }
        }
        cache->serial=serial; cache->source=source;
        instance.tableCache=cache; instance.recordDirty.clear();
    };
    if(staging && instance.published) commits.push_back(std::move(commit));
    else commit();
}
} // namespace oneui::views
