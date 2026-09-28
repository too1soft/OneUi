#pragma once
#include "oneui/controls/tabs.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace oneui::ui {
struct TabItem {
    std::wstring id,title;
    bool operator==(const TabItem& other) const {return id==other.id && title==other.title;}
};
class KeyedTabs {
    std::shared_ptr<Tabs> tabs_;
    std::vector<TabItem> items_;
    bool updating_=false;
public:
    std::function<void(std::wstring)> selected,closed;
    std::function<std::wstring()> selectionValue;
    explicit KeyedTabs(std::shared_ptr<Tabs> tabs):tabs_(std::move(tabs)) {
        tabs_->setOnChanged([this](int index){if(!updating_ && selected)selected(key(index));});
        tabs_->setOnCloseRequested([this](int index){auto id=key(index);if(!id.empty() && closed)closed(id);});
    }
    ~KeyedTabs(){tabs_->setOnChanged({});tabs_->setOnCloseRequested({});}
    std::wstring key(int index) const {return index>=0 && index<int(items_.size())?items_[index].id:L"";}
    void select(const std::wstring& id) {
        auto found=std::find_if(items_.begin(),items_.end(),[&](const auto& item){return item.id==id;});
        updating_=true;tabs_->setSelectedIndex(found==items_.end()?-1:int(found-items_.begin()));updating_=false;
    }
    void update(const std::vector<TabItem>& items) {
        std::set<std::wstring> keys;
        for(const auto& item:items)if(item.id.empty() || !keys.insert(item.id).second)throw std::invalid_argument("Tabs require unique nonempty IDs");
        if(items==items_)return;
        auto id=selectionValue?selectionValue():key(tabs_->selectedIndex());
        if(!keys.count(id))id=items.empty()?L"":items.front().id;
        std::vector<std::wstring> labels;for(const auto& item:items)labels.push_back(item.title);
        updating_=true;items_=items;tabs_->setItems(std::move(labels));updating_=false;
        select(id);if(selected)selected(id);
    }
};
}
