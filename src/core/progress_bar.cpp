#include "oneui/controls/progress_bar.h"

#include "oneui/style.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace oneui {
namespace {

void applyProgressBarStyleOverride(ProgressBarStyle& style, const ProgressBarStyleOverride& overrideStyle) {
    if (overrideStyle.trackBackground) {
        style.trackBackground = *overrideStyle.trackBackground;
    }
    if (overrideStyle.fill) {
        style.fill = *overrideStyle.fill;
    }
    if (overrideStyle.disabledFill) {
        style.disabledFill = *overrideStyle.disabledFill;
    }
    if (overrideStyle.radius) {
        style.radius = *overrideStyle.radius;
    }
}

double currentTimeMs() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch()).count();
}

} // namespace

ProgressBar::ProgressBar() {
    setPreferredSize(Size{220.0f, 10.0f});
}

void ProgressBar::setValue(double value) {
    const auto next = std::clamp(value, 0.0, 1.0);
    if (valueBinding_.get(value_) == next) return;
    valueBinding_.set(next, value_);
    updateVisualValue(next);
    invalidate();
}

double ProgressBar::value() const {
    return valueBinding_.get(value_);
}

void ProgressBar::bindValue(State<double>& state) {
    valueBinding_ = Binding<double>(state, [this] {
        updateVisualValue(valueBinding_.get(value_));
        invalidate();
    });
    updateVisualValue(valueBinding_.get(value_));
    invalidate();
}

void ProgressBar::setSmooth(bool enabled, double durationMs) {
    smoothDurationMs_ = std::clamp(durationMs, 0.0, 2000.0);
    if (smooth_ == enabled) {
        return;
    }
    smooth_ = enabled;
    if (!smooth_ || !animationsEnabled_) {
        visualValue_.reset(static_cast<float>(std::clamp(value(), 0.0, 1.0)));
    }
    invalidate();
}

bool ProgressBar::smooth() const {
    return smooth_;
}

void ProgressBar::setIndeterminate(bool indeterminate) {
    if (indeterminate_ == indeterminate) {
        return;
    }
    indeterminate_ = indeterminate;
    indeterminateStartedMs_ = currentTimeMs();
    indeterminatePhase_ = 0.0;
    if (indeterminate_ && animationsEnabled_ && hasAnimationScheduler()) {
        requestAnimationFrame();
    }
    invalidate();
}

bool ProgressBar::indeterminate() const {
    return indeterminate_;
}

void ProgressBar::setAnimationsEnabled(bool enabled) {
    if (animationsEnabled_ == enabled) {
        return;
    }
    animationsEnabled_ = enabled;
    if (!animationsEnabled_) {
        visualValue_.reset(static_cast<float>(std::clamp(value(), 0.0, 1.0)));
        indeterminatePhase_ = 0.38;
    } else if (indeterminate_ && hasAnimationScheduler()) {
        indeterminateStartedMs_ = currentTimeMs();
        requestAnimationFrame();
    }
    invalidate();
}

bool ProgressBar::animationsEnabled() const {
    return animationsEnabled_;
}

void ProgressBar::setStyleOverride(ProgressBarStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void ProgressBar::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

void ProgressBar::paint(Canvas& canvas) {
    const auto style = resolvedStyle();
    const Rect rect = frame();
    const float progress = static_cast<float>(clampedValue());

    canvas.fillRect(rect, style.trackBackground, style.radius);
    const Color fill = disabled() ? style.disabledFill : style.fill;
    if (indeterminate_) {
        const float fraction = 0.28f;
        const float position = static_cast<float>(indeterminatePhase_);
        if (rect.height > rect.width * 1.5f) {
            const float fillHeight = rect.height * fraction;
            const float travel = rect.height + fillHeight;
            const float y = rect.y - fillHeight + travel * position;
            canvas.fillRect(Rect{rect.x, y, rect.width, fillHeight}, fill, style.radius);
        } else {
            const float fillWidth = rect.width * fraction;
            const float travel = rect.width + fillWidth;
            const float x = rect.x - fillWidth + travel * position;
            canvas.fillRect(Rect{x, rect.y, fillWidth, rect.height}, fill, style.radius);
        }
        return;
    }
    if (progress <= 0.0f) {
        return;
    }

    if (rect.height > rect.width * 1.5f) {
        const float fillHeight = rect.height * progress;
        canvas.fillRect(
            Rect{rect.x, rect.y + rect.height - fillHeight, rect.width, fillHeight},
            fill,
            style.radius);
    } else {
        const float fillWidth = rect.width * progress;
        canvas.fillRect(Rect{rect.x, rect.y, fillWidth, rect.height}, fill, style.radius);
    }
}

bool ProgressBar::tickAnimations(double nowMs) {
    bool running = false;
    if (smooth_ && animationsEnabled_) {
        running = visualValue_.tick(nowMs) || running;
    }
    if (indeterminate_ && animationsEnabled_) {
        if (indeterminateStartedMs_ <= 0.0) {
            indeterminateStartedMs_ = nowMs;
        }
        indeterminatePhase_ = std::fmod(std::max(0.0, nowMs - indeterminateStartedMs_), 1200.0) / 1200.0;
        running = true;
    }
    if (running) {
        invalidate();
    }
    return running;
}

void ProgressBar::updateVisualValue(double next) {
    const auto clamped = static_cast<float>(std::clamp(next, 0.0, 1.0));
    if (!smooth_ || !animationsEnabled_ || !hasAnimationScheduler() || smoothDurationMs_ <= 0.0) {
        visualValue_.reset(clamped);
        return;
    }
    visualValue_.animateTo(
        clamped,
        currentTimeMs(),
        TransitionSpec{smoothDurationMs_, EasingCurve::EaseOutCubic});
    if (visualValue_.running()) {
        requestAnimationFrame();
    }
}

double ProgressBar::clampedValue() const {
    if (smooth_ && animationsEnabled_) {
        return std::clamp(static_cast<double>(visualValue_.value()), 0.0, 1.0);
    }
    return std::clamp(value(), 0.0, 1.0);
}

ProgressBarStyle ProgressBar::resolvedStyle() const {
    ProgressBarStyle style{};
    if (styleOverride_) {
        applyProgressBarStyleOverride(style, *styleOverride_);
    }
    return style;
}

} // namespace oneui
