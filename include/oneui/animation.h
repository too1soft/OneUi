#pragma once

#include "oneui/color.h"
#include "oneui/export.h"

namespace oneui {

enum class EasingCurve {
    Linear,
    EaseOutCubic,
    EaseInOutCubic
};

struct TransitionSpec {
    double durationMs = 80.0;
    EasingCurve easing = EasingCurve::EaseOutCubic;
};

ONEUI_API double clampUnit(double value);
ONEUI_API double applyEasing(EasingCurve curve, double progress);
ONEUI_API float interpolateFloat(float from, float to, double progress);
ONEUI_API Color interpolateColor(Color from, Color to, double progress);

class ONEUI_API FloatTransition final {
public:
    explicit FloatTransition(float value = 0.0f);

    void reset(float value);
    void animateTo(float target, double nowMs, TransitionSpec spec = {});
    bool tick(double nowMs);
    float value() const;
    float target() const;
    bool running() const;

private:
    float from_ = 0.0f;
    float value_ = 0.0f;
    float target_ = 0.0f;
    double startMs_ = 0.0;
    TransitionSpec spec_;
    bool running_ = false;
};

class ONEUI_API ColorTransition final {
public:
    explicit ColorTransition(Color value = Color{0, 0, 0, 0});

    void reset(Color value);
    void animateTo(Color target, double nowMs, TransitionSpec spec = {});
    bool tick(double nowMs);
    Color value() const;
    Color target() const;
    bool running() const;

private:
    Color from_{0, 0, 0, 0};
    Color value_{0, 0, 0, 0};
    Color target_{0, 0, 0, 0};
    double startMs_ = 0.0;
    TransitionSpec spec_;
    bool running_ = false;
};

} // namespace oneui
