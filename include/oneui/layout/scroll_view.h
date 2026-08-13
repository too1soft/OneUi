#pragma once

#include "oneui/animation.h"
#include "oneui/export.h"
#include "oneui/style_sheet.h"
#include "oneui/view.h"

#include <memory>
#include <optional>

namespace oneui {

class ONEUI_API ScrollView final : public View {
public:
    ScrollView();

    void setContent(std::shared_ptr<Widget> content);
    void setContentWidth(float width);
    void setContentHeight(float height);
    void setWheelStep(float step);
    void setChromeVisible(bool visible);
    void setScrollbarStyle(Color color, float thickness);
    void setStyleBox(StyleBox style);
    void clearStyleBox();
    void setHorizontalScrollOffset(float offset);
    void setScrollOffset(float offset);
    float horizontalScrollOffset() const;
    float scrollOffset() const;
    float maxHorizontalScrollOffset() const;
    float maxScrollOffset() const;

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool tickAnimations(double nowMs) override;

private:
    void layoutChildren() override;
    void resetInteractionState() override;
    Rect horizontalThumbRect() const;
    Rect verticalThumbRect() const;
    float resolvedContentWidth() const;
    float resolvedContentHeight() const;
    float clampHorizontalOffset(float offset) const;
    float clampOffset(float offset) const;
    bool hasHorizontalOverflow() const;
    bool hasVerticalOverflow() const;
    Rect viewportRect() const;
    void resetScrollMotion(float offset);
    bool advanceScrollMotion(double nowMs);

    std::shared_ptr<Widget> content_;
    float contentWidth_ = 0.0f;
    float contentHeight_ = 0.0f;
    float horizontalScrollOffset_ = 0.0f;
    float scrollOffset_ = 0.0f;
    SmoothScrollMotion scrollMotion_;
    double scrollTraceLastWheelMs_ = 0.0;
    double scrollTraceLastTickMs_ = 0.0;
    float wheelStep_ = 42.0f;
    bool chromeVisible_ = true;
    Color scrollbarColor_{148, 163, 184, 180};
    float scrollbarThickness_ = 4.0f;
    std::optional<StyleBox> styleBox_;
    bool draggingHorizontalThumb_ = false;
    float dragStartX_ = 0.0f;
    float dragStartHorizontalOffset_ = 0.0f;
};

} // namespace oneui
