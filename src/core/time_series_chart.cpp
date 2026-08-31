#include "oneui/controls/time_series_chart.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

namespace oneui {

TimeSeriesChart::TimeSeriesChart() {
    setPreferredSize(Size{360.0f, 180.0f});
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(L"time series chart");
    setAccessibleDescription(L"Use left and right arrow keys to inspect samples");
}

void TimeSeriesChart::setSeries(std::vector<TimeSeriesChartSeries> series) {
    series_ = std::move(series);
    const auto count = maximumSampleCount();
    if (inspectionIndex_ >= static_cast<int>(count)) {
        inspectionIndex_ = count == 0 ? -1 : static_cast<int>(count - 1);
    }
    refreshAccessibleValue();
    invalidate();
}

const std::vector<TimeSeriesChartSeries>& TimeSeriesChart::series() const {
    return series_;
}

void TimeSeriesChart::setRange(double minimum, double maximum) {
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || maximum <= minimum) {
        minimum = 0.0;
        maximum = 1.0;
    }
    minimum_ = minimum;
    maximum_ = maximum;
    invalidate();
}

double TimeSeriesChart::minimum() const {
    return minimum_;
}

double TimeSeriesChart::maximum() const {
    return maximum_;
}

void TimeSeriesChart::setGridLines(int count) {
    gridLines_ = std::clamp(count, 2, 8);
    invalidate();
}

int TimeSeriesChart::gridLines() const {
    return gridLines_;
}

void TimeSeriesChart::setSmoothCurves(bool enabled) {
    smoothCurves_ = enabled;
    invalidate();
}

void TimeSeriesChart::setAreaFill(bool enabled) {
    areaFill_ = enabled;
    invalidate();
}

void TimeSeriesChart::setDashedGrid(bool enabled) {
    dashedGrid_ = enabled;
    invalidate();
}

void TimeSeriesChart::setAxesVisible(bool visible) {
    axesVisible_ = visible;
    invalidate();
}

void TimeSeriesChart::setLineWidth(float width) {
    lineWidth_ = std::clamp(width, 1.0f, 6.0f);
    invalidate();
}

void TimeSeriesChart::setFillAlpha(unsigned char alpha) {
    fillAlpha_ = alpha;
    invalidate();
}

void TimeSeriesChart::setPlotInsets(Insets insets) {
    plotInsets_ = Insets{
        std::max(0.0f, insets.top),
        std::max(0.0f, insets.right),
        std::max(0.0f, insets.bottom),
        std::max(0.0f, insets.left)};
    invalidate();
}

void TimeSeriesChart::setThresholds(std::vector<TimeSeriesChartThreshold> thresholds) {
    thresholds.erase(
        std::remove_if(
            thresholds.begin(),
            thresholds.end(),
            [](const TimeSeriesChartThreshold& threshold) {
                return !std::isfinite(threshold.value);
            }),
        thresholds.end());
    thresholds_ = std::move(thresholds);
    invalidate();
}

const std::vector<TimeSeriesChartThreshold>& TimeSeriesChart::thresholds() const {
    return thresholds_;
}

void TimeSeriesChart::setInspectionIndex(int index, bool pinned) {
    updateInspection(index, pinned, false);
}

int TimeSeriesChart::inspectionIndex() const {
    return inspectionIndex_;
}

bool TimeSeriesChart::inspectionPinned() const {
    return inspectionPinned_;
}

void TimeSeriesChart::setOnInspectionChanged(std::function<void(int, bool)> callback) {
    onInspectionChanged_ = std::move(callback);
}

Rect TimeSeriesChart::plotRect() const {
    return frame().inset(plotInsets_);
}

std::size_t TimeSeriesChart::maximumSampleCount() const {
    std::size_t count = 0;
    for (const auto& item : series_) {
        count = std::max(count, item.values.size());
    }
    return count;
}

int TimeSeriesChart::inspectionIndexAt(Point point) const {
    const auto count = maximumSampleCount();
    const Rect rect = plotRect();
    if (count == 0 || rect.width <= 0.0f || point.x < rect.x || point.x > rect.x + rect.width ||
        point.y < rect.y || point.y > rect.y + rect.height) {
        return -1;
    }
    if (count == 1) {
        return 0;
    }
    const double progress = std::clamp(
        static_cast<double>(point.x - rect.x) / static_cast<double>(rect.width),
        0.0,
        1.0);
    return static_cast<int>(std::llround(progress * static_cast<double>(count - 1)));
}

