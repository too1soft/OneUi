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
    void setTextOptions(TextOptions options);
    const TextOptions& textOptions() const { return textOptions_; }
    /// Enables width-aware multi-line layout. Disabled by default so existing
    /// labels retain their single-line ellipsis behavior.
    void setTextWrapping(bool enabled);
    bool textWrapping() const;
    /// Limits wrapped output to a fixed number of lines. Zero means the
    /// current frame height is the only limit.
    void setMaxLines(int lines);
    int maxLines() const;
    /// Sets the wrapped line box height. Non-positive values use 1.4x the
    /// current font size.
    void setLineHeight(float height);
    float lineHeight() const;

    void paint(Canvas& canvas) override;

private:
    std::wstring text_;
    Binding<std::wstring> textBinding_;
    Color color_{25, 28, 33};
    float fontSize_ = 13.0f;
    int fontWeight_ = 400;
    TextAlign align_ = TextAlign::Left;
    bool textWrapping_ = false;
    int maxLines_ = 0;
    float lineHeight_ = 0.0f;
    TextOptions textOptions_;
};

} // namespace oneui
