#pragma once

#include "oneui/animation.h"
#include "oneui/export.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <optional>
#include <vector>
#include <string>

namespace oneui {

struct ProgressSegment {
    double fraction = 0.0;
    Color color{0, 0, 0, 0};
    std::wstring label;
};

class ONEUI_API ProgressBar final : public Widget {
public:
    ProgressBar();

    void setValue(double value);
    double value() const;
    // Absolute fractions of the whole track, not weights. Invalid input leaves
    // the previous reading intact; totals above one are clipped at the track end.
    bool setSegments(std::vector<ProgressSegment> segments);
    void bindValue(State<double>& state);
    // Smoothly follows determinate values without changing the semantic value.
    void setSmooth(bool enabled, double durationMs = 180.0);
    bool smooth() const;
    // Indeterminate mode paints a restrained moving segment while work cannot
    // expose a truthful percentage.
    void setIndeterminate(bool indeterminate);
    bool indeterminate() const;
    void setAnimationsEnabled(bool enabled);
    bool animationsEnabled() const;
    void setStyleOverride(ProgressBarStyleOverride style);
    void clearStyleOverride();

    void paint(Canvas& canvas) override;
    bool tickAnimations(double nowMs) override;

private:
    void updateVisualValue(double next);
    double clampedValue() const;
    ProgressBarStyle resolvedStyle() const;

    double value_ = 0.0;
    std::vector<ProgressSegment> segments_;
    Binding<double> valueBinding_;
    FloatTransition visualValue_{0.0f};
    double smoothDurationMs_ = 180.0;
    double indeterminateStartedMs_ = 0.0;
    double indeterminatePhase_ = 0.0;
    bool smooth_ = false;
    bool indeterminate_ = false;
    bool animationsEnabled_ = true;
    std::optional<ProgressBarStyleOverride> styleOverride_;
};

} // namespace oneui
