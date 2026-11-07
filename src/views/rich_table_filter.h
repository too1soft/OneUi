#pragma once
#include "runtime.h"
#include <unordered_set>

namespace oneui::views {
struct RichTableFilter {
    bool active=false;
    std::unordered_set<std::string> keys;
};

// Optional per-source-list metadata keeps filtering declarative without
// invalidating every retained record. False means all rows; active + empty
// means no rows. Display order always follows the source list.
inline RichTableFilter readRichTableFilter(Runtime& runtime,const std::shared_ptr<Scope>& owner,
                                           uint32_t property,const ListState& source) {
    RichTableFilter result;
    const auto& model=runtime.image.schema.model(owner->table.model_id);
    const auto prefix=model.property(property).name;
    const auto active=model.properties.find(prefix+"_filter_active");
    const auto keys=model.properties.find(prefix+"_filter_keys");
    if(active==model.properties.end() && keys==model.properties.end())return result;
    check(active!=model.properties.end() && keys!=model.properties.end() &&
          active->second.tag==1 && keys->second.tag==5,ONEUI_VIEW_TYPE,"Table filter metadata types");
    result.active=bool(runtime.read(owner,active->second.id).integer);
    if(!result.active)return result;
    const auto& mask=runtime.list(owner,keys->second.id);
    result.keys.insert(mask.keys.begin(),mask.keys.end());
    if(result.keys.size()<=8) {
        for(const auto& key:result.keys)
            check(std::find(source.keys.begin(),source.keys.end(),key)!=source.keys.end(),
                  ONEUI_VIEW_BAD_KEY,"Table filter references an unknown source key");
    } else {
        std::unordered_set<std::string> live(source.keys.begin(),source.keys.end());
        for(const auto& key:result.keys)
            check(live.count(key),ONEUI_VIEW_BAD_KEY,"Table filter references an unknown source key");
    }
    return result;
}
}
