#include "picker_cache.h"
#include "internal/unicode.h"
#include <cmath>
namespace oneui::views {
bool Runtime::pickerScalar(Instance& instance,const std::string& name,const Value& value) {
    auto& e=instance.element;const auto& type=e.component;
    if(type=="MediaSlot"&&name=="slot-name") {
        check(name=="slot-name"&&(value.text=="empty"||value.text=="error"||value.text=="preview"||value.text=="loading"),ONEUI_VIEW_TYPE,"Invalid media slot");
        e.as<MediaSlotView>()->name=value.text;return true;
    }
    if(type!="FontPicker"&&type!="MediaPicker"&&type!="MediaPreview"&&type!="Slider")return false;
    auto old=instance.pickerCache;
    auto next=old?std::make_shared<Instance::PickerCache>(*old):std::make_shared<Instance::PickerCache>();
    auto wide=[&]{return unicode::fromUtf8(value.text);};
    if(type=="FontPicker") {
        if(name=="selected")next->font.selected=value.text;
        else if(name=="error")next->font.error=wide();
        else if(name=="preview-text")next->font.previewText=wide();
        else if(name=="loading")next->font.loading=bool(value.integer);
        else if(name=="size")next->font.size=float(value.number);
        else if(name=="trigger-label")next->triggerLabel=wide();
        else if(name=="confirm-selection")next->confirmSelection=bool(value.integer);
        else if(name=="monospace-only")next->monospaceOnly=bool(value.integer);
        else if(name=="apply-label")next->applyLabel=wide();
        else if(name=="cancel-label")next->cancelLabel=wide();
        else if(name=="filter-label")next->filterLabel=wide();
        else if(name=="search-label")next->searchLabel=wide();
        else if(name=="retry-label")next->retryLabel=wide();
        else return false;
    } else if(type=="MediaPicker") {
        if(name=="asset")next->media.asset=value.text;
        else if(name=="label")next->media.label=wide();
        else if(name=="error")next->media.error=wide();
        else if(name=="loading")next->media.loading=bool(value.integer);
        else if(name=="choose-label")next->chooseLabel=wide();
        else if(name=="clear-label")next->clearLabel=wide();
        else if(name=="cancel-label")next->cancelLabel=wide();
        else if(name=="accept")check(value.text=="image",ONEUI_VIEW_TYPE,"MediaPicker accept must be image");
        else return false;
    } else if(type=="MediaPreview") {
        if(name=="source")next->source=value.text;
        else if(name=="fit")next->fit=value.text;
        else if(name=="position")next->position=value.text;
        else if(name=="opacity")next->opacity=value.number;
        else return false;
    } else {
        if(name=="minimum")next->minimum=value.number;
        else if(name=="maximum")next->maximum=value.number;
        else if(name=="step")next->step=value.number;
        else if(name=="value")next->value=value.number;
        else return false;
    }
    if(staging)rollbacks.push_back([&instance,old]{instance.pickerCache=old;for(auto& b:instance.bindings)b.previous.reset();});
    instance.pickerCache=std::move(next);return true;
}
bool Runtime::pickerList(Instance& instance,const std::string& name,ListState& list) {
    const auto& type=instance.element.component;
    if(!((type=="FontPicker"&&(name=="items"||name=="monospace-items"))||(type=="ThemeScope"&&name=="tokens")))return false;
    auto old=instance.pickerCache;
    auto next=old?std::make_shared<Instance::PickerCache>(*old):std::make_shared<Instance::PickerCache>();
    auto field=[&](const std::shared_ptr<Scope>& scope,const char* key){return read(scope,image.schema.model(scope->table.model_id).properties.at(key).id);};
    if(type=="FontPicker"&&name=="monospace-items") {
        next->monospaceKeys.clear();
        for(auto& item:list.items)next->monospaceKeys.push_back(field(item,"value").text);
    } else if(type=="FontPicker") {
        next->font.items.clear();next->font.items.reserve(list.items.size());
        for(size_t i=0;i<list.items.size();++i)next->font.items.push_back({list.keys[i],unicode::fromUtf8(field(list.items[i],"family").text),unicode::fromUtf8(field(list.items[i],"label").text),bool(field(list.items[i],"available").integer)});
    } else {
        next->colors.clear();for(size_t i=0;i<list.items.size();++i)next->colors.push_back({list.keys[i],field(list.items[i],"rgba").integer});
        ThemeScope::validate(next->colors);
    }
    if(staging)rollbacks.push_back([&instance,old]{instance.pickerCache=old;});
    instance.pickerCache=std::move(next);return true;
}
void Runtime::preparePicker(const std::shared_ptr<Instance>& instance) {
    const auto& type=instance->element.component;
    if(type!="FontPicker"&&type!="MediaPicker"&&type!="MediaPreview"&&type!="Slider"&&type!="ThemeScope")return;
    if(!instance->pickerCache)instance->pickerCache=std::make_shared<Instance::PickerCache>();
    auto c=instance->pickerCache;std::function<void()> apply;
    if(type=="FontPicker") {
        check(std::isfinite(c->font.size)&&c->font.size>0,ONEUI_VIEW_TYPE,"Invalid font size");
        apply=[w=instance->element.as<FontPicker>(),c]{
            w->setTriggerLabel(c->triggerLabel);w->setConfig(c->font);
            w->setConfirmation(c->confirmSelection,c->applyLabel,c->cancelLabel);
            w->setMonospaceFilter(c->monospaceKeys,c->monospaceOnly,c->filterLabel);
            if(!c->searchLabel.empty())w->setSearchLabel(c->searchLabel);
            w->setRetryLabel(c->retryLabel);
        };
    } else if(type=="MediaPicker")apply=[w=instance->element.as<MediaPicker>(),c]{w->setChooseLabel(c->chooseLabel);w->setClearLabel(c->clearLabel);w->setCancelLabel(c->cancelLabel);w->setConfig(c->media);};
    else if(type=="ThemeScope")apply=[w=instance->element.as<ThemeScope>(),c]{w->setTokens(c->colors);};
    else if(type=="MediaPreview") {
        check(c->fit=="contain"||c->fit=="cover"||c->fit=="stretch"||c->fit=="auto",ONEUI_VIEW_TYPE,"Invalid media fit");
        check(std::set<std::string>{"top-left","top","top-right","left","center","right","bottom-left","bottom","bottom-right"}.count(c->position),ONEUI_VIEW_TYPE,"Invalid media position");
        check(std::isfinite(c->opacity)&&c->opacity>=0&&c->opacity<=1,ONEUI_VIEW_TYPE,"Invalid media opacity");
        apply=[w=instance->element.as<MediaPreview>(),c]{w->setFit(c->fit);w->setPosition(c->position);w->setOpacity(float(c->opacity));w->setSource(c->source);};
    } else {
        check(std::isfinite(c->minimum)&&std::isfinite(c->maximum)&&std::isfinite(c->step)&&std::isfinite(c->value)&&std::isfinite(c->maximum-c->minimum)&&c->minimum<c->maximum&&c->step>0,ONEUI_VIEW_TYPE,"Invalid slider range/value");
        apply=[w=instance->element.as<Slider>(),c]{w->setRange(c->minimum,c->maximum);w->setStep(c->step);w->setValue(c->value);};
    }
    auto commit=[instance,apply=std::move(apply)] {
        instance->applying=true;
        try{apply();}catch(...){instance->applying=false;throw;}
        instance->applying=false;
    };
    if(staging&&instance->published)commits.push_back(std::move(commit));else commit();
}
} // namespace oneui::views
