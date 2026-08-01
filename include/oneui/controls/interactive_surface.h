#pragma once

#include "oneui/animation.h"
#include "oneui/export.h"
#include "oneui/view.h"

#include <functional>
#include <memory>

namespace oneui {

struct InteractiveSurfaceStateStyle {
    Color background{255, 255, 255, 255};
    Color border{226, 229, 234, 255};
    float borderWidth = 1.0f;
    float radius = 8.0f;
};

struct InteractiveSurfaceStyle {
    InteractiveSurfaceStateStyle normal;
    InteractiveSurfaceStateStyle hovered;
    InteractiveSurfaceStateStyle pressed;
    InteractiveSurfaceStateStyle disabled;
    TransitionSpec transition{120.0, EasingCurve::EaseOutCubic};
};

/// A content surface with native pointer feedback. Unlike Button it owns an
/// arbitrary child tree, making it suitable for cards, list rows, and compact
/// dashboard panels without products reimplementing hover behavior.
class ONEUI_API InteractiveSurface final : public View {
public:
    InteractiveSurface();

    void setStyle(InteractiveSurfaceStyle style);
    void setPadding(Insets padding);
    void setContent(std::shared_ptr<Widget> child);
    void setOnClick(std::function<void()> callback);
    void setDisabled(bool disabled) override;

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    CursorKind cursor(Point point) const override;
    bool isFocusable() const override;
    bool tickAnimations(double nowMs) override;

protected:
    void layoutChildren() override;
    bool hasInteractionState() const override;
    void resetInteractionState() override;

private:
    InteractiveSurfaceStateStyle resolvedStyle() const;
    InteractiveSurfaceStateStyle visualStyle(InteractiveSurfaceStateStyle target) const;
    void beginVisualTransition(
        InteractiveSurfaceStateStyle from,
        InteractiveSurfaceStateStyle target);

    InteractiveSurfaceStyle style_;
    Insets padding_;
    bool hovered_ = false;
    bool pressed_ = false;
    bool visualInitialized_ = false;
    ColorTransition backgroundTransition_;
    ColorTransition borderTransition_;
    std::function<void()> onClick_;
};

} // namespace oneui
