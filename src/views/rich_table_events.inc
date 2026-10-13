#include "rich_table_cache.h"
namespace oneui::views {
void Runtime::wireRichTable(const std::shared_ptr<Instance>& instance) {
    auto table=instance->element.as<Table>();std::weak_ptr<Instance> weak=instance;
    auto key=[weak](int index) {
        Value value;value.tag=4;
        auto node=weak.lock();auto cache=node?node->richTableCache:nullptr;
        if(cache && index>=0 && size_t(index)<cache->keys.size())value.text=cache->keys[index];
        return value;
    };
    table->setOnSelectionChanged([weak](const std::vector<int>& indices) {
        auto node=weak.lock();auto owner=node?node->owner.lock():nullptr;
        if(!owner || owner->closing || node->applying || !node->richTableCache)return;
        auto cache=node->richTableCache;
        try {
            std::set<int> before(cache->selected.begin(),cache->selected.end()),after(indices.begin(),indices.end());
            std::vector<int> changed;
            std::set_symmetric_difference(before.begin(),before.end(),after.begin(),after.end(),std::back_inserter(changed));
            cache->selected=indices;
            for(auto index:changed) {
                if(index<0 || size_t(index)>=cache->keys.size())continue;
                auto& row=cache->rows.at(cache->keys[index]);
                row.selected=bool(after.count(index));
                Value value;value.tag=1;value.integer=row.selected;
                owner->set(row.scope,owner->image.schema.model(row.scope->table.model_id).properties.at("selected").id,value);
                if(owner->closing)return;
            }
            emitComponentEvent(weak,"selection",{});
        } catch(const Failure& error){owner->fail(error.result,error.what());}
          catch(...){owner->fail(ONEUI_VIEW_FOREIGN_FAILURE,"Table selection callback failed");}
    });
    table->setOnChanged([weak,key](int row){emitComponentEvent(weak,"current",{key(row)});});
    table->setOnActivated([weak,key](int row){emitComponentEvent(weak,"activate",{key(row)});});
    table->setOnEditRequested([weak,key](int row){emitComponentEvent(weak,"edit",{key(row)});});
    table->setOnDeleteRequested([weak](const auto&){emitComponentEvent(weak,"delete",{});});
    table->setOnCellAction([weak,key](int row,int column){
        Value c;c.tag=2;c.integer=column;
        emitComponentEvent(weak,row<0?"sort":"cell",row<0?std::vector<Value>{c}:std::vector<Value>{key(row),c});
    });
    table->setOnContextMenuRequested([weak,key](int row,Point point){
        Value x,y;x.tag=y.tag=3;x.number=point.x;y.number=point.y;
        emitComponentEvent(weak,"context",{key(row),x,y});
    });
}
}
