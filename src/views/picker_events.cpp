#include "runtime.h"
namespace oneui::views {
namespace {
Value string(std::string s){Value v;v.tag=4;v.text=std::move(s);return v;}
Value integer(int64_t n){Value v;v.tag=2;v.integer=n;return v;}
Value number(double n){Value v;v.tag=3;v.number=n;return v;}
}
void Runtime::wirePicker(const std::shared_ptr<Instance>& instance) {
    std::weak_ptr<Instance> weak=instance;auto& e=instance->element;
    if(e.component=="FontPicker") {
        e.as<FontPicker>()->setPopupViewportProvider([weak]{
            auto item=weak.lock();Rect viewport{};
            for(auto parent=item?item->parent.lock():nullptr;parent;parent=parent->parent.lock()) {
                if(parent->element.widget&&parent->element.widget->frame().height>0)viewport=parent->element.widget->frame();
                if(parent->element.component=="Scroll")break;
            }
            return viewport;
        });
        e.as<FontPicker>()->setOnSelectionRequested([weak](std::string key){emitComponentEvent(weak,"selectionRequested",{string(std::move(key))});});
        e.as<FontPicker>()->setOnRetryRequested([weak]{emitComponentEvent(weak,"retryRequested",{});});
    } else if(e.component=="MediaPicker")e.as<MediaPicker>()->setOnIntent([weak](MediaPickerAction action){
        const char* names[]={"chooseRequested","clearRequested","cancelRequested"};emitComponentEvent(weak,names[int(action)],{});
    });
    else if(e.component=="MediaPreview")e.as<MediaPreview>()->setOnLoadFailed([weak](std::string source,std::string code,std::string message){
        emitComponentEvent(weak,"loadFailed",{string(std::move(source)),string(std::move(code)),string(std::move(message))});
    });
    else if(e.component=="Slider")e.as<Slider>()->setOnInteraction([weak](SliderInteraction phase,double value){
        emitComponentEvent(weak,"interaction",{integer(int(phase)),number(value)});
    });
}
} // namespace oneui::views
