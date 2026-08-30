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
    InteractiveSurfaceStateStyle focusVisible;
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
    /// Semantic action used by keyboard activation and by pointer activation
    /// when no pointer-specific callback is installed.
    void setOnClick(std::function<void()> callback);
    /// Pointer-specific activation for selection, click-count, and modifier semantics.
    void setOnPointerActivated(std::function<void(const MouseEvent&)> callback);
    /// Continuous pointer movement while the surface owns hover. This is
    /// intended for data inspection surfaces such as charts and timelines.
    void setOnPointerMoved(std::function<void(const MouseEvent&)> callback);
    /// Emits both pointer enter and pointer leave without products polling
    /// native geometry or duplicating hover-state bookkeeping.
    void setOnHoverChanged(std::function<void(bool)> callback);
    void setOnContextMenuRequested(std::function<void(const MouseEvent&)> callback);
    void setDisabled(bool disabled) override;

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onFocusChanged(bool focused) override;
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
    MouseButton pressedButton_ = MouseButton::None;
    int pressedClickCount_ = 1;
    bool visualInitialized_ = false;
    ColorTransition backgroundTransition_;
    ColorTransition borderTransition_;
    std::function<void()> onClick_;
    std::function<void(const MouseEvent&)> onPointerActivated_;
    std::function<void(const MouseEvent&)> onPointerMoved_;
    std::function<void(bool)> onHoverChanged_;
    std::function<void(const MouseEvent&)> onContextMenuRequested_;
};

} // namespace oneui