Rect TimeSeriesChart::inspectionDamageRect(int index) const {
    const auto count = maximumSampleCount();
    const Rect rect = plotRect();
    if (index < 0 || count == 0 || rect.width <= 0.0f || rect.height <= 0.0f) {
        return Rect{};
    }
    const float progress = count <= 1
        ? 1.0f
        : static_cast<float>(index) / static_cast<float>(count - 1);
    const float x = rect.x + rect.width * progress;
    return Rect{x - 7.0f, rect.y - 3.0f, 14.0f, rect.height + 6.0f};
}

void TimeSeriesChart::refreshAccessibleValue() {
    if (inspectionIndex_ < 0) {
        setAccessibleValue(L"");
        setTooltip(L"");
        return;
    }
    std::wostringstream text;
    text << L"Sample " << (inspectionIndex_ + 1);
    for (const auto& item : series_) {
        if (inspectionIndex_ >= static_cast<int>(item.values.size())) {
            continue;
        }
        const double value = item.values[static_cast<std::size_t>(inspectionIndex_)];
        if (!std::isfinite(value)) {
            continue;
        }
        text << L", " << item.name << L" " << std::fixed << std::setprecision(1) << value;
    }
    if (inspectionPinned_) {
        text << L", pinned";
    }
    setAccessibleValue(text.str());
    setTooltip(text.str());
}

void TimeSeriesChart::updateInspection(int index, bool pinned, bool notify) {
    const auto count = maximumSampleCount();
    if (count == 0) {
        index = -1;
        pinned = false;
    } else if (index >= 0) {
        index = std::clamp(index, 0, static_cast<int>(count - 1));
    } else {
        pinned = false;
    }
    if (inspectionIndex_ == index && inspectionPinned_ == pinned) {
        return;
    }
    const Rect oldDamage = inspectionDamageRect(inspectionIndex_);
    inspectionIndex_ = index;
    inspectionPinned_ = pinned;
    refreshAccessibleValue();
    const Rect newDamage = inspectionDamageRect(inspectionIndex_);
    if (oldDamage.width > 0.0f) {
        invalidateRect(oldDamage);
    }
    if (newDamage.width > 0.0f) {
        invalidateRect(newDamage);
    }
    if (notify && onInspectionChanged_) {
        onInspectionChanged_(inspectionIndex_, inspectionPinned_);
    }
}

