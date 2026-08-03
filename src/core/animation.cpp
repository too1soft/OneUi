#include "oneui/animation.h"

#include <algorithm>
#include <cmath>

namespace oneui {
namespace {

std::uint8_t interpolateByte(std::uint8_t from, std::uint8_t to, double progress) {
    const double value = static_cast<double>(from) + (static_cast<double>(to) - static_cast<double>(from)) * clampUnit(progress);
    return static_cast<std::uint8_t>(std::clamp(std::round(value), 0.0, 255.0));
}

double normalizedProgress(double nowMs, double startMs, double durationMs) {
    if (durationMs <= 0.0) {
        return 1.0;
    }
    return clampUnit((nowMs - startMs) / durationMs);
}

bool sameColor(Color lhs, Color rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

constexpr double kFirstVisualFrameMs = 8.0;
constexpr double kCriticalDampingFivePercent = 4.743864518390577;
constexpr double kMinimumScrollSettlingDurationMs = 1.0;
constexpr double kScrollSettleDistance = 0.05;
constexpr double kScrollSettleVelocity = 1.0;

} // namespace

double clampUnit(double value) {
    return std::max(0.0, std::min(1.0, value));
}

double applyEasing(EasingCurve curve, double progress) {
    const double t = clampUnit(progress);
    switch (curve) {
    case EasingCurve::Linear:
        return t;
    case EasingCurve::EaseInOutCubic:
        return t < 0.5 ? 4.0 * t * t * t : 1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0;
    case EasingCurve::EaseOutCubic:
    default:
        return 1.0 - std::pow(1.0 - t, 3.0);
    }
}

float interpolateFloat(float from, float to, double progress) {
    const double t = clampUnit(progress);
    return static_cast<float>(static_cast<double>(from) + (static_cast<double>(to) - static_cast<double>(from)) * t);
}

Color interpolateColor(Color from, Color to, double progress) {
    return Color{
        interpolateByte(from.r, to.r, progress),
        interpolateByte(from.g, to.g, progress),
        interpolateByte(from.b, to.b, progress),
        interpolateByte(from.a, to.a, progress)};
}

FloatTransition::FloatTransition(float value)
    : from_(value), value_(value), target_(value) {}

void FloatTransition::reset(float value) {
    from_ = value;
    value_ = value;
    target_ = value;
    running_ = false;
}

void FloatTransition::animateTo(float target, double nowMs, TransitionSpec spec) {
    if (target == target_ && running_) {
        return;
    }
    from_ = value_;
    target_ = target;
    startMs_ = nowMs;
    spec_ = spec;
    if (spec_.durationMs <= 0.0 || from_ == target_) {
        value_ = target_;
        running_ = false;
        return;
    }
    running_ = true;
    const double raw = normalizedProgress(nowMs + kFirstVisualFrameMs, startMs_, spec_.durationMs);
    value_ = interpolateFloat(from_, target_, applyEasing(spec_.easing, raw));
}

bool FloatTransition::tick(double nowMs) {
    if (!running_) {
        return false;
    }
    const double raw = normalizedProgress(nowMs, startMs_, spec_.durationMs);
    value_ = interpolateFloat(from_, target_, applyEasing(spec_.easing, raw));
    if (raw >= 1.0) {
        value_ = target_;
        running_ = false;
    }
    return true;
}

float FloatTransition::value() const {
    return value_;
}

float FloatTransition::target() const {
    return target_;
}

bool FloatTransition::running() const {
    return running_;
}

SmoothScrollMotion::SmoothScrollMotion(float value)
    : value_(value), target_(value), minimum_(value), maximum_(value) {}

void SmoothScrollMotion::reset(float value) {
    value_ = value;
    target_ = value;
    velocity_ = 0.0f;
    minimum_ = value;
    maximum_ = value;
    lastMs_ = 0.0;
    angularFrequency_ = 0.0;
    running_ = false;
}

bool SmoothScrollMotion::addDelta(
    float delta,
    float minimum,
    float maximum,
    double nowMs,
    ScrollMotionSpec spec) {
    if (maximum < minimum || std::fabs(delta) <= 0.0001f) {
        return false;
    }

    const bool wasRunning = running_;
    if (wasRunning) {
        tick(nowMs);
    }

    minimum_ = minimum;
    maximum_ = maximum;
    value_ = std::clamp(value_, minimum_, maximum_);
    target_ = std::clamp(target_, minimum_, maximum_);

    const float queued = target_ - value_;
    const bool reversesQueuedMotion = delta * queued < 0.0f;
    const float previousTarget = reversesQueuedMotion ? value_ : target_;
    const float nextTarget = std::clamp(previousTarget + delta, minimum_, maximum_);
    const float acceptedDelta = nextTarget - previousTarget;
    if (std::fabs(acceptedDelta) <= 0.0001f) {
        return false;
    }

    spec_ = spec;
    const double settlingDurationMs = wasRunning
        ? spec_.retargetSettlingDurationMs
        : spec_.initialSettlingDurationMs;
    const double settlingSeconds = std::max(
        settlingDurationMs,
        kMinimumScrollSettlingDurationMs) / 1000.0;
    angularFrequency_ = kCriticalDampingFivePercent / settlingSeconds;

    const bool startsNewDirection = reversesQueuedMotion
        || velocity_ * acceptedDelta < 0.0f;
    if (startsNewDirection) {
        velocity_ = 0.0f;
    }
    target_ = nextTarget;

    // Seed a responsive initial speed only when a motion begins. Packets that
    // extend an existing gesture update the target while preserving velocity;
    // adding another impulse there would count the same input twice and create
    // a visible second kick on accelerated mouse wheels.
    if (!wasRunning || startsNewDirection) {
        const double impulse = static_cast<double>(acceptedDelta)
            * angularFrequency_ * std::max(0.0, spec_.inputVelocityRatio);
        velocity_ = static_cast<float>(impulse);
    }

    const double remaining = static_cast<double>(target_ - value_);
    const double maximumVelocity = std::fabs(remaining) * angularFrequency_
        * std::max(0.0, spec_.maximumVelocityRatio);
    if (maximumVelocity > 0.0 && std::fabs(velocity_) > maximumVelocity) {
        velocity_ = static_cast<float>(std::copysign(maximumVelocity, velocity_));
    }

    lastMs_ = nowMs;
    running_ = true;
    return true;
}

bool SmoothScrollMotion::tick(double nowMs) {
    if (!running_) {
        return false;
    }

    const double elapsedSeconds = std::max(0.0, (nowMs - lastMs_) / 1000.0);
    lastMs_ = nowMs;
    if (elapsedSeconds <= 0.0) {
        return true;
    }

    const double displacement = static_cast<double>(value_ - target_);
    const double velocity = static_cast<double>(velocity_);
    const double coefficient = velocity + angularFrequency_ * displacement;
    const double decay = std::exp(-angularFrequency_ * elapsedSeconds);
    const double nextDisplacement = (displacement + coefficient * elapsedSeconds) * decay;
    const double nextVelocity =
        (velocity - angularFrequency_ * coefficient * elapsedSeconds) * decay;

    value_ = std::clamp(
        target_ + static_cast<float>(nextDisplacement),
        minimum_,
        maximum_);
    velocity_ = static_cast<float>(nextVelocity);

    if (std::fabs(target_ - value_) <= kScrollSettleDistance
        && std::fabs(velocity_) <= kScrollSettleVelocity) {
        value_ = target_;
        velocity_ = 0.0f;
        running_ = false;
    }
    return true;
}

float SmoothScrollMotion::value() const {
    return value_;
}

float SmoothScrollMotion::target() const {
    return target_;
}

float SmoothScrollMotion::velocity() const {
    return velocity_;
}

bool SmoothScrollMotion::running() const {
    return running_;
}

ColorTransition::ColorTransition(Color value)
    : from_(value), value_(value), target_(value) {}

void ColorTransition::reset(Color value) {
    from_ = value;
    value_ = value;
    target_ = value;
    running_ = false;
}

void ColorTransition::animateTo(Color target, double nowMs, TransitionSpec spec) {
    if (sameColor(target, target_) && running_) {
        return;
    }
    from_ = value_;
    // 全透明色只有 alpha 有意义：从透明淡入时采用目标的 RGB（若按字面
    // rgba(0,0,0,0) 从黑插值，亮色 hover 底会先闪过半透明深灰）；淡出到
    // 透明同理保留起点 RGB，只动 alpha。
    if (from_.a == 0 && target.a != 0) {
        from_ = Color{target.r, target.g, target.b, 0};
        value_ = from_;
    } else if (target.a == 0 && from_.a != 0) {
        target = Color{from_.r, from_.g, from_.b, 0};
    }
    target_ = target;
    startMs_ = nowMs;
    spec_ = spec;
    if (spec_.durationMs <= 0.0 || sameColor(from_, target_)) {
        value_ = target_;
        running_ = false;
        return;
    }
    running_ = true;
    const double raw = normalizedProgress(nowMs + kFirstVisualFrameMs, startMs_, spec_.durationMs);
    value_ = interpolateColor(from_, target_, applyEasing(spec_.easing, raw));
}

bool ColorTransition::tick(double nowMs) {
    if (!running_) {
        return false;
    }
    const double raw = normalizedProgress(nowMs, startMs_, spec_.durationMs);
    value_ = interpolateColor(from_, target_, applyEasing(spec_.easing, raw));
    if (raw >= 1.0) {
        value_ = target_;
        running_ = false;
    }
    return true;
}

Color ColorTransition::value() const {
    return value_;
}

Color ColorTransition::target() const {
    return target_;
}

bool ColorTransition::running() const {
    return running_;
}

} // namespace oneui
