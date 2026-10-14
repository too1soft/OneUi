#pragma once

#include "oneui/icon.h"
#include "oneui/widget.h"

#include <optional>

namespace oneui {

class ONEUI_API IconView final : public Widget {
public:
    explicit IconView(IconSymbol symbol = IconSymbol::RemoteAssist);

    void setSymbol(IconSymbol symbol);
    void setColor(Color color);
    void setAccent(Color color);
    void setStrokeWidth(float width);
    // An explicit glyph size keeps the symbol at that size, centred, when a
    // parent assigns a larger frame (e.g. a stretched drag handle). Without
    // one the glyph scales with the frame.
    void setGlyphSize(std::optional<float> size);
    std::optional<float> glyphSize() const { return glyphSize_; }

    void paint(Canvas& canvas) override;

private:
    IconSymbol symbol_ = IconSymbol::RemoteAssist;
    Color color_{218, 219, 225, 255};
    Color accent_{0, 0, 0, 0};
    float strokeWidth_ = 1.5f;
    std::optional<float> glyphSize_;
};

} // namespace oneui
