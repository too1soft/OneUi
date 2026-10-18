#include "oneui_c_api_controls_internal.h"
#include "oneui/platform/window.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/text_field.h"
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

oneui::TextFieldStateStyleOverride toTextFieldStateOverride(const OneUiTextFieldStateStyle& style) {
    oneui::TextFieldStateStyleOverride result;
    result.background = toNativeColor(style.background);
    result.foreground = toNativeColor(style.foreground);
    result.placeholderForeground = toNativeColor(style.placeholder_foreground);
    result.border = toNativeColor(style.border);
    result.selectionBackground = toNativeColor(style.selection_background);
    result.caretColor = toNativeColor(style.caret_color);
    result.borderWidth = style.border_width;
    result.radius = style.radius;
    result.padding = toNativeInsets(style.padding);
    result.focusRing = toFocusRingOverride(style.focus_ring);
    return result;
}

oneui::TextFieldStyleOverride toTextFieldOverride(const OneUiTextFieldStyle& style) {
    oneui::TextFieldStyleOverride result;
    result.normal = toTextFieldStateOverride(style.normal);
    result.hovered = toTextFieldStateOverride(style.hovered);
    result.disabled = toTextFieldStateOverride(style.disabled);
    result.readOnly = toTextFieldStateOverride(style.read_only);
    result.focusVisible = toTextFieldStateOverride(style.focus_visible);
    return result;
}

} // namespace

void oneui_text_field_set_focused_placeholder(OneUiWidget* field, int visible) {
    if(auto* value=asWidget<oneui::TextField>(field)) value->setPlaceholderVisibleWhenFocused(visible != 0);
}

OneUiWidget* oneui_text_field_create(const wchar_t* placeholder) {
    auto field = std::make_shared<oneui::TextField>(wideOrEmpty(placeholder));
    field->setClipboard(std::make_shared<oneui::SystemClipboard>());
    return wrap(std::move(field));
}

OneUiWidget* oneui_text_field_create_utf8(OneUiUtf8String placeholder) {
    auto field = std::make_shared<oneui::TextField>(utf8OrEmpty(placeholder));
    field->setClipboard(std::make_shared<oneui::SystemClipboard>());
    return wrap(std::move(field));
}

OneUiWidget* oneui_text_area_create_utf8(OneUiUtf8String placeholder) {
    auto field = std::make_shared<oneui::TextArea>(utf8OrEmpty(placeholder));
    field->setClipboard(std::make_shared<oneui::SystemClipboard>());
    return wrap(std::move(field));
}

void oneui_text_area_set_vertical_scroll_offset(OneUiWidget* text_area, float offset) {
    auto* nativeTextArea = asWidget<oneui::TextArea>(text_area);
    if (nativeTextArea) {
        nativeTextArea->setVerticalScrollOffset(offset);
    }
}

float oneui_text_area_vertical_scroll_offset(OneUiWidget* text_area) {
    auto* nativeTextArea = asWidget<oneui::TextArea>(text_area);
    return nativeTextArea ? nativeTextArea->verticalScrollOffset() : 0.0f;
}

float oneui_text_area_max_vertical_scroll_offset(OneUiWidget* text_area) {
    auto* nativeTextArea = asWidget<oneui::TextArea>(text_area);
    return nativeTextArea ? nativeTextArea->maxVerticalScrollOffset() : 0.0f;
}

void oneui_text_area_scroll_to_top(OneUiWidget* text_area) {
    auto* nativeTextArea = asWidget<oneui::TextArea>(text_area);
    if (nativeTextArea) {
        nativeTextArea->scrollToTop();
    }
}

void oneui_text_field_set_text(OneUiWidget* text_field, const wchar_t* text) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setText(wideOrEmpty(text));
}

void oneui_text_field_set_text_utf8(OneUiWidget* text_field, OneUiUtf8String text) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setText(utf8OrEmpty(text));
}

int oneui_text_field_undo(OneUiWidget* text_field) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    return nativeTextField && nativeTextField->undo() ? 1 : 0;
}

int oneui_text_field_redo(OneUiWidget* text_field) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    return nativeTextField && nativeTextField->redo() ? 1 : 0;
}

void oneui_text_field_set_read_only(OneUiWidget* text_field, int read_only) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setReadOnly(read_only != 0);
}

