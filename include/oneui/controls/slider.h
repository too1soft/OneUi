#pragma once

#include "oneui/export.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <functional>
#include <optional>

namespace oneui {

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
    std::optional<SliderStyleOverride> styleOverride_;
    Binding<double> valueBinding_;
    std::function<void(double)> onChanged_;
};

} // namespace oneui
