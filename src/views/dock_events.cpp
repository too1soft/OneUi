#include "runtime.h"
#include "oneui/layout/dock_workspace_view.h"
namespace oneui::views {
namespace {
Value text(std::string s){Value v;v.tag=4;v.text=std::move(s);return v;}
Value integer(int64_t n){Value v;v.tag=2;v.integer=n;return v;}
Value number(double n){Value v;v.tag=3;v.number=n;return v;}
Value boolean(bool n){Value v;v.tag=1;v.integer=n;return v;}
}
void Runtime::wireDock(const std::shared_ptr<Instance>& instance) {
    if(instance->element.component!="DockWorkspace")return;
    std::weak_ptr<Instance> weak=instance;
    instance->element.as<DockWorkspaceView>()->setOnIntent([weak](const DockIntent& event) {
        const char* names[]={"dockRequested","ratioRequested","floatRequested","boundsRequested","pinRequested","raiseRequested","maximizeRequested","visibilityRequested","redockRequested"};
        std::vector<Value> args{text(event.source)};
        if(event.kind==DockIntentKind::Dock){args.push_back(text(event.target));args.push_back(text(event.edge));}
        if(event.kind==DockIntentKind::Ratio)args.push_back(number(event.ratio));
        if(event.kind==DockIntentKind::Float||event.kind==DockIntentKind::Bounds) {
            args.push_back(number(event.bounds.x));args.push_back(number(event.bounds.y));args.push_back(number(event.bounds.width));args.push_back(number(event.bounds.height));
        }
        if(event.kind==DockIntentKind::Pin||event.kind==DockIntentKind::Visibility)args.push_back(boolean(event.flag));
        args.push_back(integer(event.request));args.push_back(integer(event.revision));
        emitComponentEvent(weak,names[int(event.kind)],std::move(args));
    });
}
} // namespace oneui::views
