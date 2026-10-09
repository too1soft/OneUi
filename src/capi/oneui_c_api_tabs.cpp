#include "oneui/controls/tabs.h"
#include "oneui_c_api_internal.h"
#include "oneui_c_api_widget_internal.h"
using namespace oneui::capi;
namespace {
template <class T> T *asWidget(OneUiWidget *value) {
    return value ? dynamic_cast<T *>(value->widget.get()) : nullptr;
}
} // namespace
int oneui_tabs_begin_edit(OneUiWidget *tabs, int index) {
    auto *native = asWidget<oneui::Tabs>(tabs);
    return native && native->beginEdit(index) ? 1 : 0;
}
void oneui_tabs_cancel_edit(OneUiWidget *tabs) {
    if (auto *native = asWidget<oneui::Tabs>(tabs)) {
        native->cancelEdit();
    }
}
int oneui_tabs_editing_index(OneUiWidget *tabs) {
    auto *native = asWidget<oneui::Tabs>(tabs);
    return native ? native->editingIndex() : -1;
}
void oneui_tabs_set_on_edit_finished(OneUiWidget *tabs, OneUiTabEditCallback callback,
                                     void *user_data) {
    if (auto *native = asWidget<oneui::Tabs>(tabs)) {
        native->setOnEditFinished(
            callback ? std::function<void(int, const std::wstring &,
                                          oneui::TabEditReason)>{[callback, user_data](
                                                                     int index,
                                                                     const std::wstring &value,
                                                                     oneui::TabEditReason reason) {
                const auto text = utf8FromWide(value);
                callback(index, text.data(), text.size(), static_cast<int>(reason), user_data);
            }}
                     : nullptr);
    }
}

void oneui_tabs_set_items_utf8(OneUiWidget *tabs, const OneUiUtf8String *items, std::size_t count) {
    auto *nativeTabs = asWidget<oneui::Tabs>(tabs);
    if (!nativeTabs) {
        return;
    }
    std::vector<std::wstring> values;
    if (items && count > 0) {
        values.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            values.push_back(utf8OrEmpty(items[index]));
        }
    }
    nativeTabs->setItems(std::move(values));
}

void oneui_tabs_set_item_icons(OneUiWidget *tabs, const int *symbols, std::size_t count) {
    auto *nativeTabs = asWidget<oneui::Tabs>(tabs);
    if (!nativeTabs) {
        return;
    }
    std::vector<std::optional<oneui::IconSymbol>> values;
    if (symbols && count > 0) {
        values.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            const int symbol = symbols[index];
            if (symbol < 0 || symbol > static_cast<int>(oneui::IconSymbol::OutlineWorkbenchClose)) {
                values.push_back(std::nullopt);
            } else {
                values.push_back(static_cast<oneui::IconSymbol>(symbol));
            }
        }
    }
    nativeTabs->setItemIcons(std::move(values));
}

void oneui_tabs_set_document_mode(OneUiWidget *tabs, int enabled) {
    if (auto *nativeTabs = asWidget<oneui::Tabs>(tabs))
        nativeTabs->setDocumentMode(enabled != 0);
}
int oneui_tabs_overflow_item_count(OneUiWidget *tabs) {
    auto *nativeTabs = asWidget<oneui::Tabs>(tabs);
    return nativeTabs ? nativeTabs->overflowItemCount() : 0;
}
void oneui_tabs_set_on_overflow_changed(OneUiWidget *tabs, OneUiIntCallback callback,
                                        void *user_data) {
    if (auto *nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        if (!callback)
            nativeTabs->setOnOverflowChanged(nullptr);
        else
            nativeTabs->setOnOverflowChanged(
                [callback, user_data](int n) { callback(n, user_data); });
    }
}
void oneui_tabs_set_item_status_colors(OneUiWidget *tabs, const OneUiColor *colors, size_t count) {
    if (auto *nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        std::vector<oneui::Color> values;
        if (colors) {
            for (size_t i = 0; i < count; ++i)
                values.push_back(toNativeColor(colors[i]));
        }
        nativeTabs->setItemStatusColors(std::move(values));
    }
}
void oneui_tabs_set_selected_index(OneUiWidget *tabs, int index) {
    if (auto *nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        nativeTabs->setSelectedIndex(index);
    }
}

