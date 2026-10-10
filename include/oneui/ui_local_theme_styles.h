#pragma once
namespace oneui::ui {
inline void StyleRegistry::applyLocalTheme(const std::shared_ptr<ThemeScope>& scope,const StyleSheet& parent) {
    themeParents_[scope.get()]=std::make_shared<StyleSheet>(parent);
    auto sheet=std::make_shared<StyleSheet>(scope->merge(parent));
    std::weak_ptr<int> lifetime=queryLifetime_;std::weak_ptr<ThemeScope> weak=scope;
    scope->setOnTokensChanged([this,lifetime,weak]{
        if(lifetime.expired())return;
        if(auto scope=weak.lock()) {
            auto found=themeParents_.find(scope.get());
            auto parent=found==themeParents_.end()?std::make_shared<StyleSheet>(sheet_):found->second;
            applyLocalTheme(scope,*parent);
        }
    });
    std::vector<std::shared_ptr<Widget>> work(scope->children().begin(),scope->children().end());
    while(!work.empty()) {
        auto widget=work.back();work.pop_back();
        // NativeHost terminal palettes are deliberately outside this contract.
        if(std::dynamic_pointer_cast<NativeHostView>(widget))continue;
        if(auto nested=std::dynamic_pointer_cast<ThemeScope>(widget)){applyLocalTheme(nested,*sheet);continue;}
        for(auto& entry:entries_)if(entry.widget.lock()==widget){localThemes_[widget.get()]=sheet;apply(entry,sheet_);break;}
        if(auto view=std::dynamic_pointer_cast<View>(widget))for(auto& child:view->children())work.push_back(child);
    }
}
inline void StyleRegistry::refreshLocalThemes() {
    localThemes_.clear();themeParents_.clear();
    std::vector<std::shared_ptr<ThemeScope>> scopes;
    std::set<const Widget*> nested;
    for(auto& entry:entries_)if(auto scope=std::dynamic_pointer_cast<ThemeScope>(entry.widget.lock()))scopes.push_back(scope);
    for(auto& scope:scopes) {
        std::vector<std::shared_ptr<Widget>> work(scope->children().begin(),scope->children().end());
        while(!work.empty()) {
            auto widget=work.back();work.pop_back();
            if(std::dynamic_pointer_cast<NativeHostView>(widget))continue;
            if(std::dynamic_pointer_cast<ThemeScope>(widget))nested.insert(widget.get());
            if(auto view=std::dynamic_pointer_cast<View>(widget))for(auto& child:view->children())work.push_back(child);
        }
    }
    for(auto& scope:scopes)if(!nested.count(scope.get()))applyLocalTheme(scope,sheet_);
}
} // namespace oneui::ui
