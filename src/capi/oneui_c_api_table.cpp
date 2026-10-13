#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/table.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <cwchar>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace oneui::capi;

namespace {

std::vector<std::wstring> splitWideBy(const std::wstring& text, wchar_t delimiter) {
    std::vector<std::wstring> result;
    std::wstringstream stream(text);
    std::wstring part;
    while (std::getline(stream, part, delimiter)) {
        result.push_back(part);
    }
    return result;
}

std::vector<oneui::TableColumn> splitWideTableColumns(const wchar_t* text) {
    std::vector<oneui::TableColumn> result;
    for (const auto& row : splitWideItems(text)) {
        const auto parts = splitWideBy(row, L'\t');
        if (parts.empty() || parts[0].empty()) {
            continue;
        }
        oneui::TableColumn column;
        column.header = parts[0];
        if (parts.size() > 1 && !parts[1].empty()) {
            wchar_t* end = nullptr;
            const float width = std::wcstof(parts[1].c_str(), &end);
            if (end != parts[1].c_str()) {
                column.width = width;
            }
        }
        result.push_back(std::move(column));
    }
    return result;
}

std::vector<std::vector<std::wstring>> splitWideTableRows(const wchar_t* text) {
    std::vector<std::vector<std::wstring>> result;
    if (!text) {
        return result;
    }
    std::wstringstream stream(text);
    std::wstring row;
    while (std::getline(stream, row, L'\n')) {
        if (!row.empty()) {
            result.push_back(splitWideBy(row, L'\t'));
        }
    }
    return result;
}

static std::vector<oneui::TableCell> copyRichTableRow(const OneUiTableRichRowUtf8& row) {
    std::vector<oneui::TableCell> copy;
    if (!row.cells) return copy;
    for (size_t i = 0; i < row.count; ++i) {
        const auto& source = row.cells[i]; oneui::TableCell cell;
        cell.text = utf8OrEmpty(source.text); cell.detail = utf8OrEmpty(source.detail); cell.badge = utf8OrEmpty(source.badge);
        cell.alignment = source.alignment == 2 ? oneui::TextAlign::Right : source.alignment == 1 ? oneui::TextAlign::Center : oneui::TextAlign::Left;
        if (source.icon >= 0 && source.icon <= static_cast<int>(oneui::IconSymbol::OutlineWorkbenchBroadcast)) cell.leadingIcon = static_cast<oneui::IconSymbol>(source.icon);
        if (source.foreground.a) cell.foreground = toNativeColor(source.foreground);
        if (source.badge_color_flags & 1u) cell.badgeForeground = toNativeColor(source.badge_foreground);
        if (source.badge_color_flags & 2u) cell.badgeBackground = toNativeColor(source.badge_background);
        cell.iconSize = 14.0f; cell.fontSize = source.font_size; cell.fontWeight = source.font_weight;
        copy.push_back(std::move(cell));
    }
    return copy;
}

} // namespace

void oneui_table_set_rich_rows_utf8(OneUiWidget* table, const OneUiTableRichRowUtf8* rows, size_t count) {
    auto* native = asWidget<oneui::Table>(table);
    if (!native || (!rows && count)) return;
    std::vector<std::vector<oneui::TableCell>> copy;
    for (size_t i = 0; i < count; ++i) copy.push_back(copyRichTableRow(rows[i]));
    native->setRichRows(std::move(copy));
}

int oneui_table_update_rich_row_utf8(OneUiWidget* table, size_t index, const OneUiTableRichRowUtf8* row) {
    auto* native = asWidget<oneui::Table>(table);
    return native && row && native->updateRichRow(index, copyRichTableRow(*row));
}

void oneui_table_set_column_presentation(OneUiWidget* table, int column, int alignment, int action) {
    if (auto* native = asWidget<oneui::Table>(table)) native->setColumnPresentation(column,
        alignment == 2 ? oneui::TextAlign::Right : alignment == 1 ? oneui::TextAlign::Center : oneui::TextAlign::Left, action != 0);
}

void oneui_table_set_column_dividers_visible(OneUiWidget* table, int visible) {
    if (auto* value = asWidget<oneui::Table>(table)) value->setColumnDividersVisible(visible != 0);
}

void oneui_table_set_column_kind(OneUiWidget* table, int column, int kind) {
    if (kind != 0 && kind != 1) return;
    if (auto* native = asWidget<oneui::Table>(table)) {
        native->setColumnKind(column, kind == 1 ? oneui::TableCellKind::Percent : oneui::TableCellKind::Text);
    }
}

void oneui_table_set_selection_column_visible(OneUiWidget* table, int visible) {
    if (auto* value = asWidget<oneui::Table>(table)) value->setSelectionColumnVisible(visible != 0);
}