void TimeSeriesChart::paint(Canvas& canvas) {
    const Rect rect = plotRect();
    if (rect.width <= 2.0f || rect.height <= 2.0f) {
        return;
    }

    const Color grid{theme().border.r, theme().border.g, theme().border.b, 118};
    const auto drawGrid = [&](Point from, Point to) {
        if (!dashedGrid_) {
            canvas.drawLine(from, to, grid, 1.0f);
            return;
        }
        const bool horizontal = std::abs(to.x - from.x) >= std::abs(to.y - from.y);
        const float length = horizontal ? std::abs(to.x - from.x) : std::abs(to.y - from.y);
        const float direction = horizontal ? (to.x >= from.x ? 1.0f : -1.0f) : (to.y >= from.y ? 1.0f : -1.0f);
        for (float offset = 0.0f; offset < length; offset += 7.0f) {
            const float end = std::min(length, offset + 3.0f);
            if (horizontal) {
                canvas.drawLine(
                    Point{from.x + direction * offset, from.y},
                    Point{from.x + direction * end, from.y},
                    grid,
                    1.0f);
            } else {
                canvas.drawLine(
                    Point{from.x, from.y + direction * offset},
                    Point{from.x, from.y + direction * end},
                    grid,
                    1.0f);
            }
        }
    };
    for (int index = 0; index < gridLines_; ++index) {
        const float progress = gridLines_ <= 1
            ? 0.0f
            : static_cast<float>(index) / static_cast<float>(gridLines_ - 1);
        const float y = rect.y + rect.height * progress;
        drawGrid(Point{rect.x, y}, Point{rect.x + rect.width, y});
    }
    for (int index = 1; index < 6; ++index) {
        const float x = rect.x + rect.width * static_cast<float>(index) / 6.0f;
        drawGrid(Point{x, rect.y}, Point{x, rect.y + rect.height});
    }
    if (axesVisible_) {
        const Color axis{theme().border.r, theme().border.g, theme().border.b, 205};
        canvas.drawLine(Point{rect.x, rect.y}, Point{rect.x, rect.y + rect.height}, axis, 1.0f);
        canvas.drawLine(
            Point{rect.x, rect.y + rect.height},
            Point{rect.x + rect.width, rect.y + rect.height},
            axis,
            1.0f);
    }

    const double span = std::max(0.000001, maximum_ - minimum_);
    for (const auto& threshold : thresholds_) {
        if (threshold.value < minimum_ || threshold.value > maximum_) {
            continue;
        }
        const double normalized = (threshold.value - minimum_) / span;
        const float y = rect.y + rect.height * static_cast<float>(1.0 - normalized);
        for (float x = rect.x; x < rect.x + rect.width; x += 8.0f) {
            canvas.drawLine(
                Point{x, y},
                Point{std::min(rect.x + rect.width, x + 4.0f), y},
                threshold.color,
                1.0f);
        }
    }

    canvas.save();
    canvas.clipRect(rect);
    for (const auto& item : series_) {
        if (item.values.empty()) {
            continue;
        }
        const Color color = disabled() ? theme().disabledForeground : item.color;
        const auto pointAt = [&](std::size_t index) {
            const float xProgress = item.values.size() <= 1
                ? 1.0f
                : static_cast<float>(index) / static_cast<float>(item.values.size() - 1);
            const double normalized = std::clamp((item.values[index] - minimum_) / span, 0.0, 1.0);
            return Point{
                rect.x + rect.width * xProgress,
                rect.y + rect.height * static_cast<float>(1.0 - normalized)};
        };
        std::size_t index = 0;
        while (index < item.values.size()) {
            while (index < item.values.size() && !std::isfinite(item.values[index])) {
                ++index;
            }
            const std::size_t start = index;
            while (index < item.values.size() && std::isfinite(item.values[index])) {
                ++index;
            }
            const std::size_t end = index;
            if (start == end) {
                continue;
            }
            std::vector<Point> points;
            points.reserve(end - start);
            for (std::size_t valueIndex = start; valueIndex < end; ++valueIndex) {
                points.push_back(pointAt(valueIndex));
            }
            CanvasPath linePath;
            linePath.moveTo(points.front());
            for (std::size_t pointIndex = 1; pointIndex < points.size(); ++pointIndex) {
                if (!smoothCurves_ || points.size() < 3) {
                    linePath.lineTo(points[pointIndex]);
                    continue;
                }
                const Point p0 = points[pointIndex > 1 ? pointIndex - 2 : pointIndex - 1];
                const Point p1 = points[pointIndex - 1];
                const Point p2 = points[pointIndex];
                const Point p3 = points[pointIndex + 1 < points.size() ? pointIndex + 1 : pointIndex];
                constexpr float tension = 1.0f / 6.0f;
                linePath.cubicTo(
                    Point{p1.x + (p2.x - p0.x) * tension, p1.y + (p2.y - p0.y) * tension},
                    Point{p2.x - (p3.x - p1.x) * tension, p2.y - (p3.y - p1.y) * tension},
                    p2);
            }
            if (areaFill_ && points.size() > 1) {
                CanvasPath areaPath = linePath;
                areaPath.lineTo(Point{points.back().x, rect.y + rect.height});
                areaPath.lineTo(Point{points.front().x, rect.y + rect.height});
                areaPath.close();
                Color fillStart = color;
                fillStart.a = fillAlpha_;
                Color fillEnd = color;
                fillEnd.a = 0;
                canvas.fillPathLinearGradient(areaPath, rect, fillStart, fillEnd, 90.0f);
            }
            canvas.strokePath(linePath, color, lineWidth_, true);
        }
    }

    const auto sampleCount = maximumSampleCount();
    if (inspectionIndex_ >= 0 && sampleCount > 0) {
        const float progress = sampleCount <= 1
            ? 1.0f
            : static_cast<float>(inspectionIndex_) / static_cast<float>(sampleCount - 1);
        const float x = rect.x + rect.width * progress;
        const Color crosshair = inspectionPinned_
            ? Color{theme().primary.r, theme().primary.g, theme().primary.b, 220}
            : Color{theme().text.r, theme().text.g, theme().text.b, 118};
        canvas.drawLine(Point{x, rect.y}, Point{x, rect.y + rect.height}, crosshair, 1.0f);
        for (const auto& item : series_) {
            if (inspectionIndex_ >= static_cast<int>(item.values.size())) {
                continue;
            }
            const double value = item.values[static_cast<std::size_t>(inspectionIndex_)];
            if (!std::isfinite(value)) {
                continue;
            }
            const double normalized = std::clamp((value - minimum_) / span, 0.0, 1.0);
            const float y = rect.y + rect.height * static_cast<float>(1.0 - normalized);
            canvas.fillEllipse(Rect{x - 3.5f, y - 3.5f, 7.0f, 7.0f}, theme().surface);
            canvas.fillEllipse(Rect{x - 2.0f, y - 2.0f, 4.0f, 4.0f}, item.color);
        }
    }
    canvas.restore();

    if (focused() && focusVisible()) {
        canvas.strokeRect(
            frame().inset(Insets{1.0f, 1.0f, 1.0f, 1.0f}),
            theme().focusOutline,
            3.0f,
            1.0f);
    }
}

