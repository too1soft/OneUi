#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/badge.h"
#include "oneui/controls/icon_badge.h"
#include "oneui/controls/icon_button.h"
#include "oneui/controls/icon_view.h"
#include "oneui/controls/label.h"
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

OneUiWidget* oneui_label_create(const wchar_t* text) {
    return wrap(std::make_shared<oneui::Label>(wideOrEmpty(text)));
}

OneUiWidget* oneui_label_create_utf8(OneUiUtf8String text) {
    return wrap(std::make_shared<oneui::Label>(utf8OrEmpty(text)));
}

void oneui_label_set_text(OneUiWidget* label, const wchar_t* text) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setText(wideOrEmpty(text));
}

void oneui_label_set_text_utf8(OneUiWidget* label, OneUiUtf8String text) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setText(utf8OrEmpty(text));
}

int oneui_label_set_rich_text_utf8(
    OneUiWidget* label,
    OneUiUtf8String text,
    const OneUiLabelTextSpanUtf8* spans,
    size_t span_count) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel || (text.length > 0 && !text.data) || (span_count > 0 && !spans)) {
        return 0;
    }
    const std::string_view bytes(text.data ? text.data : "", text.length);
    const auto isBoundary = [&](std::size_t offset) {
        return offset <= bytes.size() &&
            (offset == bytes.size() ||
             (static_cast<unsigned char>(bytes[offset]) & 0xc0u) != 0x80u);
    };
    std::vector<oneui::TextStyleSpan> nativeSpans;
    nativeSpans.reserve(span_count);
    std::size_t previousEnd = 0;
    for (std::size_t index = 0; index < span_count; ++index) {
        const auto& span = spans[index];
        if (span.start_utf8_offset >= span.end_utf8_offset ||
            span.end_utf8_offset > bytes.size() || span.start_utf8_offset < previousEnd ||
            !isBoundary(span.start_utf8_offset) || !isBoundary(span.end_utf8_offset) ||
            !std::isfinite(span.font_size) || span.font_size < 0.0f ||
            span.font_weight < 0 || span.font_weight > 1000) {
            return 0;
        }
        const auto wideStart = utf8OrEmpty(
            OneUiUtf8String{bytes.data(), span.start_utf8_offset}).size();
        const auto wideEnd = utf8OrEmpty(
            OneUiUtf8String{bytes.data(), span.end_utf8_offset}).size();
        nativeSpans.push_back(oneui::TextStyleSpan{
            wideStart,
            wideEnd,
            span.font_size,
            span.font_weight,
            span.italic != 0,
            span.monospace != 0,
            span.underline != 0,
            span.strikethrough != 0,
            toNativeColor(span.foreground),
            toNativeColor(span.background)});
        previousEnd = span.end_utf8_offset;
    }
    try {
        nativeLabel->setRichText(utf8OrEmpty(text), std::move(nativeSpans));
        return 1;
    } catch (...) {
        oneui::capi::recordCurrentException(__func__);
        return 0;
    }
}

void oneui_label_set_color(OneUiWidget* label, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setColor(toNativeColor(OneUiColor{r, g, b, a}));
}

void oneui_label_set_status_indicator(OneUiWidget* label, float diameter, float gap) {
    if (auto* nativeLabel = asWidget<oneui::Label>(label)) nativeLabel->setStatusIndicator(diameter, gap);
}

void oneui_label_set_font_size(OneUiWidget* label, float font_size) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setFontSize(font_size);
}

float oneui_label_natural_text_width(OneUiWidget* label) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    return nativeLabel ? nativeLabel->naturalTextSize().width : 0.0f;
}

void oneui_label_set_font_weight(OneUiWidget* label, int font_weight) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setFontWeight(std::clamp(font_weight, 100, 900));
}

void oneui_label_set_align(OneUiWidget* label, int align) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    switch (align) {
    case 1:
        nativeLabel->setAlign(oneui::TextAlign::Center);
        break;
    case 2:
        nativeLabel->setAlign(oneui::TextAlign::Right);
        break;
    default:
        nativeLabel->setAlign(oneui::TextAlign::Left);
        break;
    }
}

void oneui_label_set_text_wrapping(OneUiWidget* label, int enabled) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setTextWrapping(enabled != 0);
}

void oneui_label_set_max_lines(OneUiWidget* label, int max_lines) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setMaxLines(max_lines);
}

