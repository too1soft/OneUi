#include "oneui/controls/table.h"
#include "internal/widget_presentation.h"
#include <algorithm>
#include <numeric>
#include <cmath>
namespace oneui {
void Table::setColumnLayout(std::optional<std::vector<float>> widths,
                            std::optional<float> selectionWidth) {
    if (widths && (widths->empty() || std::any_of(widths->begin(), widths->end(),
            [](float width) { return !std::isfinite(width) || width < 0; }))) return;
    if (selectionWidth && (!std::isfinite(*selectionWidth) || *selectionWidth < 24 || *selectionWidth > 128)) return;
    auto& presentation = internal::WidgetPresentationAccess::table(*this, columns_);
    if (presentation.widths == widths && presentation.selectionWidth == selectionWidth) return;
    presentation.widths = std::move(widths);
    presentation.selectionWidth = selectionWidth;
    projectColumns();
}
void Table::projectColumns() {
    const auto& presentation = internal::WidgetPresentationAccess::table(*this, columns_);
    columns_ = presentation.authored;
    if (presentation.widths) {
        columns_.resize(std::min(columns_.size(), presentation.widths->size()));
        for (size_t i = 0; i < columns_.size(); ++i) columns_[i].width = (*presentation.widths)[i];
    }
    if (focusedHeader_ >= int(columns_.size())) focusedHeader_ = -1;
    if (pressedColumn_ >= int(columns_.size())) pressedColumn_ = -1;
    invalidate();
}
float Table::selectionGutter() const {
    if (!selectionColumnVisible_) return 0;
    const auto* presentation = internal::WidgetPresentationAccess::find(*this);
    return presentation && presentation->table
        ? presentation->table->selectionWidth.value_or(selectionColumnWidth_) : selectionColumnWidth_;
}
bool Table::reconcileRichRows(std::vector<std::vector<TableCell>> rows,
                              const std::vector<int>& oldToNew) {
    if (oldToNew.size() != rows_.size()) return false;
    std::vector<bool> used(rows.size());
    for (const auto index : oldToNew) {
        if (index < -1 || index >= int(rows.size())) return false;
        if (index >= 0) {
            if (used[index]) return false;
            used[index] = true;
        }
    }
    std::vector<std::vector<std::wstring>> plain;
    plain.reserve(rows.size());
    for (const auto& row : rows) {
        std::vector<std::wstring> cells;
        cells.reserve(row.size());
        for (const auto& cell : row) cells.push_back(cell.text);
        plain.push_back(std::move(cells));
    }
    clearRowProjection();
    rows_ = std::move(plain);
    richRows_ = std::move(rows);
    remapRowsState(oldToNew);
    return true;
}
void Table::clearRowProjection() {
    auto* presentation = internal::WidgetPresentationAccess::find(*this);
    if (presentation && presentation->table) presentation->table->rows.reset();
}
bool Table::projectRichRows(
        const std::vector<std::shared_ptr<const std::vector<TableCell>>>& source,
        const std::vector<std::size_t>& visible, const std::vector<int>& oldToNew) {
    if (oldToNew.size() != rows_.size()) return false;
    std::vector<bool> used(visible.size()), projected(source.size());
    for (const auto index : oldToNew) {
        if (index < -1 || index >= int(visible.size())) return false;
        if (index >= 0) {
            if (used[index]) return false;
            used[index] = true;
        }
    }
    for (const auto index : visible) {
        if (index >= source.size() || projected[index]) return false;
        projected[index] = true;
    }
    if (std::any_of(source.begin(), source.end(), [](const auto& row) { return !row; })) return false;
    auto& owner = internal::WidgetPresentationAccess::table(*this, columns_).rows;
    auto next = std::make_unique<internal::TableRowProjection>();
    next->source = source;
    next->active = visible;
    next->payloads.reserve(source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        if (owner && i < owner->source.size() && owner->source[i] == source[i]) {
            next->payloads.push_back(owner->payloads[i]);
        } else {
            auto row = std::make_shared<internal::TableRowProjection::Payload>();
            row->rich = *source[i];
            row->plain.reserve(source[i]->size());
            for (const auto& cell : *source[i]) row->plain.push_back(cell.text);
            next->payloads.push_back(std::move(row));
        }
    }
    std::vector<std::vector<TableCell>> rich;
    std::vector<std::vector<std::wstring>> plain;
    rich.reserve(visible.size());
    plain.reserve(visible.size());
    // Prepare new row storage before moving any visible cell data.
    // Active slots own no duplicate cell vectors; hiding restores their payload.
    if (owner) for (size_t i = 0; i < owner->active.size(); ++i) {
        auto& row = *owner->payloads[owner->active[i]];
        row.rich = std::move(richRows_[i]);
        row.plain = std::move(rows_[i]);
    }
    for (const auto index : visible) {
        plain.push_back(std::move(next->payloads[index]->plain));
        rich.push_back(std::move(next->payloads[index]->rich));
    }
    rows_ = std::move(plain);
    richRows_ = std::move(rich);
    owner = std::move(next);
    remapRowsState(oldToNew);
    return true;
}
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
    clearRowProjection();
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
    clearRowProjection();
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
    clearRowProjection();
    rotate(rows_); rotate(richRows_); remapRowsState(mapping); return true;
}
} // namespace oneui
