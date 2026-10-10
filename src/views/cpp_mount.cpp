#include "runtime.h"
#include "oneui/view_cpp_adapter.h"

namespace oneui::ui::view {
namespace {
struct Mounted {
    std::shared_ptr<views::Runtime> runtime;
    ~Mounted() { runtime->close(); }
};
}
Element mount(Mount &mount,const uint8_t *bytes,size_t length,
              const OneUiViewModelVTable *table,void *vm) {
    auto image=oneview::readImage(oneview::Bytes(bytes,bytes+length));
    auto runtime=std::make_shared<views::Runtime>(std::move(image),mount.clipboard(),mount.styles());
    runtime->initialize(table,vm,true);
    std::function<void(const std::shared_ptr<views::Instance>&)> references=[&](const auto &node) {
        auto ref=runtime->image.records[oneview::NODE][node->node][7];
        if(ref!=oneview::none && !runtime->image.strings[ref].empty())
            mount.remember(runtime->image.strings[ref],node->element.widget);
        for(auto &child:node->children)references(child);
    };
    references(runtime->root);
    for(auto &[name,widget]:runtime->hosts)
        mount.attachNative(name,std::dynamic_pointer_cast<NativeHostView>(widget));
    runtime->schedule=mount.scheduler();
    std::weak_ptr<views::Runtime> weak=runtime;
    mount.childFlush_.push_back([weak] { if(auto r=weak.lock())r->flush(); });
    auto owner=std::make_shared<Mounted>();owner->runtime=runtime;mount.own(owner);
    return runtime->root->element;
}
}
