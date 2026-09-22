#pragma once

#include "oneui/export.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <functional>
#include <optional>

namespace oneui {

enum class SliderInteraction { Begin = 0, Update = 1, Commit = 2, Cancel = 3 };

class ONEUI_API Slider final : public Widget {
public:
    Slider();

    void setRange(double minimum, double maximum);
    void setStep(double step);
    void setValue(double value);
    double value() const;
    void bindValue(State<double>& state);
    void setStyleOverride(SliderStyleOverride style);
    void clearStyleOverride();
    void setOnChanged(std::function<void(double)> callback);
    // Input-only lifecycle. Programmatic setValue retains onChanged semantics
    // but never generates interaction events. Cancel restores the begin value.
    void setOnInteraction(std::function<void(SliderInteraction, double)> callback);
    void cancelInteraction();
    bool dragging() const;

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool isFocusable() const override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    void assignValue(double value);
    void assignFromPoint(Point point);
    void notifyInteraction(SliderInteraction phase);
    double normalizedValue() const;
    SliderStyle resolvedStyle() const;
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    double minimum_ = 0.0;
    double maximum_ = 1.0;
    double step_ = 0.01;
    double value_ = 0.0;
    bool hovered_ = false;
    bool pressed_ = false;
    double beginValue_ = 0.0;
    std::optional<SliderStyleOverride> styleOverride_;
    Binding<double> valueBinding_;
    std::function<void(double)> onChanged_;
    std::function<void(SliderInteraction, double)> onInteraction_;
};

} // namespace oneui
