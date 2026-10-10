#include "oneui/controls/table.h"
#include <algorithm>
#include <numeric>
namespace oneui {
void Table::remapRowsState(const std::vector<int>& oldToNew) {
    const auto previous=selection_.selectedIndices();
    const auto selected=selectedIndex();
    auto remap=[&](int i) { return i>=0 && i<int(oldToNew.size()) ? oldToNew[i] : -1; };
    selection_.remapItems(int(rows_.size()),oldToNew);
    hoveredIndex_=remap(hoveredIndex_); pressedIndex_=remap(pressedIndex_);
    // Structural changes invalidate a pending drag target. Report cancellation
    // while its original ID is still available, as setRows does.
    const int source=reorderSourceIndex_;
    const bool cancel=externalDragging_ && itemDragEnabled_ && onItemDrag_ &&
                      source>=0 && source<int(itemDragIds_.size());
    const ItemDragEvent event{cancel ? itemDragIds_[source] : L"",ItemDragPhase::Cancelled,reorderCurrentPoint_};
    const auto callback=onItemDrag_;
    resetReorderState(); itemDragIds_.clear();
    scrollOffset_=std::clamp(scrollOffset_,0.0f,maxScrollOffset());
    resetScrollMotion(scrollOffset_);
    notifySelectionChanged(previous,selected); invalidate();
    if (cancel) callback(event);
}
bool Table::insertRows(size_t index,std::vector<std::vector<std::wstring>> rows) {
    if (index>rows_.size()) return false;
    if (rows.empty()) return true;
    std::vector<std::vector<TableCell>> rich;
    for (const auto& row : rows) {
        std::vector<TableCell> cells;
        for (const auto& text : row) cells.push_back(TableCell{text});
        rich.push_back(std::move(cells));
    }
    std::vector<int> mapping(rows_.size());
    for(size_t i=0;i<mapping.size();++i) mapping[i]=int(i<index ? i : i+rows.size());
    rows_.reserve(rows_.size()+rows.size()); richRows_.reserve(richRows_.size()+rich.size());
    rows_.insert(rows_.begin()+index,std::make_move_iterator(rows.begin()),std::make_move_iterator(rows.end()));
    richRows_.insert(richRows_.begin()+index,std::make_move_iterator(rich.begin()),std::make_move_iterator(rich.end()));
    remapRowsState(mapping); return true;
}
bool Table::eraseRows(size_t index,size_t count) {
    if (index>rows_.size() || count>rows_.size()-index) return false;
    if (!count) return true;
    std::vector<int> mapping(rows_.size());
    for(size_t i=0;i<mapping.size();++i)
        mapping[i]=i<index ? int(i) : i<index+count ? -1 : int(i-count);
    rows_.erase(rows_.begin()+index,rows_.begin()+index+count);
    richRows_.erase(richRows_.begin()+index,richRows_.begin()+index+count);
    remapRowsState(mapping); return true;
}
bool Table::moveRows(size_t index,size_t count,size_t to) {
    if(index>rows_.size() || count>rows_.size()-index || to>rows_.size()-count) return false;
    if (!count || to==index) return true;
    auto rotate=[&](auto& rows) {
        if (to<index) std::rotate(rows.begin()+to,rows.begin()+index,rows.begin()+index+count);
        else std::rotate(rows.begin()+index,rows.begin()+index+count,rows.begin()+to+count);
    };
    std::vector<int> order(rows_.size()),mapping(rows_.size());
    std::iota(order.begin(),order.end(),0); rotate(order);
    for(size_t i=0;i<order.size();++i) mapping[order[i]]=int(i);
    rotate(rows_); rotate(richRows_); remapRowsState(mapping); return true;
}
} // namespace oneui
