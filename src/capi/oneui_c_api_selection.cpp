#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/checkbox.h"
#include "oneui/controls/list.h"
#include "oneui/controls/radio_group.h"
#include "oneui/controls/select.h"
#include "oneui/controls/switch.h"
#include "oneui/controls/tabs.h"
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

OneUiWidget* oneui_switch_create(const wchar_t* text) {
    return wrap(std::make_shared<oneui::Switch>(wideOrEmpty(text)));
}

void oneui_switch_set_text(OneUiWidget* switch_widget, const wchar_t* text) {
    auto* nativeSwitch = asWidget<oneui::Switch>(switch_widget);
    if (!nativeSwitch) {
        return;
    }
    nativeSwitch->setText(wideOrEmpty(text));
}

void oneui_switch_set_checked(OneUiWidget* switch_widget, int checked) {
    auto* nativeSwitch = asWidget<oneui::Switch>(switch_widget);
    if (!nativeSwitch) {
        return;
    }
    nativeSwitch->setChecked(checked != 0);
}

int oneui_switch_checked(OneUiWidget* switch_widget) {
    auto* nativeSwitch = asWidget<oneui::Switch>(switch_widget);
    if (!nativeSwitch) {
        return 0;
    }
    return nativeSwitch->checked() ? 1 : 0;
}

void oneui_switch_set_on_changed(OneUiWidget* switch_widget, OneUiBoolCallback callback, void* user_data) {
    auto* nativeSwitch = asWidget<oneui::Switch>(switch_widget);
    if (!nativeSwitch) {
        return;
    }
    if (!callback) {
        nativeSwitch->setOnChanged(nullptr);
        return;
    }
    nativeSwitch->setOnChanged([callback, user_data](bool checked) {
        callback(checked ? 1 : 0, user_data);
    });
}

OneUiWidget* oneui_checkbox_create(const wchar_t* text) {
    return wrap(std::make_shared<oneui::Checkbox>(wideOrEmpty(text)));
}

void oneui_checkbox_set_text(OneUiWidget* checkbox, const wchar_t* text) {
    auto* nativeCheckbox = asWidget<oneui::Checkbox>(checkbox);
    if (!nativeCheckbox) {
        return;
    }
    nativeCheckbox->setText(wideOrEmpty(text));
}

void oneui_checkbox_set_checked(OneUiWidget* checkbox, int checked) {
    auto* nativeCheckbox = asWidget<oneui::Checkbox>(checkbox);
    if (!nativeCheckbox) {
        return;
    }
    nativeCheckbox->setChecked(checked != 0);
}

int oneui_checkbox_checked(OneUiWidget* checkbox) {
    auto* nativeCheckbox = asWidget<oneui::Checkbox>(checkbox);
    if (!nativeCheckbox) {
        return 0;
    }
    return nativeCheckbox->checked() ? 1 : 0;
}

void oneui_checkbox_set_on_changed(OneUiWidget* checkbox, OneUiBoolCallback callback, void* user_data) {
    auto* nativeCheckbox = asWidget<oneui::Checkbox>(checkbox);
    if (!nativeCheckbox) {
        return;
    }
    if (!callback) {
        nativeCheckbox->setOnChanged(nullptr);
        return;
    }
    nativeCheckbox->setOnChanged([callback, user_data](bool checked) {
        callback(checked ? 1 : 0, user_data);
    });
}

OneUiWidget* oneui_segmented_control_create() {
    return wrap(std::make_shared<oneui::Tabs>());
}

void oneui_segmented_control_set_items(OneUiWidget* segmented_control, const wchar_t* items) {
    if (auto* nativeTabs = asWidget<oneui::Tabs>(segmented_control)) {
        nativeTabs->setItems(splitWideItems(items));
    }
}

void oneui_segmented_control_set_selected_index(OneUiWidget* segmented_control, int index) {
    if (auto* nativeTabs = asWidget<oneui::Tabs>(segmented_control)) {
        nativeTabs->setSelectedIndex(index);
    }
}

int oneui_segmented_control_selected_index(OneUiWidget* segmented_control) {
    if (auto* nativeTabs = asWidget<oneui::Tabs>(segmented_control)) {
        return nativeTabs->selectedIndex();
    }
    return 0;
}

void oneui_segmented_control_set_on_changed(OneUiWidget* segmented_control, OneUiIntCallback callback, void* user_data) {
    auto* nativeTabs = asWidget<oneui::Tabs>(segmented_control);
    if (!nativeTabs) {
        return;
    }
    if (!callback) {
        nativeTabs->setOnChanged(nullptr);
        return;
    }
    nativeTabs->setOnChanged([callback, user_data](int value) {
        callback(value, user_data);
    });
}

