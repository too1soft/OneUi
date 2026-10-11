#include "workspace_cache.h"
#include "internal/unicode.h"
namespace oneui::views {
bool Runtime::paneHeaderScalar(Instance& instance,const std::string& name,const Value& value) {
    auto& e=instance.element;
    if(e.component=="HeaderSlot" && name=="slot-name") {
        check(value.text=="identity"||value.text=="actions"||value.text=="menu",ONEUI_VIEW_TYPE,"Header slot name");
        e.as<PaneHeaderSlot>()->name=value.text;return true;
    }
    if(e.component!="PaneHeader")return false;
    auto header=e.as<PaneHeader>();auto config=header->config();
    if(name=="tooltip") {
        if(!instance.workspaceCache)instance.workspaceCache=std::make_shared<Instance::WorkspaceCache>();
        instance.workspaceCache->headerTooltip=unicode::fromUtf8(value.text);
        header->setTooltip(*instance.workspaceCache->headerTooltip);return true;
    }
    for(const auto& action:std::initializer_list<std::pair<const char*,PaneHeaderAction>>{
        {"split-label",PaneHeaderAction::Split},{"maximize-label",PaneHeaderAction::Maximize},
        {"close-label",PaneHeaderAction::Close},{"live-label",PaneHeaderAction::Live},
        {"menu-label",PaneHeaderAction::Menu}})if(name==action.first) {
        header->setActionLabel(action.second,unicode::fromUtf8(value.text));return true;
    }

    if(name=="pane-key")config.pane=value.text;
    else if(name=="number")config.number=value.integer;
    else if(name=="title")config.title=unicode::fromUtf8(value.text);
    else if(name=="path")config.path=unicode::fromUtf8(value.text);
    else if(name=="active")config.active=bool(value.integer);
    else if(name=="broadcast-role")config.broadcastRole=value.text;
    else if(name=="broadcast-hint")config.broadcastHint=unicode::fromUtf8(value.text);
    else if(name=="broadcast-label")config.broadcastLabel=unicode::fromUtf8(value.text);
    else if(name=="viewing-history")config.viewingHistory=bool(value.integer);
    else if(name=="can-close")config.canClose=bool(value.integer);
    else if(name=="can-split")config.canSplit=bool(value.integer);
    else if(name=="can-maximize")config.canMaximize=bool(value.integer);
    else return false;
    header->setConfig(std::move(config));
    if(instance.workspaceCache && instance.workspaceCache->headerTooltip)
        header->setTooltip(*instance.workspaceCache->headerTooltip);
    return true;
}
} // namespace oneui::views
