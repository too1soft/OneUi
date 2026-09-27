#pragma once
#include "oneui/ui_declarative.h"

namespace oneui::ui {
struct LayoutIssue {
    std::string code, path, message;
    StyleRegistry::Source source;
    Rect frame;
};

// Call after native layout, on the UI thread. This audit neither changes the
// tree nor requests frames. Scroll content is intentionally outside its viewport.
inline std::vector<LayoutIssue> inspectLayout(const std::shared_ptr<Widget>& root,const StyleRegistry& registry) {
    const auto sources=registry.sources();
    std::vector<LayoutIssue> issues;
    std::function<void(const std::shared_ptr<Widget>&,const std::shared_ptr<Widget>&,std::string,StyleRegistry::Source)> visit;
    visit=[&](const auto& widget,const auto& parent,std::string path,StyleRegistry::Source source) {
        if(!widget || !widget->visible() || issues.size()>=100)return;
        auto found=sources.find(widget.get());
        if(found!=sources.end()) {
            auto own=found->second;
            if(own.file.empty()){own.file=source.file;own.line=source.line;}
            source=std::move(own);
        }
        const auto f=widget->frame();
        auto issue=[&](std::string code,std::string message) {
            if(auto stack=std::dynamic_pointer_cast<Stack>(parent)) {
                message+="; parent axis="+std::string(stack->direction()==StackDirection::Row?"horizontal":"vertical");
                if(auto flex=stack->explicitFlex(widget)) {
                    std::ostringstream constraints;constraints<<", min="<<flex->min<<", max="<<flex->max<<", shrink="<<flex->shrink;
                    if(flex->basis)constraints<<", basis="<<*flex->basis;
                    message+=constraints.str();
                }
            }
            issues.push_back({std::move(code),path,std::move(message),source,f});
        };
        if(!std::isfinite(f.x) || !std::isfinite(f.y) || !std::isfinite(f.width) || !std::isfinite(f.height) || f.width<0 || f.height<0) {issue("invalid-frame","Invalid resolved geometry");return;}
        if(parent && !std::dynamic_pointer_cast<ScrollView>(parent)) {
            auto p=parent->frame();
            if(f.x<p.x-1 || f.y<p.y-1 || f.x+f.width>p.x+p.width+1 || f.y+f.height>p.y+p.height+1)
                issue("overflow","Child exceeds parent bounds; check minimum/basis constraints or use a scrolling body");
        }
        if(auto label=std::dynamic_pointer_cast<Label>(widget); label && !label->text().empty()) {
            auto needed=label->measure({std::max(0.f,f.width),INFINITY});
            if(needed.height>f.height+1)issue("text-clipped","Text needs "+std::to_string(needed.height)+"px height; allow content height or reduce parent compression");
        }
        if(auto view=std::dynamic_pointer_cast<View>(widget)) {
            int index=0;
            for(auto& child:view->children())visit(child,widget,path+"/"+std::to_string(index++),source);
        }
    };
    visit(root,{},"root",{});return issues;
}
inline std::string formatLayoutIssues(const std::vector<LayoutIssue>& issues) {
    if(issues.empty())return "No layout issues.\n";
    std::ostringstream out;
    for(auto& i:issues) {
        if(!i.source.file.empty())out<<i.source.file<<":"<<i.source.line<<": ";
        out<<"["<<i.code<<"] "<<i.source.component<<" ("<<i.path<<") "<<i.message
           <<"; frame="<<i.frame.x<<","<<i.frame.y<<","<<i.frame.width<<","<<i.frame.height<<"\n";
    }
    return out.str();
}
} // namespace oneui::ui
