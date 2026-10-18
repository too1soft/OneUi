#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/tree_view.h"
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

std::vector<oneui::TreeItem> treeItemsFromUtf8(const OneUiTreeItemUtf8* items, std::size_t count) {
    std::vector<oneui::TreeItem> result;
    if (!items || count == 0) {
        return result;
    }

    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        oneui::TreeItem item;
        item.id = utf8OrEmpty(items[index].id);
        item.parentId = utf8OrEmpty(items[index].parent_id);
        item.title = utf8OrEmpty(items[index].title);
        item.detail = utf8OrEmpty(items[index].detail);
        item.expanded = items[index].expanded != 0;
        result.push_back(std::move(item));
    }
    return result;
}

} // namespace

OneUiWidget* oneui_tree_view_create() {
    return wrap(std::make_shared<oneui::TreeView>());
}

void oneui_tree_view_set_items_utf8(
    OneUiWidget* tree_view,
    const OneUiTreeItemUtf8* items,
    std::size_t count) {
    if (auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        nativeTree->setItems(treeItemsFromUtf8(items, count));
    }
}

void oneui_tree_view_set_selected_id_utf8(OneUiWidget* tree_view, OneUiUtf8String id) {
    if (auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        nativeTree->setSelectedId(utf8OrEmpty(id));
    }
}

float oneui_tree_view_content_height(OneUiWidget* tree_view) {
    if (const auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        return nativeTree->contentHeight();
    }
    return 0.0f;
}

std::size_t oneui_tree_view_selected_id_utf8(OneUiWidget* tree_view, char* buffer, std::size_t buffer_len) {
    const auto* nativeTree = asWidget<oneui::TreeView>(tree_view);
    if (!nativeTree) {
        return 0;
    }
    const std::string id = utf8FromWide(nativeTree->selectedId());
    copyUtf8Field(id, buffer, buffer_len);
    return id.size() + 1;
}

void oneui_tree_view_update_external_drop_target(
    OneUiWidget* tree_view,
    float x,
    float y) {
    if (auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        nativeTree->updateExternalDropTarget(oneui::Point{x, y});
    }
}

void oneui_tree_view_clear_external_drop_target(OneUiWidget* tree_view) {
    if (auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        nativeTree->clearExternalDropTarget();
    }
}

std::size_t oneui_tree_view_external_drop_target_id_utf8(
    OneUiWidget* tree_view,
    char* buffer,
    std::size_t buffer_len) {
    const auto* nativeTree = asWidget<oneui::TreeView>(tree_view);
    if (!nativeTree) {
        return 0;
    }
    const std::string id = utf8FromWide(nativeTree->externalDropTargetId());
    copyUtf8Field(id, buffer, buffer_len);
    return id.size() + 1;
}

void oneui_tree_view_set_on_selection_changed_utf8(
    OneUiWidget* tree_view,
    OneUiUtf8TextCallback callback,
    void* user_data) {
    auto* nativeTree = asWidget<oneui::TreeView>(tree_view);
    if (!nativeTree) {
        return;
    }
    if (!callback) {
        nativeTree->setOnSelectionChanged(nullptr);
        return;
    }
    nativeTree->setOnSelectionChanged([callback, user_data](const std::wstring& id) {
        const std::string utf8 = utf8FromWide(id);
        callback(utf8.data(), utf8.size(), user_data);
    });
}

void oneui_tree_view_set_on_expansion_changed_utf8(
    OneUiWidget* tree_view,
    OneUiTreeExpansionCallback callback,
    void* user_data) {
    auto* nativeTree = asWidget<oneui::TreeView>(tree_view);
    if (!nativeTree) {
        return;
    }
    if (!callback) {
        nativeTree->setOnExpansionChanged(nullptr);
        return;
    }
    nativeTree->setOnExpansionChanged([callback, user_data](const std::wstring& id, bool expanded) {
        const std::string utf8 = utf8FromWide(id);
        callback(utf8.data(), utf8.size(), expanded ? 1 : 0, user_data);
    });
}

void oneui_tree_view_set_reorder_enabled(OneUiWidget* tree_view, int enabled) {
    if (auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        nativeTree->setReorderEnabled(enabled != 0);
    }
}

int oneui_tree_view_reorder_enabled(OneUiWidget* tree_view) {
    if (const auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        return nativeTree->reorderEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_tree_view_set_on_reorder_requested_utf8(
    OneUiWidget* tree_view,
    OneUiTreeReorderRequestedCallback callback,
    void* user_data) {
    auto* nativeTree = asWidget<oneui::TreeView>(tree_view);
    if (!nativeTree) {
        return;
    }
    nativeTree->setOnReorderRequested(
        callback
            ? std::function<void(const std::wstring&, const std::wstring&)>{
                [callback, user_data](const std::wstring& source, const std::wstring& target) {
                    const std::string sourceUtf8 = utf8FromWide(source);
                    const std::string targetUtf8 = utf8FromWide(target);
                    callback(
                        sourceUtf8.data(),
                        sourceUtf8.size(),
                        targetUtf8.data(),
                        targetUtf8.size(),
                        user_data);
                }}
            : nullptr);
}