int oneui_tabs_selected_index(OneUiWidget *tabs) {
    if (auto *nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        return nativeTabs->selectedIndex();
    }
    return 0;
}

void oneui_tabs_set_compact(OneUiWidget *tabs, int compact) {
    if (auto *nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        nativeTabs->setSizingMode(compact != 0 ? oneui::TabsSizingMode::Compact
                                               : oneui::TabsSizingMode::Equal);
    }
}

void oneui_tabs_set_item_width_range(OneUiWidget *tabs, float minimum, float maximum) {
    if (auto *nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        nativeTabs->setItemWidthRange(minimum, maximum);
    }
}

void oneui_tabs_set_closable(OneUiWidget *tabs, int closable) {
    if (auto *nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        nativeTabs->setClosable(closable != 0);
    }
}

void oneui_tabs_set_reorder_enabled(OneUiWidget *tabs, int enabled) {
    if (auto *nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        nativeTabs->setReorderEnabled(enabled != 0);
    }
}

int oneui_tabs_reorder_enabled(OneUiWidget *tabs) {
    if (auto *nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        return nativeTabs->reorderEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_tabs_set_on_changed(OneUiWidget *tabs, OneUiIntCallback callback, void *user_data) {
    auto *nativeTabs = asWidget<oneui::Tabs>(tabs);
    if (!nativeTabs) {
        return;
    }
    if (!callback) {
        nativeTabs->setOnChanged(nullptr);
        return;
    }
    nativeTabs->setOnChanged([callback, user_data](int value) { callback(value, user_data); });
}

void oneui_tabs_set_on_close_requested(OneUiWidget *tabs, OneUiIntCallback callback,
                                       void *user_data) {
    auto *nativeTabs = asWidget<oneui::Tabs>(tabs);
    if (!nativeTabs) {
        return;
    }
    if (!callback) {
        nativeTabs->setOnCloseRequested(nullptr);
        return;
    }
    nativeTabs->setOnCloseRequested(
        [callback, user_data](int value) { callback(value, user_data); });
}

void oneui_tabs_set_on_context_menu_requested(OneUiWidget *tabs, OneUiIndexPointCallback callback,
                                              void *user_data) {
    auto *nativeTabs = asWidget<oneui::Tabs>(tabs);
    if (!nativeTabs) {
        return;
    }
    nativeTabs->setOnContextMenuRequested(
        callback ? std::function<void(int, oneui::Point)>{[callback, user_data](
                                                              int index, oneui::Point point) {
            callback(index, point.x, point.y, user_data);
        }}
                 : nullptr);
}

void oneui_tabs_set_on_reorder_requested(OneUiWidget *tabs, OneUiReorderRequestedCallback callback,
                                         void *user_data) {
    auto *nativeTabs = asWidget<oneui::Tabs>(tabs);
    if (!nativeTabs) {
        return;
    }
    nativeTabs->setOnReorderRequested(
        callback ? std::function<void(int, int)>{[callback, user_data](int source, int target) {
            callback(source, target, user_data);
        }}
                 : nullptr);
}

void oneui_tabs_set_on_detach_requested(OneUiWidget *tabs, OneUiIndexPointCallback callback,
                                        void *data) {
    if (auto native = asWidget<oneui::Tabs>(tabs))
        native->setOnDetachRequested(
            callback ? std::function<void(int, oneui::Point)>{[callback, data](int index,
                                                                               oneui::Point point) {
                callback(index, point.x, point.y, data);
            }}
                     : nullptr);
}
