#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/button.h"
#include "oneui/controls/card.h"
#include "oneui/controls/status_strip.h"
#include "oneui/controls/state_view.h"
#include "oneui/controls/tile.h"
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

OneUiWidget* oneui_card_create() {
    return wrap(std::make_shared<oneui::Card>());
}

void oneui_card_set_content(OneUiWidget* card, OneUiWidget* child) {
    auto* nativeCard = asWidget<oneui::Card>(card);
    if (!nativeCard) {
        return;
    }
    nativeCard->setContent(child ? child->widget : nullptr);
}

OneUiWidget* oneui_tile_create(const wchar_t* title, const wchar_t* subtitle) {
    return wrap(std::make_shared<oneui::Tile>(wideOrEmpty(title), wideOrEmpty(subtitle)));
}

void oneui_tile_set_title(OneUiWidget* tile, const wchar_t* title) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        nativeTile->setTitle(wideOrEmpty(title));
    }
}

void oneui_tile_set_subtitle(OneUiWidget* tile, const wchar_t* subtitle) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        nativeTile->setSubtitle(wideOrEmpty(subtitle));
    }
}

void oneui_tile_set_leading_symbol(OneUiWidget* tile, int symbol) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        const auto clamped = clampIconSymbol(symbol);
        nativeTile->setLeadingSymbol(static_cast<oneui::IconSymbol>(clamped));
    }
}

void oneui_tile_clear_leading_symbol(OneUiWidget* tile) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        nativeTile->clearLeadingSymbol();
    }
}

void oneui_tile_set_trailing_symbol(OneUiWidget* tile, int symbol) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        const auto clamped = clampIconSymbol(symbol);
        nativeTile->setTrailingSymbol(static_cast<oneui::IconSymbol>(clamped));
    }
}

void oneui_tile_clear_trailing_symbol(OneUiWidget* tile) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        nativeTile->clearTrailingSymbol();
    }
}

void oneui_tile_set_on_click(OneUiWidget* tile, OneUiVoidCallback callback, void* user_data) {
    auto* nativeTile = asWidget<oneui::Tile>(tile);
    if (!nativeTile) {
        return;
    }
    if (!callback) {
        nativeTile->setOnClick(nullptr);
        return;
    }
    nativeTile->setOnClick([callback, user_data] {
        callback(user_data);
    });
}

OneUiWidget* oneui_status_strip_create(const wchar_t* title, const wchar_t* message) {
    return wrap(std::make_shared<oneui::StatusStrip>(wideOrEmpty(title), wideOrEmpty(message)));
}

void oneui_status_strip_set_title(OneUiWidget* status_strip, const wchar_t* title) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        nativeStatusStrip->setTitle(wideOrEmpty(title));
    }
}

void oneui_status_strip_set_message(OneUiWidget* status_strip, const wchar_t* message) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        nativeStatusStrip->setMessage(wideOrEmpty(message));
    }
}

void oneui_status_strip_set_icon(OneUiWidget* status_strip, int symbol) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        const auto clamped = clampIconSymbol(symbol);
        nativeStatusStrip->setIconSymbol(static_cast<oneui::IconSymbol>(clamped));
    }
}

void oneui_status_strip_set_primary_action(OneUiWidget* status_strip, const wchar_t* text) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        nativeStatusStrip->setPrimaryAction(wideOrEmpty(text));
    }
}

void oneui_status_strip_set_primary_action_presentation(OneUiWidget* status_strip, int presentation) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        nativeStatusStrip->setPrimaryActionPresentation(
            presentation == 1
                ? oneui::StatusStripActionPresentation::Link
                : oneui::StatusStripActionPresentation::Button);
    }
}

void oneui_status_strip_set_primary_action_trailing_icon(OneUiWidget* status_strip, int symbol) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        if (symbol < 0) {
            nativeStatusStrip->setPrimaryActionTrailingIcon(std::nullopt);
        } else {
            nativeStatusStrip->setPrimaryActionTrailingIcon(static_cast<oneui::IconSymbol>(
                std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::OutlineSave))));
        }
    }
}

void oneui_status_strip_set_secondary_action(OneUiWidget* status_strip, const wchar_t* text) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        nativeStatusStrip->setSecondaryAction(wideOrEmpty(text));
    }
}

void oneui_status_strip_set_on_primary_action(OneUiWidget* status_strip, OneUiVoidCallback callback, void* user_data) {
    auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip);
    if (!nativeStatusStrip) {
        return;
    }
    nativeStatusStrip->setOnPrimaryAction(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

void oneui_status_strip_set_on_secondary_action(OneUiWidget* status_strip, OneUiVoidCallback callback, void* user_data) {
    auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip);
    if (!nativeStatusStrip) {
        return;
    }
    nativeStatusStrip->setOnSecondaryAction(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

OneUiWidget* oneui_state_view_create(const wchar_t* title, const wchar_t* message) {
    return wrap(std::make_shared<oneui::StateView>(wideOrEmpty(title), wideOrEmpty(message)));
}

void oneui_state_view_set_title(OneUiWidget* state_view, const wchar_t* title) {
    if (auto* nativeStateView = asWidget<oneui::StateView>(state_view)) {
        nativeStateView->setTitle(wideOrEmpty(title));
    }
}

void oneui_state_view_set_message(OneUiWidget* state_view, const wchar_t* message) {
    if (auto* nativeStateView = asWidget<oneui::StateView>(state_view)) {
        nativeStateView->setMessage(wideOrEmpty(message));
    }
}

void oneui_state_view_set_icon(OneUiWidget* state_view, int symbol) {
    if (auto* nativeStateView = asWidget<oneui::StateView>(state_view)) {
        nativeStateView->setIcon(static_cast<oneui::IconSymbol>(symbol));
    }
}

void oneui_state_view_set_action(OneUiWidget* state_view, const wchar_t* text) {
    if (auto* nativeStateView = asWidget<oneui::StateView>(state_view)) {
        nativeStateView->setAction(wideOrEmpty(text));
    }
}

void oneui_state_view_set_on_action(OneUiWidget* state_view, OneUiVoidCallback callback, void* user_data) {
    auto* nativeStateView = asWidget<oneui::StateView>(state_view);
    if (!nativeStateView) {
        return;
    }
    nativeStateView->setOnAction(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}
