#include "oneui/controls/sparkline.h"

#include "oneui/style.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {

Sparkline::Sparkline() {
    setPreferredSize(Size{220.0f, 72.0f});
}

void Sparkline::setValues(std::vector<double> values) {
    for (double& value : values) {
        value = std::clamp(std::isfinite(value) ? value : 0.0, 0.0, 1.0);
    }
    values_ = std::move(values);
    invalidate();
}

const std::vector<double>& Sparkline::values() const {
    return values_;
}

void Sparkline::setStyleBox(StyleBox style) {
    styleBox_ = std::move(style);
    invalidate();
}

void Sparkline::clearStyleBox() {
    styleBox_.reset();
    invalidate();
}

void Sparkline::paint(Canvas& canvas) {
    const Rect rect = frame();
    const Color background = styleBox_ && styleBox_->background.color
        ? *styleBox_->background.color
        : theme().surface;
    const Color line = styleBox_ && styleBox_->foreground
        ? *styleBox_->foreground
        : theme().primary;
    const Color disabledLine = theme().disabledForeground;
    const Color grid = styleBox_ && styleBox_->borderColor
        ? *styleBox_->borderColor
        : theme().border;
    const float radius = styleBox_ && styleBox_->radius
        ? *styleBox_->radius
        : theme().radiusMd;
    const float strokeWidth = styleBox_ && styleBox_->borderWidth
        ? std::max(1.0f, *styleBox_->borderWidth)
        : 2.0f;

    canvas.fillRect(rect, background, radius);
    if (rect.width <= 8.0f || rect.height <= 8.0f) {
        return;
    }

    const Rect plot{rect.x + 4.0f, rect.y + 4.0f, rect.width - 8.0f, rect.height - 8.0f};
    const Color subtleGrid{grid.r, grid.g, grid.b, static_cast<unsigned char>(std::min(96, static_cast<int>(grid.a)))};
    canvas.drawLine(
        Point{plot.x, plot.y + plot.height * 0.5f},
        Point{plot.x + plot.width, plot.y + plot.height * 0.5f},
        subtleGrid,
        1.0f);

    if (values_.empty()) {
        return;
    }

    const Color series = disabled() ? disabledLine : line;
    const auto pointAt = [&](std::size_t index) {
        const float progress = values_.size() <= 1
            ? 1.0f
            : static_cast<float>(index) / static_cast<float>(values_.size() - 1);
        const float value = static_cast<float>(values_[index]);
        return Point{
            plot.x + plot.width * progress,
            plot.y + plot.height * (1.0f - value),
        };
    };

    Point previous = pointAt(0);
    for (std::size_t index = 1; index < values_.size(); ++index) {
        const Point current = pointAt(index);
        canvas.drawLine(previous, current, series, strokeWidth);
        previous = current;
    }
    canvas.fillEllipse(
        Rect{previous.x - 2.5f, previous.y - 2.5f, 5.0f, 5.0f},
        series);
}

} // namespace oneui
