#pragma once
#include "oneui/view.h"
#include "oneui/layout/reorderable_grid.h"
#include <stdexcept>
#include <unordered_set>

namespace oneui {
// A semantic key and one filling child. The grid owns geometry and gestures.
class TemplateGridItem final : public View {
public:
    explicit TemplateGridItem(std::shared_ptr<Widget> child) { add(std::move(child)); }
    void setKey(std::wstring key) {
        if (key.empty()) throw std::invalid_argument("GridItem requires a nonempty grid-key");
        key_ = std::move(key);
    }
    const std::wstring& key() const { return key_; }
protected:
    void layoutChildren() override {
        if (!children().empty()) children().front()->setFrame(frame());
    }
private:
    std::wstring key_;
};
inline std::vector<ReorderableGridItem> templateGridItems(
    const std::vector<std::shared_ptr<Widget>>& widgets) {
    std::vector<ReorderableGridItem> result;
    std::unordered_set<std::wstring> keys;
    for (const auto& widget : widgets) {
        auto item = std::dynamic_pointer_cast<TemplateGridItem>(widget);
        if (!item || item->key().empty() || !keys.insert(item->key()).second)
            throw std::invalid_argument("ReorderableGrid requires uniquely keyed GridItem children");
        result.push_back({item->key(), widget});
    }
    return result;
}
}
