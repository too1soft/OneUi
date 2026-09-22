#pragma once

#include "oneui/canvas.h"
#include "oneui/export.h"
#include "oneui/reactive.h"
#include "oneui/widget.h"

#include <string>
#include <vector>

namespace oneui {

class ONEUI_API Label final : public Widget {
public:
    explicit Label(std::wstring text = {});

    void setText(std::wstring text);
    /// Replaces the text and its non-overlapping inline style ranges as one
    /// invalidation, preventing streaming producers from exposing stale spans.
    void setRichText(std::wstring text, std::vector<TextStyleSpan> spans);
    const std::wstring& text() const;
    const std::vector<TextStyleSpan>& textSpans() const { return textSpans_; }
    void bindText(State<std::wstring>& state);
    void setColor(Color color);
    void setFontSize(float size);
    void setFontWeight(int weight);
    void setAlign(TextAlign align);
    TextAlign textAlign() const { return align_; }
    float fontSize() const { return fontSize_; }
    int fontWeight() const { return fontWeight_; }
    /// Intrinsic text dimensions before clipping/ellipsis, using the same
    /// family, spans, weight and DPI as paint(). Intended for layout audits.
    Size naturalTextSize() const;
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
    std::vector<TextStyleSpan> textSpans_;
};

} // namespace oneui
