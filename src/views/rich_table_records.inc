#include "rich_table_cache.h"
namespace oneui::views {
void Runtime::applyRichTableRows(Instance& instance,ListState& state) {
    using Cache=Instance::RichTableCache;
    auto cache=instance.richTableCache;
    const bool reset=!cache || cache->source!=&state || cache->serial!=state.serial;
    if(!cache)cache=std::make_shared<Cache>();
    std::set<std::string> dirty;
    if(reset)dirty.insert(state.keys.begin(),state.keys.end());
    else for(auto scope:instance.recordDirty) {
        auto found=cache->owners.find(scope);
        if(found!=cache->owners.end())dirty.insert(found->second);
    }
    if(!reset && dirty.empty())return;
    std::unordered_map<std::string,size_t> indices;
    for(size_t i=0;i<state.keys.size();++i)indices.emplace(state.keys[i],i);
    std::map<std::string,Cache::Row> prepared;
    auto value=[&](const std::shared_ptr<Scope>& scope,const char* name,Value fallback) {
        const auto& fields=image.schema.model(scope->table.model_id).properties;
        const auto found=fields.find(name);
        return found==fields.end()?fallback:read(scope,found->second.id);
    };
    auto integer=[](int64_t n){Value v;v.tag=2;v.integer=n;return v;};
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
        auto at=indices.find(key);if(at==indices.end())continue;
        Cache::Row row;row.scope=state.items[at->second];
        const auto& fields=image.schema.model(row.scope->table.model_id).properties;
        row.selected=bool(read(row.scope,fields.at("selected").id).integer);
        row.cells=list(row.scope,fields.at("cells").id).items;
        for(const auto& cell:row.cells) {
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
            row.value.push_back(std::move(native));
        }
        prepared.emplace(key,std::move(row));
    }
    auto commit=[cache,prepared=std::move(prepared),keys=state.keys,serial=state.serial,
                 source=&state,reset,&instance]() mutable {
        struct Applying {Instance& node;bool old;Applying(Instance& n):node(n),old(n.applying){n.applying=true;}
            ~Applying(){node.applying=old;}} applying(instance);
        auto table=instance.element.as<Table>();
        const auto scroll=table->scrollOffset();
        const auto oldTop=table->rowHeight()>0?size_t(scroll/table->rowHeight()):0;
        const auto topKey=oldTop<cache->keys.size()?cache->keys[oldTop]:std::string{};
        const auto remainder=table->rowHeight()>0?std::fmod(scroll,table->rowHeight()):0;
        for(auto& [key,row]:prepared) {
            if(auto old=cache->rows.find(key);old!=cache->rows.end()) {
                cache->owners.erase(old->second.scope.get());
                for(auto& cell:old->second.cells)cache->owners.erase(cell.get());
            }
            cache->owners[row.scope.get()]=key;
            for(auto& cell:row.cells)cache->owners[cell.get()]=key;
            cache->rows[key]=std::move(row);
        }
        if(reset) {
            std::vector<std::vector<TableCell>> rows;rows.reserve(keys.size());
            for(const auto& key:keys)rows.push_back(cache->rows.at(key).value);
            std::unordered_map<std::string,int> destinations;
            for (size_t i=0;i<keys.size();++i) destinations.emplace(keys[i],int(i));
            std::vector<int> mapping;
            mapping.reserve(cache->keys.size());
            for (const auto& key:cache->keys) {
                const auto found=destinations.find(key);
                mapping.push_back(found==destinations.end()?-1:found->second);
            }
            check(table->reconcileRichRows(std::move(rows),mapping),ONEUI_VIEW_TYPE,"Table row identity mapping");
            std::set<std::string> live(keys.begin(),keys.end());
            for(auto it=cache->rows.begin();it!=cache->rows.end();) {
                if(live.count(it->first)){++it;continue;}
                cache->owners.erase(it->second.scope.get());
                for(auto& cell:it->second.cells)cache->owners.erase(cell.get());
                it=cache->rows.erase(it);
            }
        } else {
            for(size_t i=0;i<keys.size();++i)
                if(prepared.count(keys[i]))table->updateRichRow(i,cache->rows.at(keys[i]).value);
        }
        cache->selected.clear();
        for(size_t i=0;i<keys.size();++i)if(cache->rows.at(keys[i]).selected)cache->selected.push_back(int(i));
        // A redundant selection setter would discard the keyboard range anchor.
        if (table->selectedIndices()!=cache->selected) table->setSelectedIndices(cache->selected);
        auto top=std::find(keys.begin(),keys.end(),topKey);
        table->setScrollOffset(top!=keys.end()?float(top-keys.begin())*table->rowHeight()+remainder:scroll);
        cache->keys=std::move(keys);cache->serial=serial;cache->source=source;
        instance.richTableCache=cache;instance.recordDirty.clear();
    };
    if(staging && instance.published)commits.push_back(std::move(commit));else commit();
}
}
