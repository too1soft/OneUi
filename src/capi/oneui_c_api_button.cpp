#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/button.h"
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

oneui::ButtonStateStyleOverride toButtonStateOverride(const OneUiButtonStateStyle& style) {
    oneui::ButtonStateStyleOverride result;
    result.background = toNativeColor(style.background);
    result.foreground = toNativeColor(style.foreground);
    result.border = toNativeColor(style.border);
    result.borderWidth = style.border_width;
    result.radius = style.radius;
    result.focusRing = toFocusRingOverride(style.focus_ring);
    return result;
}

oneui::ButtonStyleOverride toButtonOverride(const OneUiButtonStyle& style) {
    oneui::ButtonStyleOverride result;
    result.normal = toButtonStateOverride(style.normal);
    result.hovered = toButtonStateOverride(style.hovered);
    result.pressed = toButtonStateOverride(style.pressed);
    result.disabled = toButtonStateOverride(style.disabled);
    result.focusVisible = toButtonStateOverride(style.focus_visible);
    return result;
}

} // namespace

void oneui_button_set_trailing_icon(OneUiWidget* button, int symbol) {
    if(auto* value=asWidget<oneui::Button>(button)) value->setTrailingIcon(static_cast<oneui::IconSymbol>(std::clamp(symbol,0,static_cast<int>(oneui::IconSymbol::OutlineSave))));
}

OneUiWidget* oneui_button_create(const wchar_t* text) {
    return wrap(std::make_shared<oneui::Button>(wideOrEmpty(text)));
}

OneUiWidget* oneui_button_create_utf8(OneUiUtf8String text) {
    return wrap(std::make_shared<oneui::Button>(utf8OrEmpty(text)));
}

void oneui_button_set_text(OneUiWidget* button, const wchar_t* text) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    nativeButton->setText(wideOrEmpty(text));
}

void oneui_button_set_text_utf8(OneUiWidget* button, OneUiUtf8String text) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    nativeButton->setText(utf8OrEmpty(text));
}

void oneui_button_set_icon(OneUiWidget* button, int symbol) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    if (symbol < 0) {
        nativeButton->clearIcon();
        return;
    }
    const auto clamped = clampIconSymbol(symbol);
    nativeButton->setIcon(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_button_set_content_align(OneUiWidget* button, int align) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    const auto clamped = std::clamp(align, 0, 2);
    nativeButton->setContentAlign(static_cast<oneui::TextAlign>(clamped));
}

void oneui_button_set_trailing_text_utf8(OneUiWidget* button, OneUiUtf8String text) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    nativeButton->setTrailingText(utf8OrEmpty(text));
}

void oneui_button_set_variant(OneUiWidget* button, OneUiButtonVariant variant) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    nativeButton->setVariant(variant == OneUiButtonVariantSecondary
        ? oneui::ButtonVariant::Secondary
        : oneui::ButtonVariant::Primary);
}

void oneui_button_set_on_click(OneUiWidget* button, OneUiVoidCallback callback, void* user_data) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    if (!callback) {
        nativeButton->setOnClick(nullptr);
        return;
    }
    nativeButton->setOnClick([callback, user_data] {
        callback(user_data);
    });
}

void oneui_button_set_style(OneUiWidget* button, const OneUiButtonStyle* style) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton || !style) {
        return;
    }
    nativeButton->setStyleOverride(toButtonOverride(*style));
}

void oneui_button_clear_style(OneUiWidget* button) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    nativeButton->clearStyleOverride();
}
