#include "oneui/controls/label.h"

#include <utility>

namespace oneui {

Label::Label(std::wstring text) : text_(std::move(text)) {
    setPreferredSize(Size{0.0f, 22.0f});
}

void Label::setText(std::wstring text) {
    textBinding_.set(std::move(text), text_);
    invalidate();
}

const std::wstring& Label::text() const {
    return textBinding_.get(text_);
}

void Label::bindText(State<std::wstring>& state) {
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

void Label::paint(Canvas& canvas) {
    canvas.drawTextStyled(text(), frame(), disabled() ? Color{148, 163, 184} : color_, fontSize_, align_, fontWeight_);
}

} // namespace oneui
