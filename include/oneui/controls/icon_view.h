#pragma once

#include "oneui/icon.h"
#include "oneui/widget.h"

namespace oneui {

class ONEUI_API IconView final : public Widget {
public:
    explicit IconView(IconSymbol symbol = IconSymbol::RemoteAssist);

    void setSymbol(IconSymbol symbol);
    void setColor(Color color);
    void setAccent(Color color);
    void setStrokeWidth(float width);

    void paint(Canvas& canvas) override;

private:
    IconSymbol symbol_ = IconSymbol::RemoteAssist;
    Color color_{218, 219, 225, 255};
    Color accent_{0, 0, 0, 0};
    float strokeWidth_ = 1.5f;
};

} // namespace oneui
