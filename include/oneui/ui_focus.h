#pragma once
#include "oneui/ui_declarative.h"

namespace oneui::ui {
// After layout, reveal a retained field through its enclosing scroll views.
// No geometry is authored here; only existing scroll offsets are adjusted.
inline bool revealField(const std::shared_ptr<Widget>& root,Widget* field) {
    std::vector<std::shared_ptr<Widget>> path;
    std::function<bool(std::shared_ptr<Widget>)> find=[&](auto w) {
        if(!w || !w->visible())return false;
        path.push_back(w);if(w.get()==field)return true;
        if(auto v=std::dynamic_pointer_cast<View>(w))for(auto& child:v->children())if(find(child))return true;
        path.pop_back();return false;
    };
    if(!field || !find(root))return false;
    auto target=field->frame();
    for(auto it=path.rbegin();it!=path.rend();++it)if(auto scroll=std::dynamic_pointer_cast<ScrollView>(*it)) {
        const auto viewport=scroll->frame();const float before=scroll->scrollOffset();
        float delta=0;
        if(target.y<viewport.y+8)delta=target.y-viewport.y-8;
        else if(target.y+target.height>viewport.y+viewport.height-8)delta=target.y+target.height-viewport.y-viewport.height+8;
        scroll->setScrollOffset(before+delta);target.y-=scroll->scrollOffset()-before;
    }
    return true;
}
inline Widget* focusedField(const std::shared_ptr<Widget>& root) {
    if(!root || !root->visible())return nullptr;
    if(auto v=std::dynamic_pointer_cast<View>(root))for(auto& child:v->children())if(auto result=focusedField(child))return result;
    return root->focused() && root->isFocusable()?root.get():nullptr;
}
} // namespace oneui::ui
