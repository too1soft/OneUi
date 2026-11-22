#pragma once
// Included after StyleRegistry: release metadata together with retired widgets.
namespace oneui::ui {
inline const StyleRegistry::Entry* StyleRegistry::indexedEntry(const std::shared_ptr<Widget>& widget) {
    if (!entryIndices_) {
        EntryIndices indices;
        indices.reserve(entries_.size());
        for (std::size_t index = 0; index < entries_.size(); ++index)
            if (auto live = entries_[index].widget.lock()) indices.emplace(live.get(), index);
        entryIndices_.emplace(std::move(indices));
    }
    const auto found = entryIndices_->find(widget.get());
    if (found == entryIndices_->end()) return nullptr;
    const auto& entry = entries_[found->second];
    return entry.widget.lock() == widget ? &entry : nullptr;
}

inline void StyleRegistry::pruneEntries() {
    const auto previous = entries_.size();
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
        [](const Entry& entry) { return entry.widget.expired(); }), entries_.end());
    bool retired = previous != entries_.size();
    if (retired) entryIndices_.reset();
    for (auto it = layoutDefaults_.begin(); it != layoutDefaults_.end();) {
        if (it->second.widget.expired()) {
            // Weak references retain allocate_shared/make_shared allocations
            // after the Widget destructor. They must not outlive the entry.
            it = layoutDefaults_.erase(it);
            retired = true;
        } else ++it;
    }
    if (!retired) return;
    std::unordered_set<const Widget*> live;
    live.reserve(entries_.size());
    for (const auto& entry : entries_)
        if (auto widget = entry.widget.lock()) live.insert(widget.get());
    auto forget = [&](auto& states) {
        for (auto it = states.begin(); it != states.end();)
            if (!live.count(it->first)) it = states.erase(it);
            else ++it;
    };
    forget(queries_);
    forget(localThemes_);
    forget(themeParents_);
}
} // namespace oneui::ui
