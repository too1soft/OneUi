#include "reload_plan.h"
#include "pointer_event_slots.h"
namespace oneui::views {
namespace {
std::string identity(const Runtime &runtime,const Instance &node) {
    const auto &image=runtime.image;
    std::string result=node.element.component+":"+image.strings[image.records.at(oneview::NODE)[node.node][11]];
    for(auto owner:node.owners)
        if(image.records.at(oneview::NODE)[owner][0]==3)
            result+="/call:"+image.strings[image.records.at(oneview::NODE)[owner][11]];
    return result;
}
bool sameScopes(const Instance &a,const Instance &b) {
    if(a.scopes.size()!=b.scopes.size())return false;
    for(size_t i=0;i<a.scopes.size();++i)
        if(a.scopes[i]->vm!=b.scopes[i]->vm || a.scopes[i]->identity!=b.scopes[i]->identity)return false;
    return true;
}
void walk(const std::shared_ptr<Instance> &node,const std::function<void(const std::shared_ptr<Instance>&)> &f) {
    f(node);
    for(auto &child:node->children)walk(child,f);
    for(auto &[_,row]:node->rows)walk(row,f);
}
std::map<const Widget*,std::string> positions(const std::shared_ptr<Widget> &root) {
    std::map<const Widget*,std::string> paths;
    std::function<void(const std::shared_ptr<Widget>&,std::string)> visit=[&](const auto &w,std::string path) {
        paths.emplace(w.get(),path);
        if(auto view=std::dynamic_pointer_cast<View>(w))
            for(size_t i=0;i<view->children().size();++i)visit(view->children()[i],path+"/"+std::to_string(i));
    };
    visit(root,"");return paths;
}
}
ReloadPlan::ReloadPlan(Runtime &old,Runtime &next):old(old),next(next) {
    walk(old.root,[&](const auto &n){oldSemanticWidgets.insert(n->element.widget.get());});
    walk(next.root,[&](const auto &n){semanticWidgets.insert(n->element.widget.get());});
    match(old.root,next.root);
    if(matches.count(next.root.get()))native(old.root->element.widget,next.root->element.widget);
    const auto before=positions(old.root->element.widget),after=positions(next.root->element.widget);
    for(auto &[name,widget]:old.hosts) {
        auto host=std::dynamic_pointer_cast<ui::NativeHostView>(widget);
        if(host->children().empty())continue;
        auto replacement=next.hosts.find(name);
        check(replacement!=next.hosts.end() && widgets.count(replacement->second.get()) &&
            widgets.at(replacement->second.get())==widget &&
            before.at(widget.get())==after.at(replacement->second.get()),ONEUI_VIEW_BUSY,
            "Reload would remove, rename or move an attached NativeHost");
    }
}
ReloadPlan::~ReloadPlan() {
    if(!committed)for(auto it=rollback.rbegin();it!=rollback.rend();++it)
        try {(*it)();}catch(...){}
}
void ReloadPlan::match(const std::shared_ptr<Instance> &a,const std::shared_ptr<Instance> &b) {
    if(identity(old,*a)!=identity(next,*b) || !sameScopes(*a,*b))return;
    matches.emplace(b.get(),a);
    std::map<std::string,std::shared_ptr<Instance>> previous;
    for(auto &child:a->children)previous.emplace(identity(old,*child),child);
    for(auto &child:b->children) {
        auto found=previous.find(identity(next,*child));
        if(found!=previous.end())match(found->second,child);
    }
    for(auto &[key,row]:b->rows) {
        auto found=a->rows.find(key);
        if(found!=a->rows.end())match(found->second,row);
    }
}
void ReloadPlan::native(const std::shared_ptr<Widget> &a,const std::shared_ptr<Widget> &b) {
    if(typeid(*a)!=typeid(*b))return;
    widgets.emplace(b.get(),a);
    auto av=std::dynamic_pointer_cast<View>(a),bv=std::dynamic_pointer_cast<View>(b);
    if(!av || !bv || std::dynamic_pointer_cast<ui::NativeHostView>(a))return;
    for(size_t i=0;i<bv->children().size();++i) {
        auto child=bv->children()[i];
        std::shared_ptr<Widget> oldChild;
        if(semanticWidgets.count(child.get())) {
            for(auto &[node,match]:matches)
                if(node->element.widget==child && std::find(av->children().begin(),av->children().end(),
                    match->element.widget)!=av->children().end()) {oldChild=match->element.widget;break;}
        } else if(i<av->children().size() && !oldSemanticWidgets.count(av->children()[i].get()))oldChild=av->children()[i];
        if(oldChild)native(oldChild,child);
    }
}
bool ReloadPlan::commitComposition() {
    std::set<const Widget *> kept;
    for(auto &[_,widget]:widgets)kept.insert(widget.get());
    bool committed=false;
    walk(old.root,[&kept,&committed](const std::shared_ptr<Instance> &node) {
        if(!kept.count(node->element.widget.get()))
            if(auto input=node->element.as<TextField>())
                committed=input->commitPendingComposition() || committed;
    });
    return committed;
}
void ReloadPlan::prepare() {
    // Candidate parents own only unpublished widgets. Detach their children
    // before preparing additions in surviving parents; live children stay put.
    std::function<void(const std::shared_ptr<Widget>&)> prepare=[&](const auto &widget) {
        auto candidate=std::dynamic_pointer_cast<View>(widget);
        if(!candidate)return;
        auto list=candidate->children();
        for(auto &child:list)prepare(child);
        auto found=widgets.find(widget.get());
        if(found==widgets.end() || std::dynamic_pointer_cast<ui::NativeHostView>(widget))return;
        auto target=std::dynamic_pointer_cast<View>(found->second);
        std::vector<std::shared_ptr<Widget>> desired;
        for(auto &child:list)desired.push_back(widgets.count(child.get()) ? widgets.at(child.get()) : child);
        candidate->clearChildren();
        if(auto oldStack=std::dynamic_pointer_cast<Stack>(target)) {
            auto newStack=std::dynamic_pointer_cast<Stack>(candidate);
            for(size_t i=0;i<list.size();++i) {
                    auto flex=newStack->explicitFlex(list[i]);
                    auto previous=oldStack->explicitFlex(desired[i]);auto child=desired[i];
                    rollback.push_back([oldStack,child,previous] {
                        if(previous)oldStack->setFlex(child,*previous);else oldStack->clearFlex(child);
                    });
                    if(flex)oldStack->setFlex(child,*flex);else oldStack->clearFlex(child);
            }
        }
        children.push_back(target->prepareChildren(std::move(desired)));
    };
    prepare(next.root->element.widget);
    for(auto &[candidate,previous]:matches) {
        if(!widgets.count(candidate->element.widget.get()))continue;
        const auto oldPointer = pointerEventMask(*previous), newPointer = pointerEventMask(*candidate);
        if (oldPointer != newPointer)
            properties.push_back(preparePointerEventSlots(previous, oldPointer, newPointer));
        auto before=authored(old,*previous),after=authored(next,*candidate);
        for(auto &[name,_]:before)
            if(!after.count(name) && name!="class")
                properties.push_back(resetProperty(old.factory,previous->element,candidate->element,name));
        if(!after.count("visible"))
            properties.push_back([widget=previous->element.widget,visible=candidate->element.widget->visible()] {
                widget->setVisible(visible);
            });
        if(!after.count("disabled"))
            properties.push_back([widget=previous->element.widget,disabled=candidate->element.widget->disabled()] {
                widget->setDisabled(disabled);
            });
    }
}
std::shared_ptr<Instance> ReloadPlan::adopt(const std::shared_ptr<Instance> &candidate) {
    auto result=candidate;
    auto found=matches.find(candidate.get());
    if(found!=matches.end() && widgets.count(candidate->element.widget.get())) {
        result=found->second;
        auto element=result->element;
        element.classes=candidate->element.classes;
        element.flex=candidate->element.flex;
        auto mapped=[&](const std::shared_ptr<Widget> &widget) {
            return widget && widgets.count(widget.get()) ? widgets.at(widget.get()) : widget;
        };
        element.title=std::dynamic_pointer_cast<Label>(mapped(candidate->element.title));
        element.hint=std::dynamic_pointer_cast<Label>(mapped(candidate->element.hint));
        element.error=std::dynamic_pointer_cast<Label>(mapped(candidate->element.error));
        element.fieldControl=mapped(candidate->element.fieldControl);
        element.content=std::dynamic_pointer_cast<Stack>(mapped(candidate->element.content));
        element.collapsible=candidate->element.collapsible;
        for(auto &widget:element.collapsible)widget=mapped(widget);
        *result=*candidate;
        result->element=std::move(element);
        // The candidate's registration points at the candidate; register this copy.
        result->shortcut.reset();
        if(result->shortcutChord)old.registerShortcut(*result);
        result->tableCache.reset(); // Candidate cache has not been published to the retained Table.
    }
    result->owner=old.weak_from_this();
    for(auto &child:result->children) {child=adopt(child);child->parent=result;}
    for(auto &[_,row]:result->rows) {row=adopt(row);row->parent=result;}
    result->dirty=true;result->published=true;
    for(auto &binding:result->bindings)binding.previous.reset();
    return result;
}
}