void oneui_table_set_selection_column_width(OneUiWidget* table, float width) {
    if (auto* value = asWidget<oneui::Table>(table)) value->setSelectionColumnWidth(width);
}

void oneui_table_set_header_sort(OneUiWidget* table, int column, int descending) {
    if (auto* native = asWidget<oneui::Table>(table)) native->setHeaderSort(column, descending != 0);
}

void oneui_table_set_header_height(OneUiWidget* table, float height) {
    if (auto* native = asWidget<oneui::Table>(table)) native->setHeaderHeight(height);
}

void oneui_table_set_on_cell_action(OneUiWidget* table, OneUiReorderRequestedCallback callback, void* user_data) {
    if (auto* native = asWidget<oneui::Table>(table)) {
        if (!callback) native->setOnCellAction(nullptr);
        else native->setOnCellAction([callback,user_data](int row, int column) { callback(row,column,user_data); });
    }
}

OneUiWidget* oneui_table_create() {
    return wrap(std::make_shared<oneui::Table>());
}

void oneui_table_set_columns(OneUiWidget* table, const wchar_t* columns) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) {
        return;
    }
    nativeTable->setColumns(splitWideTableColumns(columns));
}

void oneui_table_set_rows(OneUiWidget* table, const wchar_t* rows) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) {
        return;
    }
    nativeTable->setRows(splitWideTableRows(rows));
}

void oneui_table_set_columns_utf8(
    OneUiWidget* table,
    const OneUiTableColumnUtf8* columns,
    std::size_t count) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    std::vector<oneui::TableColumn> values;
    values.reserve(count);
    if (columns) {
        for (std::size_t index = 0; index < count; ++index) {
            values.push_back(oneui::TableColumn{
                utf8OrEmpty(columns[index].header),
                std::max(0.0f, columns[index].width)});
        }
    }
    nativeTable->setColumns(std::move(values));
}

void oneui_table_set_rows_utf8(
    OneUiWidget* table,
    const OneUiTableRowUtf8* rows,
    std::size_t count) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    std::vector<std::vector<std::wstring>> values;
    values.reserve(count);
    if (rows) {
        for (std::size_t rowIndex = 0; rowIndex < count; ++rowIndex) {
            std::vector<std::wstring> cells;
            const auto& row = rows[rowIndex];
            cells.reserve(row.cell_count);
            if (row.cells) {
                for (std::size_t cellIndex = 0; cellIndex < row.cell_count; ++cellIndex) {
                    cells.push_back(utf8OrEmpty(row.cells[cellIndex]));
                }
            }
            values.push_back(std::move(cells));
        }
    }
    nativeTable->setRows(std::move(values));
}

int oneui_table_update_row_utf8(
    OneUiWidget* table,
    std::size_t index,
    const OneUiTableRowUtf8* row) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable || !row) return 0;
    std::vector<std::wstring> cells;
    cells.reserve(row->cell_count);
    if (row->cells) {
        for (std::size_t cellIndex = 0; cellIndex < row->cell_count; ++cellIndex) {
            cells.push_back(utf8OrEmpty(row->cells[cellIndex]));
        }
    }
    return nativeTable->updateRow(index, std::move(cells)) ? 1 : 0;
}

void oneui_table_set_selection_mode(OneUiWidget* table, int mode) {
    if (auto* nativeTable = asWidget<oneui::Table>(table)) {
        nativeTable->setSelectionMode(
            mode == 1 ? oneui::SelectionMode::Multiple : oneui::SelectionMode::Single);
    }
}

void oneui_table_set_selected_index(OneUiWidget* table, int index) {
    if (auto* nativeTable = asWidget<oneui::Table>(table)) nativeTable->setSelectedIndex(index);
}

int oneui_table_selected_index(OneUiWidget* table) {
    const auto* nativeTable = asWidget<oneui::Table>(table);
    return nativeTable ? nativeTable->selectedIndex() : -1;
}

void oneui_table_set_selected_indices(
    OneUiWidget* table,
    const int* indices,
    std::size_t count) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    std::vector<int> values;
    if (indices && count > 0) values.assign(indices, indices + count);
    nativeTable->setSelectedIndices(std::move(values));
}

std::size_t oneui_table_selected_indices(
    OneUiWidget* table,
    int* buffer,
    std::size_t buffer_len) {
    const auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return 0;
    const auto& values = nativeTable->selectedIndices();
    if (buffer && buffer_len > 0) {
        std::copy_n(values.begin(), std::min(buffer_len, values.size()), buffer);
    }
    return values.size();
}

void oneui_table_set_row_height(OneUiWidget* table, float height) {
    if (auto* nativeTable = asWidget<oneui::Table>(table)) nativeTable->setRowHeight(height);
}

