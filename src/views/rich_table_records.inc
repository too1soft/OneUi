#include "rich_table_cache.h"
#include "rich_table_filter.h"
namespace oneui::views {
void Runtime::applyRichTableRows(Instance& instance,ListState& state,
                                const std::shared_ptr<Scope>& owner,uint32_t sourceProperty) {
    ONEUI_VIEW_TIME(Conversion);
    using Cache=Instance::RichTableCache;
    auto cache=instance.richTableCache;
    const bool reset=!cache || cache->source!=&state || cache->serial!=state.serial;
    if(!cache)cache=std::make_shared<Cache>();
    std::unordered_set<std::string> dirty, dirtyCells;
    const bool reread=!cache->source || cache->source!=&state ||
        (reset && (state.lastReset || cache->serial+1!=state.serial));
    if(reread)dirty.insert(state.keys.begin(),state.keys.end());
    else if(reset) for(const auto& delta:state.applied)
        if(delta.kind==ONEUI_LIST_INSERT || delta.kind==ONEUI_LIST_UPDATE)
            dirty.insert(delta.keys.begin(),delta.keys.end());
    for(auto scope:instance.recordDirty) {
        auto found=cache->owners.find(scope);
        if(found!=cache->owners.end()) {
            dirty.insert(found->second.key);
            if(found->second.cell)dirtyCells.insert(found->second.key);
        }
    }
    auto filter=readRichTableFilter(*this,owner,sourceProperty,state);
    const bool filterChanged=filter.active!=cache->filterActive || filter.keys!=cache->filterKeys;
    if(!reset && dirty.empty() && !filterChanged)return;
    const bool stableScopes=!reread && (!reset ||
        std::all_of(state.applied.begin(),state.applied.end(),
                    [](const Delta& d){return d.kind==ONEUI_LIST_UPDATE;}));
    const bool prune=reread || (reset && std::any_of(state.applied.begin(),state.applied.end(),
                                                    [](const Delta& d){return d.kind==ONEUI_LIST_REMOVE;}));
    std::unordered_map<std::string,size_t> indices;
    if(!stableScopes)
        for(size_t i=0;i<state.keys.size();++i)indices.emplace(state.keys[i],i);
    std::vector<std::pair<std::string,Cache::Row>> prepared;
    prepared.reserve(dirty.size());
    struct Metadata { Cache::Row* row; bool selected,visible; uint64_t selectedVersion; };
    std::vector<Metadata> metadata;metadata.reserve(dirty.size());
    std::unordered_set<std::string> updatedCells;
    bool visibilityChanged=false;
    uint32_t selectedField=0,cellsField=0,visibleField=0;
    if(!state.items.empty()) {
        const auto& fields=image.schema.model(state.items.front()->table.model_id).properties;
        selectedField=fields.at("selected").id;cellsField=fields.at("cells").id;
        auto found=fields.find("visible");if(found!=fields.end())visibleField=found->second.id;
    }
    auto value=[&](const std::shared_ptr<Scope>& scope,const char* name,Value fallback) {
        const auto& fields=image.schema.model(scope->table.model_id).properties;
        const auto found=fields.find(name);
        return found==fields.end()?fallback:read(scope,found->second.id);
    };
    auto integer=[](int64_t n){Value v;v.tag=2;v.integer=n;return v;};
    auto boolean=[](bool b){Value v;v.tag=1;v.integer=b;return v;};
    auto number=[](double n){Value v;v.tag=3;v.number=n;return v;};
    auto text=[](){Value v;v.tag=4;return v;};
    auto color=[](int64_t rgba)->std::optional<Color> {
        check(rgba>=-1 && rgba<=int64_t(0xffffffffu),ONEUI_VIEW_TYPE,"Table cell color range");
        if(rgba<0)return {};
        return Color{uint8_t(rgba>>24),uint8_t(rgba>>16),uint8_t(rgba>>8),uint8_t(rgba)};
    };
    auto icon=[](const std::string& name)->std::optional<IconSymbol> {
        if(name.empty())return {};
        auto result=ui::namedIcon(name);
        check(bool(result),ONEUI_VIEW_TYPE,"Table cell icon name");return result;
    };
    for(const auto& key:dirty) {
        auto old=cache->rows.find(key);
        Cache::Row row;
        if(stableScopes) {
            if(old==cache->rows.end())continue;
            row.scope=old->second.scope;
        } else {
            auto at=indices.find(key);if(at==indices.end())continue;
            row.scope=state.items[at->second];
        }
        const bool same=old!=cache->rows.end() && old->second.scope==row.scope;
        row.selectedVersion=row.scope->versions[selectedField];
        row.selected=same && row.selectedVersion==old->second.selectedVersion
            ? old->second.selected : bool(read(row.scope,selectedField).integer);
        const auto visible=visibleField ? read(row.scope,visibleField) : boolean(true);
        check(visible.tag==1,ONEUI_VIEW_TYPE,"Table row visibility type");
        row.visible=bool(visible.integer);
        visibilityChanged|=same && old->second.visible!=row.visible;
        row.cellsVersion=row.scope->versions[cellsField];
        // Selection/filter changes do not invalidate cell values or leases.
        if(same && row.cellsVersion==old->second.cellsVersion && !dirtyCells.count(key)) {
            metadata.push_back({&old->second,row.selected,row.visible,row.selectedVersion});
            continue;
        }
        row.cells=std::make_shared<const std::vector<std::shared_ptr<Scope>>>(list(row.scope,cellsField).items);
        auto nativeCells=std::make_shared<std::vector<TableCell>>();
        for(const auto& cell:*row.cells) {
            TableCell native;
            native.text=unicode::fromUtf8(value(cell,"text",text()).text);
            native.detail=unicode::fromUtf8(value(cell,"detail",text()).text);
            native.badge=unicode::fromUtf8(value(cell,"badge",text()).text);
            native.leadingIcon=icon(value(cell,"icon",text()).text);
            native.trailingIcon=icon(value(cell,"trailing_icon",text()).text);
            native.foreground=color(value(cell,"foreground",integer(-1)).integer);
            native.indicator=color(value(cell,"indicator",integer(-1)).integer);
            native.badgeForeground=color(value(cell,"badge_foreground",integer(-1)).integer);
            native.badgeBackground=color(value(cell,"badge_background",integer(-1)).integer);
            const auto align=value(cell,"alignment",integer(0)).integer;
            const auto weight=value(cell,"weight",integer(400)).integer;
            const auto size=value(cell,"font_size",number(0)).number;
            const auto iconSize=value(cell,"icon_size",number(20)).number;
            check(align>=0 && align<=2 && weight>=100 && weight<=900 &&
                  std::isfinite(size) && size>=0 && std::isfinite(iconSize) && iconSize>=0,
                  ONEUI_VIEW_TYPE,"Table cell presentation range");
            native.alignment=TextAlign(align);native.fontWeight=int(weight);
            native.fontSize=float(size);native.iconSize=float(iconSize);
            nativeCells->push_back(std::move(native));
        }
        row.value=std::move(nativeCells);
        updatedCells.insert(key);
        prepared.emplace_back(key,std::move(row));
    }
    auto commit=[cache,prepared=std::move(prepared),metadata=std::move(metadata),keys=state.keys,serial=state.serial,
                 updatedCells=std::move(updatedCells),filter=std::move(filter),source=&state,
                 reset=reset||visibilityChanged||filterChanged,prune,&instance]() mutable {
        ONEUI_VIEW_TIME(Rows);
        struct Applying {Instance& node;bool old;Applying(Instance& n):node(n),old(n.applying){n.applying=true;}
            ~Applying(){node.applying=old;}} applying(instance);
        auto table=instance.element.as<Table>();
        const auto scroll=table->scrollOffset();
        const auto oldTop=table->rowHeight()>0?size_t(scroll/table->rowHeight()):0;
        const auto topKey=oldTop<cache->keys.size()?cache->keys[oldTop]:std::string{};
        const auto remainder=table->rowHeight()>0?std::fmod(scroll,table->rowHeight()):0;
        for(const auto& change:metadata) {
            change.row->selected=change.selected;change.row->visible=change.visible;
            change.row->selectedVersion=change.selectedVersion;
        }
        for(auto& [key,row]:prepared) {
            auto old=cache->rows.find(key);
            // Visibility/selection changes keep the same leases. Rebuilding
            // their ownership index would allocate once per unchanged cell.
            const bool sameOwners=old!=cache->rows.end() && old->second.scope==row.scope && old->second.cells==row.cells;
            if(old!=cache->rows.end() && !sameOwners) {
                cache->owners.erase(old->second.scope.get());
                for(auto& cell:*old->second.cells)cache->owners.erase(cell.get());
            }
            if(!sameOwners) {
                cache->owners[row.scope.get()]={key,false};
                for(auto& cell:*row.cells)cache->owners[cell.get()]={key,true};
            }
            cache->rows[key]=std::move(row);
        }
        const auto allKeys=prune ? keys : std::vector<std::string>{};
        keys.erase(std::remove_if(keys.begin(),keys.end(),
            [&](const auto& key){return !cache->rows.at(key).visible || (filter.active && !filter.keys.count(key));}),keys.end());
        if(reset) {
            std::vector<std::vector<TableCell>> rows;rows.reserve(keys.size());
            for(auto& [_,row]:cache->rows)row.nextIndex=-1;
            for(size_t i=0;i<keys.size();++i) {
                auto& row=cache->rows.at(keys[i]);row.nextIndex=int(i);
                rows.push_back(*row.value);
            }
            std::vector<int> mapping;
            mapping.reserve(cache->keys.size());
            for (const auto& key:cache->keys) {
                const auto found=cache->rows.find(key);
                mapping.push_back(found==cache->rows.end()?-1:found->second.nextIndex);
            }
            check(table->reconcileRichRows(std::move(rows),mapping),ONEUI_VIEW_TYPE,"Table row identity mapping");
            if(prune) {
                std::unordered_set<std::string> live(allKeys.begin(),allKeys.end());
                for(auto it=cache->rows.begin();it!=cache->rows.end();) {
                    if(live.count(it->first)){++it;continue;}
                    cache->owners.erase(it->second.scope.get());
                    for(auto& cell:*it->second.cells)cache->owners.erase(cell.get());
                    it=cache->rows.erase(it);
                }
            }
        } else {
            for(size_t i=0;i<keys.size();++i)
                if(updatedCells.count(keys[i]))table->updateRichRow(i,*cache->rows.at(keys[i]).value);
        }
        cache->selected.clear();
        for(size_t i=0;i<keys.size();++i)if(cache->rows.at(keys[i]).selected)cache->selected.push_back(int(i));
        // A redundant selection setter would discard the keyboard range anchor.
        if (table->selectedIndices()!=cache->selected) table->setSelectedIndices(cache->selected);
        auto top=std::find(keys.begin(),keys.end(),topKey);
        table->setScrollOffset(top!=keys.end()?float(top-keys.begin())*table->rowHeight()+remainder:scroll);
        cache->keys=std::move(keys);cache->serial=serial;cache->source=source;
        cache->filterActive=filter.active;cache->filterKeys=std::move(filter.keys);
        instance.richTableCache=cache;instance.recordDirty.clear();
    };
    if(staging && instance.published)commits.push_back(std::move(commit));else commit();
}
}
