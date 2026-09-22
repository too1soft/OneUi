#include "oneui/controls/label.h"

#include <algorithm>
#include "text/text_layout.h"
#include "internal/unicode.h"
#include <cmath>
#include <cwctype>
#include <stdexcept>
#include <vector>
#include <utility>

namespace oneui {
Label::Label(std::wstring text) : text_(std::move(text)) {
    setPreferredSize(Size{0.0f, 22.0f});
}

void Label::setText(std::wstring text) {
    if (textBinding_.get(text_) == text && textSpans_.empty()) return;
    const auto alive = lifetimeToken();
    textBinding_.set(std::move(text), text_);
    if (alive.expired()) return;
    textSpans_.clear();
    invalidate();
}

void Label::setRichText(std::wstring text, std::vector<TextStyleSpan> spans) {
    std::size_t previousEnd = 0;
    for (const auto& span : spans) {
        const bool validMetrics = std::isfinite(span.fontSize) && span.fontSize >= 0.0f &&
            span.fontWeight >= 0 && span.fontWeight <= 1000;
        const bool validRange = span.start < span.end && span.end <= text.size() &&
            span.start >= previousEnd && unicode::boundary(text, span.start) == span.start &&
            unicode::boundary(text, span.end) == span.end;
        if (!validMetrics || !validRange) {
            throw std::invalid_argument("Invalid OneUI rich label span");
        }
        previousEnd = span.end;
    }
    if (textBinding_.get(text_) == text && textSpans_ == spans) return;
    const auto alive = lifetimeToken();
    textBinding_.set(std::move(text), text_);
    if (alive.expired()) return;
    textSpans_ = std::move(spans);
    invalidate();
}

const std::wstring& Label::text() const {
    return textBinding_.get(text_);
}

void Label::bindText(State<std::wstring>& state) {
    textSpans_.clear();
    textBinding_ = Binding<std::wstring>(state, [this] {
        invalidate();
    });
    invalidate();
}

void Label::setColor(Color color) {
    color_ = color;
    invalidate();
}

void Label::setFontSize(float size) {
    fontSize_ = size;
    invalidate();
}

void Label::setFontWeight(int weight) {
    fontWeight_ = weight;
    invalidate();
}

void Label::setAlign(TextAlign align) {
    align_ = align;
    invalidate();
}

void Label::setTextWrapping(bool enabled) {
    if (textWrapping_ == enabled) {
        return;
    }
    textWrapping_ = enabled;
    textOptions_.wrap = enabled ? TextWrapMode::WordWrap : TextWrapMode::NoWrap;
    invalidate();
}

bool Label::textWrapping() const {
    return textWrapping_;
}

void Label::setMaxLines(int lines) {
    const int next = std::max(0, lines);
    if (maxLines_ == next) {
        return;
    }
    maxLines_ = next;
    invalidate();
}

int Label::maxLines() const {
    return maxLines_;
}

void Label::setLineHeight(float height) {
    const float next = std::max(0.0f, height);
    if (lineHeight_ == next) {
        return;
    }
    lineHeight_ = next;
    invalidate();
}

float Label::lineHeight() const {
    return lineHeight_;
}

void Label::setTextOptions(TextOptions options) {
    if (options.locale.empty()) options.locale = "und";
    textOptions_ = std::move(options);
    textWrapping_ = textOptions_.wrap == TextWrapMode::WordWrap;
    invalidate();
}

Size Label::naturalTextSize() const {
    text::LayoutOptions options;
    options.text = textOptions_;
    options.text.wrap = TextWrapMode::NoWrap;
    options.family = textFontFamily();
    options.size = fontSize_;
    options.weight = fontWeight_;
    options.scale = textDpiScale();
    options.spans = textSpans_;
    const auto layout = text::Layout::make(text(), options);
    return {layout->width(), layout->height()};
}

void Label::paint(Canvas& canvas) {
    const Rect bounds = frame();
    if (bounds.width <= 0 || bounds.height <= 0 || fontSize_ <= 0) return;
    TextBlockStyle style;
    style.dpiScale = textDpiScale();
    style.options = textOptions_;
    style.fontSize = fontSize_; style.fontWeight = fontWeight_;
    style.fontFamily = textFontFamily(); style.align = align_;
    style.ellipsis = true;
    style.spans = textSpans_;
    if (textWrapping_) {
        style.lineHeight = lineHeight_ > 0 ? lineHeight_ : std::max(1.0f, fontSize_ * 1.4f);
        const auto heightLimit = std::max(1, static_cast<int>(bounds.height / style.lineHeight));
        style.maxLines = maxLines_ > 0 ? std::min(maxLines_, heightLimit) : heightLimit;
    } else style.maxLines = 1;
    text::LayoutOptions options;
    options.text = style.options; options.family = style.fontFamily;
    options.size = style.fontSize; options.weight = style.fontWeight;
    options.width = bounds.width; options.lineHeight = style.lineHeight;
    options.scale = textDpiScale(); options.align = align_;
    options.maxLines = style.maxLines; options.ellipsis = true;
    options.spans = style.spans;
    const auto layout = text::Layout::make(text(), options);
    auto area = bounds;
    if (!textWrapping_) area.y += (bounds.height - layout->height()) / 2;
    canvas.save(); canvas.clipRect(bounds);
    canvas.drawTextBlock(text(), area, disabled() ? Color{148, 163, 184} : color_, style);
    canvas.restore();
}

} // namespace oneui