bool TimeSeriesChart::onMouseMove(const MouseEvent& event) {
    if (disabled() || inspectionPinned_) {
        return false;
    }
    const int index = inspectionIndexAt(event.position);
    updateInspection(index, false, true);
    return index >= 0;
}

bool TimeSeriesChart::onMouseDown(const MouseEvent& event) {
    if (disabled() || event.button != MouseButton::Left || !contains(event.position)) {
        return false;
    }
    pressed_ = true;
    setFocused(true);
    setFocusVisible(false);
    return true;
}

bool TimeSeriesChart::onMouseUp(const MouseEvent& event) {
    if (!pressed_) {
        return false;
    }
    pressed_ = false;
    if (event.button != MouseButton::Left || !contains(event.position)) {
        return true;
    }
    const int index = inspectionIndexAt(event.position);
    const bool unpin = inspectionPinned_ && index == inspectionIndex_;
    updateInspection(unpin ? -1 : index, !unpin && index >= 0, true);
    return true;
}

bool TimeSeriesChart::onKeyDown(const KeyEvent& event) {
    if (disabled() || !event.pressed) {
        return false;
    }
    const int count = static_cast<int>(maximumSampleCount());
    if (count <= 0) {
        return false;
    }
    if (event.key == Key::Left || event.key == Key::Right) {
        int index = inspectionIndex_ < 0 ? count - 1 : inspectionIndex_;
        index += event.key == Key::Left ? -1 : 1;
        updateInspection(std::clamp(index, 0, count - 1), true, true);
        return true;
    }
    if (event.key == Key::Home || event.key == Key::End) {
        updateInspection(event.key == Key::Home ? 0 : count - 1, true, true);
        return true;
    }
    if (event.key == Key::Enter || event.key == Key::Space) {
        updateInspection(
            inspectionIndex_ < 0 ? count - 1 : inspectionIndex_,
            !inspectionPinned_,
            true);
        return true;
    }
    if (event.key == Key::Escape) {
        updateInspection(-1, false, true);
        return true;
    }
    return false;
}

bool TimeSeriesChart::onFocusChanged(bool focused) {
    setFocused(focused);
    if (!focused && !inspectionPinned_) {
        updateInspection(-1, false, true);
    }
    invalidate();
    return true;
}

bool TimeSeriesChart::isFocusable() const {
    return !disabled();
}

CursorKind TimeSeriesChart::cursor(Point point) const {
    return !disabled() && contains(point) ? CursorKind::Crosshair : CursorKind::Default;
}

bool TimeSeriesChart::hasInteractionState() const {
    return pressed_ || inspectionIndex_ >= 0;
}

void TimeSeriesChart::resetInteractionState() {
    pressed_ = false;
    updateInspection(-1, false, true);
}

} // namespace oneui
