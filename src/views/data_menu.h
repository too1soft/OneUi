#pragma once
#include "oneui/controls/menu.h"
#include "oneui/view.h"
#include "oneui/ui_icon_names.h"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace oneui::views {
// Private implementation: public authoring uses records and stable command IDs.
struct DataMenuEntry {
    std::string id, parent;
    std::wstring text, shortcut;
    std::optional<IconSymbol> icon;
    bool disabled = false, checked = false, danger = false, separator = false, header = false;
    bool operator==(const DataMenuEntry& other) const {
        return std::tie(id, parent, text, shortcut, icon, disabled, checked, danger, separator, header)
            == std::tie(other.id, other.parent, other.text, other.shortcut, other.icon,
                        other.disabled, other.checked, other.danger, other.separator, other.header);
    }
};

class DataMenu final : public View {
public:
    DataMenu() : menu_(std::make_shared<Menu>()) {
        add(menu_);
        menu_->setOnItemActivated([this](int index) { activate(index); });
    }
    void setTitle(std::wstring value) { if (title_ != value) { title_ = std::move(value); rebuild(); } }
    void setSubtitle(std::wstring value) { if (subtitle_ != value) { subtitle_ = std::move(value); rebuild(); } }
    void setOnActivated(std::function<void(std::string)> action) { action_ = std::move(action); }
    static void validate(const std::vector<DataMenuEntry>& entries) {
        if (entries.size() > 10000) throw std::invalid_argument("Menu item limit");
        std::map<std::string, const DataMenuEntry*> ids;
        for (const auto& entry : entries)
            if (entry.id.empty() || !ids.emplace(entry.id, &entry).second)
                throw std::invalid_argument("Menu IDs must be unique and nonempty");
        for (const auto& entry : entries) {
            auto parent = entry.parent;
            size_t depth = 1;
            while (!parent.empty()) {
                auto found = ids.find(parent);
                if (found == ids.end() || found->second->separator || found->second->header || ++depth > 64)
                    throw std::invalid_argument("Invalid menu parent/cycle/depth");
                parent = found->second->parent;
            }
            if (entry.header && !entry.parent.empty())
                throw std::invalid_argument("Menu headers stay at the top level");
        }
    }
    void setItems(std::vector<DataMenuEntry> entries) {
        validate(entries);
        // A binding transaction can replay an unchanged snapshot after input.
        // Preserve scrolling and submenu navigation only for identical content.
        if (entries_ == entries) return;
        entries_ = std::move(entries);
        path_.clear();
        rebuild();
    }
    Size naturalSize() const override { return {224, menu_->preferredHeight()}; }
    bool isFocusable() const override { return true; }
    bool onFocusChanged(bool focused) override {
        View::onFocusChanged(focused);
        return menu_->onFocusChanged(focused);
    }
    bool onKeyDown(const KeyEvent& event) override {
        if (!interactive()) return false;
        if (event.key == Key::Left && !path_.empty()) {
            path_.pop_back(); rebuild(); return true;
        }
        auto forwarded = event;
        navigateOnly_ = event.key == Key::Right;
        if (navigateOnly_) forwarded.key = Key::Enter;
        // Escape is intentionally left to the containing light-dismiss popup.
        const auto handled = menu_->onKeyDown(forwarded);
        navigateOnly_ = false;
        return handled;
    }
protected:
    void layoutChildren() override { menu_->setFrame(frame()); }
private:
    bool hasChildren(const std::string& id) const {
        return std::any_of(entries_.begin(), entries_.end(), [&](const auto& item) { return item.parent == id; });
    }
    void rebuild() {
        menu_->clearItems();
        visible_.clear();
        if (path_.empty()) {
            if (!title_.empty() || !subtitle_.empty()) menu_->addHeader(title_, subtitle_);
        } else {
            const auto& parent = entries_[path_.back()];
            menu_->addItem(L"\u2039 " + parent.text);
            visible_.push_back(-1);
        }
        const auto parent = path_.empty() ? std::string{} : entries_[path_.back()].id;
        for (size_t i = 0; i < entries_.size(); ++i) {
            const auto& entry = entries_[i];
            if (entry.parent != parent) continue;
            if (entry.separator) { menu_->addSeparator(); continue; }
            if (entry.header) { menu_->addHeader(entry.text, L""); continue; }
            auto label = entry.checked ? L"\u2713 " + entry.text : entry.text;
            if (!entry.shortcut.empty()) label += L"    " + entry.shortcut;
            if (hasChildren(entry.id)) label += L"  \u203a";
            const int index = menu_->addItem(label, entry.icon, entry.danger);
            menu_->setItemDisabled(index, entry.disabled);
            visible_.push_back(int(i));
        }
        invalidate();
    }
    void activate(int index) {
        if (index < 0 || size_t(index) >= visible_.size()) return;
        const auto entryIndex = visible_[size_t(index)];
        if (entryIndex < 0) { path_.pop_back(); rebuild(); return; }
        const auto& entry = entries_[size_t(entryIndex)];
        if (entry.disabled || entry.separator) return;
        if (hasChildren(entry.id)) { path_.push_back(size_t(entryIndex)); rebuild(); return; }
        if (navigateOnly_) return;
        auto callback = action_;
        auto id = entry.id;
        if (callback) callback(std::move(id));
    }
    std::shared_ptr<Menu> menu_;
    std::vector<DataMenuEntry> entries_;
    std::vector<int> visible_;
    std::vector<size_t> path_;
    std::wstring title_, subtitle_;
    std::function<void(std::string)> action_;
    bool navigateOnly_ = false;
};
} // namespace oneui::views