void oneui_table_set_scroll_offset(OneUiWidget* table, float offset) {
    if (auto* nativeTable = asWidget<oneui::Table>(table)) nativeTable->setScrollOffset(offset);
}

float oneui_table_scroll_offset(OneUiWidget* table) {
    const auto* nativeTable = asWidget<oneui::Table>(table);
    return nativeTable ? nativeTable->scrollOffset() : 0.0f;
}

float oneui_table_max_scroll_offset(OneUiWidget* table) {
    const auto* nativeTable = asWidget<oneui::Table>(table);
    return nativeTable ? nativeTable->maxScrollOffset() : 0.0f;
}

void oneui_table_set_on_changed(OneUiWidget* table, OneUiIntCallback callback, void* user_data) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    nativeTable->setOnChanged(callback ? std::function<void(int)>{[callback, user_data](int index) {
        callback(index, user_data);
    }} : nullptr);
}

void oneui_table_set_on_selection_changed(
    OneUiWidget* table,
    OneUiIntArrayCallback callback,
    void* user_data) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    nativeTable->setOnSelectionChanged(
        callback ? std::function<void(const std::vector<int>&)>{[callback, user_data](const std::vector<int>& values) {
            callback(values.data(), values.size(), user_data);
        }} : nullptr);
}

void oneui_table_set_on_activated(OneUiWidget* table, OneUiIntCallback callback, void* user_data) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    nativeTable->setOnActivated(callback ? std::function<void(int)>{[callback, user_data](int index) {
        callback(index, user_data);
    }} : nullptr);
}

void oneui_table_set_on_edit_requested(OneUiWidget* table, OneUiIntCallback callback, void* user_data) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    nativeTable->setOnEditRequested(callback ? std::function<void(int)>{[callback, user_data](int index) {
        callback(index, user_data);
    }} : nullptr);
}

void oneui_table_set_on_delete_requested(
    OneUiWidget* table,
    OneUiIntArrayCallback callback,
    void* user_data) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    nativeTable->setOnDeleteRequested(
        callback ? std::function<void(const std::vector<int>&)>{[callback, user_data](const std::vector<int>& values) {
            callback(values.data(), values.size(), user_data);
        }} : nullptr);
}

void oneui_table_set_on_context_menu_requested(
    OneUiWidget* table,
    OneUiIndexPointCallback callback,
    void* user_data) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    nativeTable->setOnContextMenuRequested(
        callback ? std::function<void(int, oneui::Point)>{[callback, user_data](int index, oneui::Point point) {
            callback(index, point.x, point.y, user_data);
        }} : nullptr);
}

void oneui_table_set_reorder_enabled(OneUiWidget* table, int enabled) {
    if (auto* nativeTable = asWidget<oneui::Table>(table)) {
        nativeTable->setReorderEnabled(enabled != 0);
    }
}

int oneui_table_reorder_enabled(OneUiWidget* table) {
    if (const auto* nativeTable = asWidget<oneui::Table>(table)) {
        return nativeTable->reorderEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_table_set_on_reorder_requested(
    OneUiWidget* table,
    OneUiReorderRequestedCallback callback,
    void* user_data) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    nativeTable->setOnReorderRequested(
        callback ? std::function<void(int, int)>{[callback, user_data](int source, int target) {
            callback(source, target, user_data);
        }} : nullptr);
}

int oneui_table_set_item_drag_ids_utf8(
    OneUiWidget* table,
    const OneUiUtf8String* ids,
    std::size_t count) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable || (count > 0 && !ids)) return 0;
    std::vector<std::wstring> nativeIds;
    nativeIds.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        nativeIds.push_back(utf8OrEmpty(ids[index]));
    }
    return nativeTable->setItemDragIds(std::move(nativeIds)) ? 1 : 0;
}

void oneui_table_set_item_drag_enabled(OneUiWidget* table, int enabled) {
    if (auto* nativeTable = asWidget<oneui::Table>(table)) {
        nativeTable->setItemDragEnabled(enabled != 0);
    }
}

int oneui_table_item_drag_enabled(OneUiWidget* table) {
    if (const auto* nativeTable = asWidget<oneui::Table>(table)) {
        return nativeTable->itemDragEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_table_set_on_item_drag_utf8(
    OneUiWidget* table,
    OneUiItemDragCallback callback,
    void* user_data) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) return;
    nativeTable->setOnItemDrag(
        callback
            ? std::function<void(const oneui::ItemDragEvent&)>{
                [callback, user_data](const oneui::ItemDragEvent& event) {
                    const std::string sourceUtf8 = utf8FromWide(event.sourceId);
                    callback(
                        sourceUtf8.data(),
                        sourceUtf8.size(),
                        static_cast<int>(event.phase),
                        event.position.x,
                        event.position.y,
                        user_data);
                }}
            : nullptr);
}
