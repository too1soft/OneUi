#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/view.h"

#include <functional>
#include <memory>

namespace oneui {

enum class SplitOrientation {
    Horizontal,
    Vertical
};

class ONEUI_API SplitView final : public View {
public:
    explicit SplitView(SplitOrientation orientation = SplitOrientation::Horizontal);

    void setFirst(std::shared_ptr<Widget> child);
    void setSecond(std::shared_ptr<Widget> child);
    void setOrientation(SplitOrientation orientation);
    SplitOrientation orientation() const;
    void setSplitRatio(float ratio);
    float splitRatio() const;
    void setGap(float gap);
    void setPadding(Insets padding);
    void setResizable(bool resizable);
    bool resizable() const;
    void setMinimumPaneExtent(float first, float second);
    void setOnSplitRatioChanged(std::function<void(float)> callback);
    void setOnSplitRatioCommitted(std::function<void(float)> callback);

    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    CursorKind cursor(Point point) const override;

private:
    void layoutChildren() override;
    void rebuildChildren();
    Rect contentRect() const;
    bool hasResizableDivider() const;
    float availableExtent() const;
    float constrainedRatio(float ratio) const;
    Rect dividerHitRect() const;
    float axisPosition(Point point) const;
    void finishDividerDrag();
    void updateSplitRatio(float ratio, bool notify);
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::shared_ptr<Widget> first_;
    std::shared_ptr<Widget> second_;
    SplitOrientation orientation_ = SplitOrientation::Horizontal;
    float splitRatio_ = 0.5f;
    float gap_ = 0.0f;
    Insets padding_;
    float firstMinimumExtent_ = 0.0f;
    float secondMinimumExtent_ = 0.0f;
    float dragOffset_ = 0.0f;
    bool resizable_ = false;
    bool dividerHovered_ = false;
    bool draggingDivider_ = false;
    std::function<void(float)> onSplitRatioChanged_;
    std::function<void(float)> onSplitRatioCommitted_;
};

} // namespace oneui
