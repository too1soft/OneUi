#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/nav_item.h"
#include "oneui/layout/reorderable_grid.h"
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

OneUiWidget* oneui_nav_item_create(const wchar_t* text, int symbol, int selected) {
    auto widget = std::make_shared<oneui::NavItem>(
        wideOrEmpty(text),
        static_cast<oneui::IconSymbol>(symbol));
    widget->setSelected(selected != 0);
    return wrap(std::move(widget));
}

void oneui_nav_item_set_text(OneUiWidget* nav_item, const wchar_t* text) {
    auto* nativeNavItem = asWidget<oneui::NavItem>(nav_item);
    if (!nativeNavItem) {
        return;
    }
    nativeNavItem->setText(wideOrEmpty(text));
}

void oneui_nav_item_set_symbol(OneUiWidget* nav_item, int symbol) {
    auto* nativeNavItem = asWidget<oneui::NavItem>(nav_item);
    if (!nativeNavItem) {
        return;
    }
    nativeNavItem->setSymbol(static_cast<oneui::IconSymbol>(symbol));
}

void oneui_nav_item_set_selected(OneUiWidget* nav_item, int selected) {
    auto* nativeNavItem = asWidget<oneui::NavItem>(nav_item);
    if (!nativeNavItem) {
        return;
    }
    nativeNavItem->setSelected(selected != 0);
}

void oneui_nav_item_set_on_click(OneUiWidget* nav_item, OneUiVoidCallback callback, void* user_data) {
    auto* nativeNavItem = asWidget<oneui::NavItem>(nav_item);
    if (!nativeNavItem) {
        return;
    }
    if (!callback) {
        nativeNavItem->setOnClick(nullptr);
        return;
    }
    nativeNavItem->setOnClick([callback, user_data] {
        callback(user_data);
    });
}

OneUiWidget* oneui_reorderable_grid_create(void) {
    return wrap(std::make_shared<oneui::ReorderableGrid>());
}

void oneui_reorderable_grid_clear_items(OneUiWidget* grid) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->clearItems();
    }
}

void oneui_reorderable_grid_add_item_utf8(
    OneUiWidget* grid,
    OneUiUtf8String id,
    OneUiWidget* child) {
    auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid);
    if (!nativeGrid || !child || !child->widget) {
        return;
    }
    nativeGrid->addItem(oneui::ReorderableGridItem{
        utf8OrEmpty(id), child->widget});
}

int oneui_reorderable_grid_move_item_utf8(
    OneUiWidget* grid,
    OneUiUtf8String source_id,
    int target_index) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        return nativeGrid->moveItem(utf8OrEmpty(source_id), target_index) ? 1 : 0;
    }
    return 0;
}

void oneui_reorderable_grid_set_column_count(OneUiWidget* grid, int columns) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->setColumnCount(columns);
    }
}

void oneui_reorderable_grid_set_gaps(
    OneUiWidget* grid,
    float column_gap,
    float row_gap) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->setColumnGap(column_gap);
        nativeGrid->setRowGap(row_gap);
    }
}

void oneui_reorderable_grid_set_item_height(OneUiWidget* grid, float height) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->setItemHeight(height);
    }
}

float oneui_reorderable_grid_content_height(OneUiWidget* grid) {
    if (const auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        return nativeGrid->contentHeight();
    }
    return 0.0f;
}

void oneui_reorderable_grid_set_reorder_enabled(OneUiWidget* grid, int enabled) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->setReorderEnabled(enabled != 0);
    }
}

int oneui_reorderable_grid_reorder_enabled(OneUiWidget* grid) {
    if (const auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        return nativeGrid->reorderEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_reorderable_grid_set_on_reorder_requested_utf8(
    OneUiWidget* grid,
    OneUiGridReorderRequestedCallback callback,
    void* user_data) {
    auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid);
    if (!nativeGrid) {
        return;
    }
    nativeGrid->setOnReorderRequested(
        callback
            ? std::function<void(const std::wstring&, int)>{
                [callback, user_data](const std::wstring& source, int target) {
                    const std::string sourceUtf8 = utf8FromWide(source);
                    callback(
                        sourceUtf8.data(), sourceUtf8.size(), target, user_data);
                }}
            : nullptr);
}

void oneui_reorderable_grid_set_item_drag_enabled(OneUiWidget* grid, int enabled) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->setItemDragEnabled(enabled != 0);
    }
}

int oneui_reorderable_grid_item_drag_enabled(OneUiWidget* grid) {
    if (const auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        return nativeGrid->itemDragEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_reorderable_grid_set_on_item_drag_utf8(
    OneUiWidget* grid,
    OneUiItemDragCallback callback,
    void* user_data) {
    auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid);
    if (!nativeGrid) {
        return;
    }
    nativeGrid->setOnItemDrag(
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
