#pragma once
#include "oneui/view.h"
#include "oneui/animation.h"

namespace oneui {
enum class RevealPreset { Fade, Expand };

// Retains one native subtree. Expansion participates in the existing parent
// measurement; fading keeps its layout until the subtree has disappeared.
class ONEUI_API Reveal final : public View {
public:
    explicit Reveal(std::shared_ptr<Widget> content);
    void setOpen(bool open);
    bool open() const { return open_; }
    void setPreset(RevealPreset preset);
    void setTransition(TransitionSpec spec);
    void setReducedMotion(bool reduced);
    float progress() const { return progress_.value(); }
    bool running() const { return progress_.running(); }
    Size measure(Size available) const override;
    Size naturalSize() const override;
    void paint(Canvas& canvas) override;
    bool tickAnimations(double nowMs) override;
    void setVisible(bool visible) override;
    bool isFocusable() const override { return acceptsInput() && View::isFocusable(); }
    bool requestFocus(Widget* child,bool visible=true) override { return acceptsInput() && View::requestFocus(child,visible); }
    bool onMouseMove(const MouseEvent& e) override { return acceptsInput() && View::onMouseMove(e); }
    bool onMouseDown(const MouseEvent& e) override { return acceptsInput() && View::onMouseDown(e); }
    bool onMouseUp(const MouseEvent& e) override { return acceptsInput() && View::onMouseUp(e); }
    bool onMouseWheel(const MouseWheelEvent& e) override { return acceptsInput() && View::onMouseWheel(e); }
    bool hitTest(Point p) const override { return acceptsInput() && View::hitTest(p); }
    bool paintsAboveSiblings() const override { return acceptsInput() && View::paintsAboveSiblings(); }
    bool focusFirstLeaf() override { return acceptsInput() && View::focusFirstLeaf(); }
    bool focusLastLeaf() override { return acceptsInput() && View::focusLastLeaf(); }
    bool onKeyDown(const KeyEvent& e) override { return acceptsInput() && View::onKeyDown(e); }
    bool onKeyUp(const KeyEvent& e) override { return acceptsInput() && View::onKeyUp(e); }
    bool onFocusChanged(bool value) override { return (!value || acceptsInput()) && View::onFocusChanged(value); }
    std::shared_ptr<Widget> activeFocusChild() const override { return acceptsInput()?View::activeFocusChild():std::shared_ptr<Widget>{}; }
    Rect paintBounds() const override;
protected:
    void layoutChildren() override;
private:
    bool acceptsInput() const { return open_ && !running() && visible() && !disabled(); }
    bool open_=true, reduced_=false;
    RevealPreset preset_=RevealPreset::Expand;
    TransitionSpec spec_{220,EasingCurve::EaseOutCubic};
    FloatTransition progress_{1};
};
}