OneUiWidget* oneui_tabs_create() {
    return wrap(std::make_shared<oneui::Tabs>());
}

OneUiWidget* oneui_select_create() {
    return wrap(std::make_shared<oneui::Select>());
}

void oneui_select_set_items_utf8(
    OneUiWidget* select,
    const OneUiUtf8String* items,
    std::size_t count) {
    auto* nativeSelect = asWidget<oneui::Select>(select);
    if (!nativeSelect) {
        return;
    }
    std::vector<std::wstring> values;
    if (items && count > 0) {
        values.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            values.push_back(utf8OrEmpty(items[index]));
        }
    }
    nativeSelect->setItems(std::move(values));
}

void oneui_select_set_selected_index(OneUiWidget* select, int index) {
    if (auto* nativeSelect = asWidget<oneui::Select>(select)) {
        nativeSelect->setSelectedIndex(index);
    }
}

int oneui_select_selected_index(OneUiWidget* select) {
    if (auto* nativeSelect = asWidget<oneui::Select>(select)) {
        return nativeSelect->selectedIndex();
    }
    return -1;
}

void oneui_select_set_on_changed(
    OneUiWidget* select,
    OneUiIntCallback callback,
    void* user_data) {
    auto* nativeSelect = asWidget<oneui::Select>(select);
    if (!nativeSelect) {
        return;
    }
    if (!callback) {
        nativeSelect->setOnChanged(nullptr);
        return;
    }
    nativeSelect->setOnChanged([callback, user_data](int value) {
        callback(value, user_data);
    });
}

OneUiWidget* oneui_list_create() {
    return wrap(std::make_shared<oneui::List>());
}

void oneui_list_set_items(OneUiWidget* list, const wchar_t* items) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setItems(splitWideListItems(items));
}

void oneui_list_set_items_utf8(
    OneUiWidget* list,
    const OneUiListItemUtf8* items,
    std::size_t count) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setItems(listItemsFromUtf8(items, count));
}

void oneui_list_set_selection_required(OneUiWidget* list, int required) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setSelectionRequired(required != 0);
}

void oneui_list_set_selected_index(OneUiWidget* list, int index) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setSelectedIndex(index);
}

int oneui_list_selected_index(OneUiWidget* list) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return 0;
    }
    return nativeList->selectedIndex();
}

void oneui_list_set_on_changed(OneUiWidget* list, OneUiIntCallback callback, void* user_data) {
    auto* nativeList = asWidget<oneui::List>(list);
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

OneUiWidget* oneui_radio_group_create() {
    return wrap(std::make_shared<oneui::RadioGroup>());
}

void oneui_radio_group_set_items(OneUiWidget* radio_group, const wchar_t* items) {
    auto* nativeRadio = asWidget<oneui::RadioGroup>(radio_group);
    if (!nativeRadio) {
        return;
    }
    nativeRadio->setItems(splitWideItems(items));
}

void oneui_radio_group_set_selected_index(OneUiWidget* radio_group, int index) {
    auto* nativeRadio = asWidget<oneui::RadioGroup>(radio_group);
    if (!nativeRadio) {
        return;
    }
    nativeRadio->setSelectedIndex(index);
}

int oneui_radio_group_selected_index(OneUiWidget* radio_group) {
    auto* nativeRadio = asWidget<oneui::RadioGroup>(radio_group);
    if (!nativeRadio) {
        return 0;
    }
    return nativeRadio->selectedIndex();
}

void oneui_radio_group_set_orientation(OneUiWidget* radio_group, int orientation) {
    auto* nativeRadio = asWidget<oneui::RadioGroup>(radio_group);
    if (!nativeRadio) {
        return;
    }
    nativeRadio->setOrientation(orientation == 1
        ? oneui::RadioGroup::Orientation::Horizontal
        : oneui::RadioGroup::Orientation::Vertical);
}

void oneui_radio_group_set_on_changed(OneUiWidget* radio_group, OneUiIntCallback callback, void* user_data) {
    auto* nativeRadio = asWidget<oneui::RadioGroup>(radio_group);
    if (!nativeRadio) {
        return;
    }
    if (!callback) {
        nativeRadio->setOnChanged(nullptr);
        return;
    }
    nativeRadio->setOnChanged([callback, user_data](int value) {
        callback(value, user_data);
    });
}