void oneui_text_field_set_password_mode(OneUiWidget* text_field, int enabled) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setPasswordMode(enabled != 0);
}

void oneui_text_field_set_password_mask(OneUiWidget* text_field, unsigned int codepoint) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField || codepoint == 0 || codepoint > 0xFFFFu) {
        return;
    }
    nativeTextField->setPasswordMask(static_cast<wchar_t>(codepoint));
}

void oneui_text_field_set_multiline(OneUiWidget* text_field, int multiline) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setMultiline(multiline != 0);
}

void oneui_text_field_set_submit_on_enter(OneUiWidget* text_field, int submit_on_enter) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setSubmitOnEnter(submit_on_enter != 0);
}

void oneui_text_field_set_line_height(OneUiWidget* text_field, float line_height) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setLineHeight(line_height);
}

void oneui_text_field_set_font_size(OneUiWidget* text_field, float font_size) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setFontSize(font_size);
}

void oneui_text_field_set_prefix_icon(OneUiWidget* text_field, int symbol) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    const auto clamped = clampIconSymbol(symbol);
    nativeTextField->setPrefixIcon(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_text_field_clear_prefix_icon(OneUiWidget* text_field) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setPrefixIcon(std::nullopt);
}

void oneui_text_field_set_suffix_icon(OneUiWidget* text_field, int symbol) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    const auto clamped = clampIconSymbol(symbol);
    nativeTextField->setSuffixIcon(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_text_field_clear_suffix_icon(OneUiWidget* text_field) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setSuffixIcon(std::nullopt);
}

void oneui_text_field_set_on_changed(OneUiWidget* text_field, OneUiTextCallback callback, void* user_data) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    if (!callback) {
        nativeTextField->setOnChanged(nullptr);
        return;
    }
    nativeTextField->setOnChanged([callback, user_data](const std::wstring& text) {
        callback(text.c_str(), user_data);
    });
}

void oneui_text_field_set_on_changed_utf8(OneUiWidget* text_field, OneUiUtf8TextCallback callback, void* user_data) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    if (!callback) {
        nativeTextField->setOnChanged(nullptr);
        return;
    }
    nativeTextField->setOnChanged([callback, user_data](const std::wstring& text) {
        const std::string utf8 = utf8FromWide(text);
        callback(utf8.data(), utf8.size(), user_data);
    });
}

void oneui_text_field_set_on_submitted(OneUiWidget* text_field, OneUiTextCallback callback, void* user_data) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    if (!callback) {
        nativeTextField->setOnSubmitted(nullptr);
        return;
    }
    nativeTextField->setOnSubmitted([callback, user_data](const std::wstring& text) {
        callback(text.c_str(), user_data);
    });
}

void oneui_text_field_set_on_submitted_utf8(OneUiWidget* text_field, OneUiUtf8TextCallback callback, void* user_data) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    if (!callback) {
        nativeTextField->setOnSubmitted(nullptr);
        return;
    }
    nativeTextField->setOnSubmitted([callback, user_data](const std::wstring& text) {
        const std::string utf8 = utf8FromWide(text);
        callback(utf8.data(), utf8.size(), user_data);
    });
}

void oneui_text_field_set_style(OneUiWidget* text_field, const OneUiTextFieldStyle* style) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField || !style) {
        return;
    }
    nativeTextField->setStyleOverride(toTextFieldOverride(*style));
}

void oneui_text_field_clear_style(OneUiWidget* text_field) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->clearStyleOverride();
}

OneUiWidget* oneui_search_box_create(const wchar_t* placeholder) {
    auto field = std::make_shared<oneui::TextField>(wideOrEmpty(placeholder));
    field->setClipboard(std::make_shared<oneui::SystemClipboard>());
    field->setPrefixIcon(oneui::IconSymbol::Search);
    field->setSuffixIcon(oneui::IconSymbol::ChevronDown);
    return wrap(std::move(field));
}

OneUiWidget* oneui_search_box_create_utf8(OneUiUtf8String placeholder) {
    auto field = std::make_shared<oneui::TextField>(utf8OrEmpty(placeholder));
    field->setClipboard(std::make_shared<oneui::SystemClipboard>());
    field->setPrefixIcon(oneui::IconSymbol::Search);
    field->setSuffixIcon(oneui::IconSymbol::ChevronDown);
    return wrap(std::move(field));
}
