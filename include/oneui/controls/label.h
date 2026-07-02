#pragma once

#include "oneui/canvas.h"
#include "oneui/export.h"
#include "oneui/reactive.h"
#include "oneui/widget.h"

#include <string>

namespace oneui {

class ONEUI_API Label final : public Widget {
public:
    explicit Label(std::wstring text = {});

    void setText(std::wstring text);
    const std::wstring& text() const;
    void bindText(State<std::wstring>& state);
    void setColor(Color color);
    void setFontSize(float size);
    void setFontWeight(int weight);
    void setAlign(TextAlign align);

    void paint(Canvas& canvas) override;

private:
    std::wstring text_;
    Binding<std::wstring> textBinding_;
    Color color_{25, 28, 33};
    float fontSize_ = 13.0f;
    int fontWeight_ = 400;
    TextAlign align_ = TextAlign::Left;
};

} // namespace oneui
