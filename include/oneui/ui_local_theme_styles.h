#pragma once
namespace oneui::ui {
inline void StyleRegistry::applyLocalTheme(const std::shared_ptr<ThemeScope>& scope,
                                           const std::shared_ptr<StyleSheet>& parent) {
    const oneui::detail::view_mount_timing::Scope measure(oneui::detail::view_mount_timing::ThemeReplay);
    if (parent) themeParents_[scope.get()] = parent;
    else themeParents_.erase(scope.get());
    // An empty scope inherits the actual parent palette. A child already on
    // that palette needs no replay, including during bottom-up construction.
    const auto sheet = scope->tokens().empty() ? parent :
        std::make_shared<StyleSheet>(scope->merge(parent ? *parent : sheet_));
    const std::weak_ptr<int> lifetime = queryLifetime_;
    const std::weak_ptr<ThemeScope> weak = scope;
    scope->setOnTokensChanged([this, lifetime, weak] {
        if (lifetime.expired()) return;
        if (auto scope = weak.lock()) {
            const auto found = themeParents_.find(scope.get());
            const auto inherited = found == themeParents_.end() ? nullptr : found->second;
            applyLocalTheme(scope, inherited);
        }
    });
    // Preview galleries have many descendants. Index this immutable replay
    // once, rather than scanning all registered siblings for every child.
    std::optional<std::unordered_map<const Widget*, const Entry*>> entries;
    std::vector<std::shared_ptr<Widget>> work(scope->children().begin(),scope->children().end());
    while(!work.empty()) {
        auto widget=work.back();work.pop_back();
        // NativeHost terminal palettes are deliberately outside this contract.
        if(std::dynamic_pointer_cast<NativeHostView>(widget))continue;
        if(auto nested=std::dynamic_pointer_cast<ThemeScope>(widget)){applyLocalTheme(nested,sheet);continue;}
        const auto previous = localThemes_.find(widget.get());
        if (sheet != (previous == localThemes_.end() ? nullptr : previous->second)) {
            if (!entries) {
                entries.emplace();
                entries->reserve(entries_.size());
                for (const auto& entry : entries_)
                    if (auto live = entry.widget.lock()) entries->emplace(live.get(), &entry);
            }
            if (auto found = entries->find(widget.get()); found != entries->end()) {
                if (sheet) localThemes_[widget.get()] = sheet;
                else localThemes_.erase(previous);
                apply(*found->second, sheet_);
            }
        }
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
    for(auto& scope:scopes)if(!nested.count(scope.get()))applyLocalTheme(scope);
}
} // namespace oneui::ui
