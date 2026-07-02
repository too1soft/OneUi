#include "oneui/controls/validation_message.h"

#include <utility>

namespace oneui {

namespace {

void applyValidationMessageStyleOverride(ValidationMessageStyle& style, const ValidationMessageStyleOverride& override) {
    if (override.helperColor) {
        style.helperColor = *override.helperColor;
    }
    if (override.errorColor) {
        style.errorColor = *override.errorColor;
    }
    if (override.fontSize) {
        style.fontSize = *override.fontSize;
    }
    if (override.lineHeight) {
        style.lineHeight = *override.lineHeight;
    }
}

} // namespace

ValidationMessage::ValidationMessage(std::wstring text) : text_(std::move(text)) {
    setPreferredSize(Size{0.0f, resolvedStyle().lineHeight});
}

void ValidationMessage::setText(std::wstring text) {
    textBinding_.set(std::move(text), text_);
    invalidate();
}

const std::wstring& ValidationMessage::text() const {
    return textBinding_.get(text_);
}

void ValidationMessage::bindText(State<std::wstring>& state) {
    textBinding_ = Binding<std::wstring>(state, [this] {
        invalidate();
    });
    invalidate();
}

void ValidationMessage::setTone(ValidationMessageTone tone) {
    tone_ = tone;
    invalidate();
}

ValidationMessageTone ValidationMessage::tone() const {
    return tone_;
}

void ValidationMessage::setStyleOverride(ValidationMessageStyleOverride style) {
    styleOverride_ = std::move(style);
    setPreferredSize(Size{preferredSize().width, resolvedStyle().lineHeight});
    invalidate();
}

void ValidationMessage::clearStyleOverride() {
    styleOverride_.reset();
    setPreferredSize(Size{preferredSize().width, resolvedStyle().lineHeight});
    invalidate();
}

void ValidationMessage::paint(Canvas& canvas) {
    if (text().empty()) {
        return;
    }

    const ValidationMessageStyle style = resolvedStyle();
    const Color color = tone_ == ValidationMessageTone::Error ? style.errorColor : style.helperColor;
    canvas.drawText(text(), frame(), disabled() ? theme().textSubtle : color, style.fontSize, TextAlign::Left);
}

ValidationMessageStyle ValidationMessage::resolvedStyle() const {
    const auto& field = theme().formField;
    ValidationMessageStyle style{field.helperColor, field.errorColor, field.messageFontSize, field.messageLineHeight};
    if (styleOverride_) {
        applyValidationMessageStyleOverride(style, *styleOverride_);
    }
    return style;
}

} // namespace oneui
