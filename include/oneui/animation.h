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

struct ScrollMotionSpec {
    // Approximate time for the first impulse and an in-flight retarget to
    // settle within five percent of their target. Keeping these separate lets
    // a gesture stay continuous across delayed device packets while appended
    // distance converges promptly. The analytic solver is frame-rate agnostic.
    double initialSettlingDurationMs = 150.0;
    double retargetSettlingDurationMs = 110.0;
    double inputVelocityRatio = 0.55;
    double maximumVelocityRatio = 0.90;
};

// Shared wheel motion contract for scrolling controls. Products may supply a
// different spec through lower-level motion APIs, but standard OneUI scroll
// surfaces must use one common behavior.
inline constexpr ScrollMotionSpec kDefaultWheelScrollMotionSpec{};

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

class ONEUI_API SmoothScrollMotion final {
public:
    explicit SmoothScrollMotion(float value = 0.0f);

    void reset(float value);
    bool addDelta(
        float delta,
        float minimum,
        float maximum,
        double nowMs,
        ScrollMotionSpec spec = {});
    bool tick(double nowMs);
    float value() const;
    float target() const;
    float velocity() const;
    bool running() const;

private:
    float value_ = 0.0f;
    float target_ = 0.0f;
    float velocity_ = 0.0f;
    float minimum_ = 0.0f;
    float maximum_ = 0.0f;
    double lastMs_ = 0.0;
    double angularFrequency_ = 0.0;
    ScrollMotionSpec spec_;
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
