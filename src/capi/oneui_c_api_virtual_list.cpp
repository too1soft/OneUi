#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/virtual_list.h"
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

OneUiWidget* oneui_virtual_list_create() {
    return wrap(std::make_shared<oneui::VirtualList>());
}

void oneui_virtual_list_set_items_utf8(
    OneUiWidget* list,
    const OneUiListItemUtf8* items,
    std::size_t count) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setItems(listItemsFromUtf8(items, count));
    }
}

void oneui_virtual_list_set_rich_items_utf8(
    OneUiWidget* list,
    const OneUiRichListItemUtf8* items,
    std::size_t count) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setRichItems(richListItemsFromUtf8(items, count));
    }
}

int oneui_virtual_list_update_item_utf8(
    OneUiWidget* list,
    std::size_t index,
    const OneUiListItemUtf8* item) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList || !item) {
        return 0;
    }
    return nativeList->updateItem(index, oneui::ListItem{
        utf8OrEmpty(item->title),
        utf8OrEmpty(item->detail),
    }) ? 1 : 0;
}

int oneui_virtual_list_update_rich_item_utf8(
    OneUiWidget* list,
    std::size_t index,
    const OneUiRichListItemUtf8* item) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList || !item) {
        return 0;
    }
    return nativeList->updateRichItem(index, richListItemFromUtf8(*item)) ? 1 : 0;
}

void oneui_virtual_list_set_selected_index(OneUiWidget* list, int index) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setSelectedIndex(index);
    }
}

int oneui_virtual_list_selected_index(OneUiWidget* list) {
    if (const auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        return nativeList->selectedIndex();
    }
    return 0;
}

void oneui_virtual_list_set_selection_mode(OneUiWidget* list, int mode) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setSelectionMode(
            mode == 1 ? oneui::SelectionMode::Multiple : oneui::SelectionMode::Single);
    }
}

void oneui_virtual_list_set_selected_indices(
    OneUiWidget* list,
    const int* indices,
    std::size_t count) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    std::vector<int> values;
    if (indices && count > 0) {
        values.assign(indices, indices + count);
    }
    nativeList->setSelectedIndices(std::move(values));
}

std::size_t oneui_virtual_list_selected_indices(
    OneUiWidget* list,
    int* buffer,
    std::size_t buffer_len) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return 0;
    }
    const auto& values = nativeList->selectedIndices();
    if (buffer && buffer_len > 0) {
        std::copy_n(values.begin(), std::min(buffer_len, values.size()), buffer);
    }
    return values.size();
}

void oneui_virtual_list_set_row_height(OneUiWidget* list, float height) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setRowHeight(height);
    }
}

void oneui_virtual_list_set_activate_on_single_click(OneUiWidget* list, int enabled) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) nativeList->setActivateOnSingleClick(enabled != 0);
}

void oneui_virtual_list_set_rich_metrics(
    OneUiWidget* list,
    float indicator_space,
    float indicator_diameter,
    float badge_height,
    float badge_radius,
    float badge_horizontal_padding,
    float title_badge_gap,
    float trailing_width,
    float trailing_gap) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setRichMetrics(oneui::VirtualListRichMetrics{
            indicator_space,
            indicator_diameter,
            badge_height,
            badge_radius,
            badge_horizontal_padding,
            title_badge_gap,
            trailing_width,
            trailing_gap});
    }
}

int oneui_virtual_list_rich_metrics(
    OneUiWidget* list,
    OneUiVirtualListRichMetrics* out_metrics) {
    const auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList || !out_metrics) {
        return 0;
    }
    const auto& metrics = nativeList->richMetrics();
    *out_metrics = OneUiVirtualListRichMetrics{
        metrics.indicatorSpace,
        metrics.indicatorDiameter,
        metrics.badgeHeight,
        metrics.badgeRadius,
        metrics.badgeHorizontalPadding,
        metrics.titleBadgeGap,
        metrics.trailingWidth,
        metrics.trailingGap};
    return 1;
}

