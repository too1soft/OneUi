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
