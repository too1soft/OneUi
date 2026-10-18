#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/dialog.h"
#include "oneui/controls/menu.h"
#include "oneui/controls/toast.h"
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

OneUiWidget* oneui_menu_create(void) {
    return wrap(std::make_shared<oneui::Menu>());
}

void oneui_menu_add_header(OneUiWidget* menu, const wchar_t* title, const wchar_t* subtitle) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return;
    }
    nativeMenu->addHeader(wideOrEmpty(title), wideOrEmpty(subtitle));
}

int oneui_menu_add_item(OneUiWidget* menu, const wchar_t* text, int icon_symbol, int danger) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return -1;
    }
    std::optional<oneui::IconSymbol> icon;
    if (icon_symbol >= 0) {
        const auto clamped = clampIconSymbol(icon_symbol);
        icon = static_cast<oneui::IconSymbol>(clamped);
    }
    return nativeMenu->addItem(wideOrEmpty(text), icon, danger != 0);
}

void oneui_menu_add_separator(OneUiWidget* menu) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return;
    }
    nativeMenu->addSeparator();
}

void oneui_menu_clear_items(OneUiWidget* menu) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return;
    }
    nativeMenu->clearItems();
}

void oneui_menu_set_item_disabled(OneUiWidget* menu, int index, int disabled) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return;
    }
    nativeMenu->setItemDisabled(index, disabled != 0);
}

float oneui_menu_preferred_height(OneUiWidget* menu) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return 0.0f;
    }
    return nativeMenu->preferredHeight();
}

void oneui_menu_set_on_activated(OneUiWidget* menu, OneUiIntCallback callback, void* user_data) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return;
    }
    if (!callback) {
        nativeMenu->setOnItemActivated(nullptr);
        return;
    }
    nativeMenu->setOnItemActivated([callback, user_data](int value) {
        callback(value, user_data);
    });
}

OneUiWidget* oneui_dialog_create(const wchar_t* title, const wchar_t* subtitle) {
    return wrap(std::make_shared<oneui::Dialog>(wideOrEmpty(title), wideOrEmpty(subtitle)));
}

void oneui_dialog_set_title(OneUiWidget* dialog, const wchar_t* title) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setTitle(wideOrEmpty(title));
}

void oneui_dialog_set_subtitle(OneUiWidget* dialog, const wchar_t* subtitle) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setSubtitle(wideOrEmpty(subtitle));
}

void oneui_dialog_set_icon(OneUiWidget* dialog, int symbol) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    if (symbol < 0) {
        nativeDialog->clearIconSymbol();
        return;
    }
    const auto clamped = clampIconSymbol(symbol);
    nativeDialog->setIconSymbol(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_dialog_set_close_visible(OneUiWidget* dialog, int visible) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setCloseVisible(visible != 0);
}

void oneui_dialog_set_on_close(OneUiWidget* dialog, OneUiVoidCallback callback, void* user_data) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setOnClose(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

void oneui_dialog_set_content(OneUiWidget* dialog, OneUiWidget* child) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setContent(child ? child->widget : nullptr);
}

void oneui_dialog_set_actions(OneUiWidget* dialog, OneUiWidget* child) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setActions(child ? child->widget : nullptr);
}

OneUiWidget* oneui_toast_create(const wchar_t* title, const wchar_t* message) {
    return wrap(std::make_shared<oneui::Toast>(wideOrEmpty(title), wideOrEmpty(message)));
}

void oneui_toast_set_title(OneUiWidget* toast, const wchar_t* title) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->setTitle(wideOrEmpty(title));
    }
}

void oneui_toast_set_message(OneUiWidget* toast, const wchar_t* message) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->setMessage(wideOrEmpty(message));
    }
}

void oneui_toast_set_primary_action(OneUiWidget* toast, const wchar_t* text) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->setPrimaryAction(wideOrEmpty(text));
    }
}

void oneui_toast_set_secondary_action(OneUiWidget* toast, const wchar_t* text) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->setSecondaryAction(wideOrEmpty(text));
    }
}

void oneui_toast_set_icon_symbol(OneUiWidget* toast, int symbol) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        const int clamped = clampIconSymbol(symbol);
        nativeToast->setIconSymbol(static_cast<oneui::IconSymbol>(clamped));
    }
}

void oneui_toast_clear_icon_symbol(OneUiWidget* toast) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->clearIconSymbol();
    }
}

void oneui_toast_set_close_visible(OneUiWidget* toast, int visible) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->setCloseVisible(visible != 0);
    }
}

void oneui_toast_set_on_primary_action(OneUiWidget* toast, OneUiVoidCallback callback, void* user_data) {
    auto* nativeToast = asWidget<oneui::Toast>(toast);
    if (!nativeToast) {
        return;
    }
    nativeToast->setOnPrimaryAction(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

void oneui_toast_set_on_secondary_action(OneUiWidget* toast, OneUiVoidCallback callback, void* user_data) {
    auto* nativeToast = asWidget<oneui::Toast>(toast);
    if (!nativeToast) {
        return;
    }
    nativeToast->setOnSecondaryAction(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

void oneui_toast_set_on_close(OneUiWidget* toast, OneUiVoidCallback callback, void* user_data) {
    auto* nativeToast = asWidget<oneui::Toast>(toast);
    if (!nativeToast) {
        return;
    }
    nativeToast->setOnClose(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}