void oneui_label_set_line_height(OneUiWidget* label, float line_height) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setLineHeight(line_height);
}

OneUiWidget* oneui_icon_create(int symbol) {
    const auto clamped = clampIconSymbol(symbol);
    return wrap(std::make_shared<oneui::IconView>(static_cast<oneui::IconSymbol>(clamped)));
}

void oneui_icon_set_symbol(OneUiWidget* icon, int symbol) {
    auto* nativeIcon = asWidget<oneui::IconView>(icon);
    if (!nativeIcon) {
        return;
    }
    const auto clamped = clampIconSymbol(symbol);
    nativeIcon->setSymbol(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_icon_set_color(OneUiWidget* icon, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeIcon = asWidget<oneui::IconView>(icon);
    if (!nativeIcon) {
        return;
    }
    nativeIcon->setColor(oneui::Color{r, g, b, a});
}

void oneui_icon_set_accent(OneUiWidget* icon, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeIcon = asWidget<oneui::IconView>(icon);
    if (!nativeIcon) {
        return;
    }
    nativeIcon->setAccent(oneui::Color{r, g, b, a});
}

void oneui_icon_set_stroke_width(OneUiWidget* icon, float width) {
    auto* nativeIcon = asWidget<oneui::IconView>(icon);
    if (!nativeIcon) {
        return;
    }
    nativeIcon->setStrokeWidth(width);
}

OneUiWidget* oneui_icon_button_create(int symbol) {
    const auto clamped = clampIconSymbol(symbol);
    return wrap(std::make_shared<oneui::IconButton>(static_cast<oneui::IconSymbol>(clamped)));
}

void oneui_icon_button_set_symbol(OneUiWidget* icon_button, int symbol) {
    auto* nativeIconButton = asWidget<oneui::IconButton>(icon_button);
    if (!nativeIconButton) {
        return;
    }
    const auto clamped = clampIconSymbol(symbol);
    nativeIconButton->setSymbol(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_icon_button_set_on_click(OneUiWidget* icon_button, OneUiVoidCallback callback, void* user_data) {
    auto* nativeIconButton = asWidget<oneui::IconButton>(icon_button);
    if (!nativeIconButton) {
        return;
    }
    if (!callback) {
        nativeIconButton->setOnClick(nullptr);
        return;
    }
    nativeIconButton->setOnClick([callback, user_data] {
        callback(user_data);
    });
}

OneUiWidget* oneui_badge_create(const wchar_t* text, int variant) {
    return wrap(std::make_shared<oneui::Badge>(
        wideOrEmpty(text),
        static_cast<oneui::BadgeVariant>(variant)));
}

void oneui_badge_set_text(OneUiWidget* badge, const wchar_t* text) {
    auto* nativeBadge = asWidget<oneui::Badge>(badge);
    if (!nativeBadge) {
        return;
    }
    nativeBadge->setText(wideOrEmpty(text));
}

void oneui_badge_set_variant(OneUiWidget* badge, int variant) {
    auto* nativeBadge = asWidget<oneui::Badge>(badge);
    if (!nativeBadge) {
        return;
    }
    nativeBadge->setVariant(static_cast<oneui::BadgeVariant>(variant));
}

OneUiWidget* oneui_icon_badge_create(int symbol) {
    const auto clamped = clampIconSymbol(symbol);
    return wrap(std::make_shared<oneui::IconBadge>(static_cast<oneui::IconSymbol>(clamped)));
}

void oneui_icon_badge_set_symbol(OneUiWidget* icon_badge, int symbol) {
    auto* nativeIconBadge = asWidget<oneui::IconBadge>(icon_badge);
    if (!nativeIconBadge) {
        return;
    }
    const auto clamped = clampIconSymbol(symbol);
    nativeIconBadge->setSymbol(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_icon_badge_set_accent(OneUiWidget* icon_badge, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeIconBadge = asWidget<oneui::IconBadge>(icon_badge);
    if (!nativeIconBadge) {
        return;
    }
    nativeIconBadge->setAccent(oneui::Color{r, g, b, a});
}

void oneui_icon_badge_set_stroke_width(OneUiWidget* icon_badge, float width) {
    auto* nativeIconBadge = asWidget<oneui::IconBadge>(icon_badge);
    if (!nativeIconBadge) {
        return;
    }
    nativeIconBadge->setStrokeWidth(width);
}