void oneui_virtual_list_set_scroll_offset(OneUiWidget* list, float offset) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setScrollOffset(offset);
    }
}

float oneui_virtual_list_scroll_offset(OneUiWidget* list) {
    if (const auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        return nativeList->scrollOffset();
    }
    return 0.0f;
}

float oneui_virtual_list_max_scroll_offset(OneUiWidget* list) {
    if (const auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        return nativeList->maxScrollOffset();
    }
    return 0.0f;
}

void oneui_virtual_list_set_on_changed(OneUiWidget* list, OneUiIntCallback callback, void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    if (!callback) {
        nativeList->setOnChanged(nullptr);
        return;
    }
    nativeList->setOnChanged([callback, user_data](int value) {
        callback(value, user_data);
    });
}

void oneui_virtual_list_set_on_selection_changed(
    OneUiWidget* list,
    OneUiIntArrayCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    if (!callback) {
        nativeList->setOnSelectionChanged(nullptr);
        return;
    }
    nativeList->setOnSelectionChanged([callback, user_data](const std::vector<int>& values) {
        callback(values.data(), values.size(), user_data);
    });
}

void oneui_virtual_list_set_on_activated(
    OneUiWidget* list,
    OneUiIntCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setOnActivated(callback ? std::function<void(int)>{[callback, user_data](int index) {
        callback(index, user_data);
    }} : nullptr);
}

void oneui_virtual_list_set_on_edit_requested(
    OneUiWidget* list,
    OneUiIntCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setOnEditRequested(callback ? std::function<void(int)>{[callback, user_data](int index) {
        callback(index, user_data);
    }} : nullptr);
}

void oneui_virtual_list_set_on_delete_requested(
    OneUiWidget* list,
    OneUiIntArrayCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setOnDeleteRequested(
        callback ? std::function<void(const std::vector<int>&)>{[callback, user_data](const std::vector<int>& indices) {
            callback(indices.data(), indices.size(), user_data);
        }} : nullptr);
}

void oneui_virtual_list_set_on_context_menu_requested(
    OneUiWidget* list,
    OneUiIndexPointCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setOnContextMenuRequested(
        callback ? std::function<void(int, oneui::Point)>{[callback, user_data](int index, oneui::Point point) {
            callback(index, point.x, point.y, user_data);
        }} : nullptr);
}

void oneui_virtual_list_set_reorder_enabled(OneUiWidget* list, int enabled) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setReorderEnabled(enabled != 0);
    }
}

int oneui_virtual_list_reorder_enabled(OneUiWidget* list) {
    if (const auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        return nativeList->reorderEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_virtual_list_set_on_reorder_requested(
    OneUiWidget* list,
    OneUiReorderRequestedCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setOnReorderRequested(
        callback ? std::function<void(int, int)>{[callback, user_data](int source, int target) {
            callback(source, target, user_data);
        }} : nullptr);
}

int oneui_virtual_list_set_item_drag_ids_utf8(
    OneUiWidget* list,
    const OneUiUtf8String* ids,
    std::size_t count) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList || (count > 0 && !ids)) {
        return 0;
    }
    std::vector<std::wstring> nativeIds;
    nativeIds.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        nativeIds.push_back(utf8OrEmpty(ids[index]));
    }
    return nativeList->setItemDragIds(std::move(nativeIds)) ? 1 : 0;
}

void oneui_virtual_list_set_item_drag_enabled(OneUiWidget* list, int enabled) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setItemDragEnabled(enabled != 0);
    }
}

int oneui_virtual_list_item_drag_enabled(OneUiWidget* list) {
    if (const auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        return nativeList->itemDragEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_virtual_list_set_on_item_drag_utf8(
    OneUiWidget* list,
    OneUiItemDragCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setOnItemDrag(
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
