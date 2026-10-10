#include "runtime.h"
#include "oneui/layout/pane_grid.h"
namespace oneui::views {
namespace {
Value text(std::string s){Value v;v.tag=4;v.text=std::move(s);return v;}
Value integer(int64_t n){Value v;v.tag=2;v.integer=n;return v;}
Value number(double n){Value v;v.tag=3;v.number=n;return v;}
}
void emitComponentEvent(const std::weak_ptr<Instance>& weak,const std::string& name,std::vector<Value> args) {
    auto item=weak.lock();auto owner=item?item->owner.lock():nullptr;
    if(!owner||owner->closing||item->applying)return;
    const auto id=oneview::registry().at(item->element.component).event(name).id;
    for(auto& record:item->events)if(record[1]==id) {
        try {owner->invoke(item,record,std::move(args));}
        catch(const Failure& e){owner->fail(e.result,e.what(),record[7]);}
        catch(...){owner->fail(ONEUI_VIEW_FOREIGN_FAILURE,"Component callback failed",record[7]);}
        return;
    }
}
void Runtime::wireWorkspace(const std::shared_ptr<Instance>& instance) {
    wireDrag(instance);
    wireDock(instance);
    if(instance->element.component=="PaneHeader") {
        std::weak_ptr<Instance> weak=instance;
        instance->element.as<PaneHeader>()->setOnIntent([weak](PaneHeaderAction action,Point point) {
            auto item=weak.lock();if(!item)return;
            const char* names[]={"activateRequested","splitRequested","maximizeRequested","closeRequested","broadcastRequested","liveRequested","menuRequested"};
            std::vector<Value> args{text(item->element.as<PaneHeader>()->config().pane)};
            if(action==PaneHeaderAction::Split)args.push_back(text("right"));
            if(action==PaneHeaderAction::Menu){args.push_back(number(point.x));args.push_back(number(point.y));}
            emitComponentEvent(weak,names[int(action)],std::move(args));
        });return;
    }
    if(instance->element.component!="PaneGrid")return;
    std::weak_ptr<Instance> weak=instance;
    instance->element.as<PaneGrid>()->setOnIntent([weak](const WorkspaceIntent& intent) {
        auto item=weak.lock();auto owner=item?item->owner.lock():nullptr;
        if(!owner||owner->closing||item->applying)return;
        auto& contract=oneview::registry().at("PaneGrid");
        std::string name;std::vector<Value> args{text(intent.source)};
        switch(intent.kind) {
        case WorkspaceIntentKind::Drop:name="dropRequested";args.push_back(text(intent.target));args.push_back(text(intent.edge));break;
        case WorkspaceIntentKind::Ratio:name="ratioRequested";args.push_back(number(intent.value));break;
        case WorkspaceIntentKind::Focus:name="focusRequested";args.push_back(text(intent.target));break;
        case WorkspaceIntentKind::Swap:name="swapRequested";args.push_back(text(intent.target));break;
        case WorkspaceIntentKind::Resize:name="resizeSettled";args.push_back(integer(intent.columns));args.push_back(integer(intent.rows));break;
        case WorkspaceIntentKind::Transition:name="transitionSettled";break;
        }
        args.push_back(integer(intent.request));
        if(intent.kind!=WorkspaceIntentKind::Resize)args.push_back(integer(intent.revision));
        for(auto& record:item->events)if(record[1]==contract.event(name).id) {
            try {
                owner->invoke(item,record,std::move(args));
                if(!owner->closing && (intent.kind==WorkspaceIntentKind::Focus || intent.kind==WorkspaceIntentKind::Swap)) {
                    owner->flush();
                    // Rebuild the complete route from the declarative root,
                    // including ancestors previously focused on the toolbar.
                    auto target=item->element.widget->activeFocusChild();
                    while(target) {auto child=target->activeFocusChild();if(!child)break;target=std::move(child);}
                    if(target)if(auto root=std::dynamic_pointer_cast<View>(owner->root->element.widget))root->requestFocus(target.get(),false);
                }
            }
            catch(const Failure& e){owner->fail(e.result,e.what(),record[7]);}
            catch(...){owner->fail(ONEUI_VIEW_FOREIGN_FAILURE,"Workspace callback failed",record[7]);}
            return;
        }
        // The grid owns the request identity. An authored strip may handle it
        // only when the grid has no binding, avoiding duplicate VM mutations.
        if(intent.kind==WorkspaceIntentKind::Drop&&intent.edge=="tab-strip") {
            std::vector<std::shared_ptr<Instance>> pending{owner->root};
            while(!pending.empty()) {
                auto candidate=pending.back();pending.pop_back();
                if(candidate->element.component=="DropSurface" &&
                   owner->image.strings[owner->image.records[oneview::NODE][candidate->node][11]]==intent.target) {
                    emitComponentEvent(candidate,"dropRequested",std::move(args));return;
                }
                for(auto& child:candidate->children)pending.push_back(child);
                for(auto& row:candidate->rows)pending.push_back(row.second);
            }
        }
    });
}
} // namespace oneui::views
