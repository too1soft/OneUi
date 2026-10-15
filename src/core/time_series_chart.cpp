#include "oneui/controls/time_series_chart.h"
#include "internal/ui_clock.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace oneui {

namespace {
constexpr double liveTransitionMs = 450.0;
// A live chart that stops receiving data stops sliding after this long.
constexpr double liveMaximumScrollSeconds = 12.0;
double easeOut(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
}
}

TimeSeriesChart::TimeSeriesChart() {
    setPreferredSize(Size{360.0f, 180.0f});
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(L"time series chart");
    setAccessibleDescription(L"Use left and right arrow keys to inspect samples");
}

void TimeSeriesChart::setSeries(std::vector<TimeSeriesChartSeries> series) {
    // Resending the same data is not an update; a live chart keeps sliding.
    if (series == series_) return;
    if (scrollRate_ > 0.0) {
        // Each series' newest value eases in from what is on screen now.
        const double now = internal::uiTimeMs();
        std::vector<double> from(series.size(), std::numeric_limits<double>::quiet_NaN());
        for (std::size_t index = 0; index < series.size() && index < series_.size(); ++index)
            if (!series_[index].values.empty()) from[index] = displayedLast(index, now);
        revealFrom_ = std::move(from);
        revealStartMs_ = now;
        dataTimeMs_ = now;
        requestAnimationFrame();
    }
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
    if (scrollRate_ > 0.0 && minimum == minimum_ && maximum != maximum_) {
        const double now = internal::uiTimeMs();
        rangeFromMax_ = displayedMaximum(now);
        rangeStartMs_ = now;
        requestAnimationFrame();
    }
    minimum_ = minimum;
    maximum_ = maximum;
    invalidate();
}

void TimeSeriesChart::setScrollRate(double rate) {
    scrollRate_ = std::isfinite(rate) && rate > 0.0 ? rate : 0.0;
    dataTimeMs_ = internal::uiTimeMs();
    if (scrollRate_ > 0.0) requestAnimationFrame();
    invalidate();
}

double TimeSeriesChart::scrollRate() const {
    return scrollRate_;
}

double TimeSeriesChart::liveShift(double nowMs) const {
    if (scrollRate_ <= 0.0) return 0.0;
    return scrollRate_ * std::clamp((nowMs - dataTimeMs_) / 1000.0, 0.0, liveMaximumScrollSeconds);
}

double TimeSeriesChart::displayedMaximum(double nowMs) const {
    if (rangeStartMs_ < 0.0 || nowMs - rangeStartMs_ >= liveTransitionMs) return maximum_;
    return rangeFromMax_ + (maximum_ - rangeFromMax_) * easeOut((nowMs - rangeStartMs_) / liveTransitionMs);
}

double TimeSeriesChart::displayedLast(std::size_t series, double nowMs) const {
    const double value = series_[series].values.back();
    if (revealStartMs_ < 0.0 || series >= revealFrom_.size() || !std::isfinite(revealFrom_[series]) ||
        !std::isfinite(value) || nowMs - revealStartMs_ >= liveTransitionMs)
        return value;
    const double from = revealFrom_[series];
    return from + (value - from) * easeOut((nowMs - revealStartMs_) / liveTransitionMs);
}

bool TimeSeriesChart::tickAnimations(double nowMs) {
    const bool easing = (rangeStartMs_ >= 0.0 && nowMs - rangeStartMs_ < liveTransitionMs) ||
        (revealStartMs_ >= 0.0 && nowMs - revealStartMs_ < liveTransitionMs);
    const bool sliding = scrollRate_ > 0.0 && nowMs - dataTimeMs_ < liveMaximumScrollSeconds * 1000.0;
    if (!easing && !sliding) return false;
    // Slow slides repaint only once they have moved by half a pixel.
    const float shift = static_cast<float>(liveShift(nowMs)) * plotRect().width;
    if (easing || std::abs(shift - paintedShiftPx_) >= 0.5f) invalidate();
    requestAnimationFrame();
    return true;
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

void TimeSeriesChart::setGridStyle(int verticalLines, float lineWidth) {
    verticalGridLines_ = std::clamp(verticalLines, 0, 16);
    gridLineWidth_ = std::isfinite(lineWidth) ? std::clamp(lineWidth, 0.1f, 6.0f) : 1.0f;
    invalidate();
}

void TimeSeriesChart::setAxisLabels(std::vector<std::wstring> labels) {
    axisLabels_ = std::move(labels);
    invalidate();
}

void TimeSeriesChart::setStyleBox(const StyleBox& style) {
    gridColor_ = style.borderColor;
    axisLabelColor_ = style.placeholderColor ? style.placeholderColor : style.foreground;
    axisLabelSize_ = style.fontSize.value_or(11.0f);
    axisLabelGap_ = std::max(0.0f, style.gap.value_or(6.0f));
    invalidate();
}

bool TimeSeriesChart::setSamplePositions(std::vector<double> positions) {
    if (!std::is_sorted(positions.begin(), positions.end()) ||
        std::any_of(positions.begin(), positions.end(), [](double p) {
            return !std::isfinite(p) || p < 0.0 || p > 1.0;
        })) return false;
    if (positions == samplePositions_) return true;
    samplePositions_ = std::move(positions);
    if (scrollRate_ > 0.0) dataTimeMs_ = internal::uiTimeMs();
    invalidate();
    return true;
}

void TimeSeriesChart::setLatestPointVisible(bool visible) {
    latestPointVisible_ = visible;
    invalidate();
}

void TimeSeriesChart::setLatestPointSize(float width, float height) {
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f) return;
    latestPointWidth_ = std::min(width, 128.0f);
    latestPointHeight_ = std::min(height, 128.0f);
    invalidate();
}

float TimeSeriesChart::sampleProgress(std::size_t index, std::size_t count) const {
    if (count == samplePositions_.size() && index < count) return static_cast<float>(samplePositions_[index]);
    return count <= 1 ? 1.0f : static_cast<float>(index) / static_cast<float>(count - 1);
}

void TimeSeriesChart::setSmoothCurves(bool enabled) {
    smoothCurves_ = enabled;
    invalidate();
}

void TimeSeriesChart::setAreaFill(bool enabled) {
    areaFill_ = enabled;
    invalidate();
}

void TimeSeriesChart::setFillExtent(TimeSeriesFillExtent extent) {
    fillExtent_ = extent;
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
        static_cast<double>(point.x - rect.x) / static_cast<double>(rect.width) + liveShift(internal::uiTimeMs()),
        0.0,
        1.0);
    if (samplePositions_.size() == count) {
        const auto it = std::lower_bound(samplePositions_.begin(), samplePositions_.end(), progress);
        if (it == samplePositions_.begin()) return 0;
        if (it == samplePositions_.end()) return static_cast<int>(count - 1);
        const auto index = static_cast<int>(it - samplePositions_.begin());
        return progress - *(it - 1) <= *it - progress ? index - 1 : index;
    }
    return static_cast<int>(std::llround(progress * static_cast<double>(count - 1)));
}

Rect TimeSeriesChart::inspectionDamageRect(int index) const {
    const auto count = maximumSampleCount();
    const Rect rect = plotRect();
    if (index < 0 || count == 0 || rect.width <= 0.0f || rect.height <= 0.0f) {
        return Rect{};
    }
    const float progress = sampleProgress(static_cast<std::size_t>(index), count);
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

    const Color grid = gridColor_.value_or(Color{theme().border.r, theme().border.g, theme().border.b, 118});
    const auto drawGrid = [&](Point from, Point to) {
        if (!dashedGrid_) {
            canvas.drawLine(from, to, grid, gridLineWidth_);
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
                    gridLineWidth_);
            } else {
                canvas.drawLine(
                    Point{from.x, from.y + direction * offset},
                    Point{from.x, from.y + direction * end},
                    grid,
                    gridLineWidth_);
            }
        }
    };
    for (int index = 0; index < gridLines_; ++index) {
        const float progress = gridLines_ <= 1
            ? 0.0f
            : static_cast<float>(index) / static_cast<float>(gridLines_ - 1);
        const float y = rect.y + rect.height * progress;
        drawGrid(Point{rect.x, y}, Point{rect.x + rect.width, y});
        if (static_cast<std::size_t>(index) < axisLabels_.size() && plotInsets_.left > 6.0f) {
            canvas.drawTextStyledEllipsized(axisLabels_[index],
                Rect{frame().x, y - axisLabelSize_ * 0.5f, std::max(0.0f, plotInsets_.left - axisLabelGap_), axisLabelSize_},
                disabled() ? theme().disabledForeground : axisLabelColor_.value_or(theme().textMuted), axisLabelSize_, TextAlign::Left, 400);
        }
    }
    for (int index = 1; index <= verticalGridLines_; ++index) {
        const float x = rect.x + rect.width * static_cast<float>(index) / static_cast<float>(verticalGridLines_ + 1);
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

    const double nowMs = internal::uiTimeMs();
    const float shift = static_cast<float>(liveShift(nowMs));
    paintedShiftPx_ = shift * rect.width;
    const double shownMaximum = displayedMaximum(nowMs);
    const double span = std::max(0.000001, shownMaximum - minimum_);
    for (const auto& threshold : thresholds_) {
        if (threshold.value < minimum_ || threshold.value > shownMaximum) {
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
    const float dotInsetX = std::max(3.0f, latestPointWidth_ * 0.5f);
    const float dotInsetY = std::max(3.0f, latestPointHeight_ * 0.5f);
    // A sliding chart clips exactly at its left edge so old samples leave cleanly.
    const float clipLeft = shift > 0.0f ? rect.x : rect.x - dotInsetX;
    canvas.clipRect(Rect{clipLeft, rect.y - dotInsetY, rect.x + rect.width + dotInsetX - clipLeft, rect.height + 2 * dotInsetY});
    for (std::size_t seriesIndex = 0; seriesIndex < series_.size(); ++seriesIndex) {
        const auto& item = series_[seriesIndex];
        if (item.values.empty()) {
            continue;
        }
        const Color color = disabled() ? theme().disabledForeground : item.color;
        const std::size_t last = item.values.size() - 1;
        const auto yAt = [&](double value) {
            const double normalized = std::clamp((value - minimum_) / span, 0.0, 1.0);
            return rect.y + rect.height * static_cast<float>(1.0 - normalized);
        };
        const auto pointAt = [&](std::size_t index) {
            const float xProgress = sampleProgress(index, item.values.size()) - shift;
            const double value = index == last ? displayedLast(seriesIndex, nowMs) : item.values[index];
            return Point{rect.x + rect.width * xProgress, yAt(value)};
        };
        // While a live chart slides, the newest value holds out to where it
        // was drawn, so the line still reaches the right edge.
        const auto holdPoint = [&]() {
            const auto newest = pointAt(last);
            return Point{rect.x + rect.width * sampleProgress(last, item.values.size()), newest.y};
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
            if (end == item.values.size() && shift > 0.0f) {
                const auto hold = holdPoint();
                if (hold.x > points.back().x + 0.5f) points.push_back(hold);
            }
            CanvasPath linePath;
            linePath.moveTo(points.front());
            if (!smoothCurves_ || points.size() < 3) {
                for (std::size_t pointIndex = 1; pointIndex < points.size(); ++pointIndex)
                    linePath.lineTo(points[pointIndex]);
            } else {
                // Monotone cubic (Steffen): smooth, but never overshoots
                // between samples, so a curve cannot dip below its data.
                const std::size_t count = points.size();
                std::vector<float> slopes(count, 0.0f);
                const auto secant = [&](std::size_t k) {
                    const float h = points[k + 1].x - points[k].x;
                    return h > 0.0001f ? (points[k + 1].y - points[k].y) / h : 0.0f;
                };
                for (std::size_t k = 1; k + 1 < count; ++k) {
                    const float h0 = points[k].x - points[k - 1].x;
                    const float h1 = points[k + 1].x - points[k].x;
                    const float s0 = secant(k - 1);
                    const float s1 = secant(k);
                    if (s0 * s1 <= 0.0f || h0 + h1 <= 0.0001f) continue;
                    const float p = (s0 * h1 + s1 * h0) / (h0 + h1);
                    const float bound = std::min({std::abs(s0), std::abs(s1), 0.5f * std::abs(p)});
                    slopes[k] = (s0 > 0.0f ? 1.0f : -1.0f) * 2.0f * bound;
                }
                const auto endSlope = [&](std::size_t neighbor, std::size_t secantIndex) {
                    const float s = secant(secantIndex);
                    const float m = (3.0f * s - slopes[neighbor]) * 0.5f;
                    return m * s <= 0.0f ? 0.0f : (std::abs(m) > 3.0f * std::abs(s) ? 3.0f * s : m);
                };
                slopes.front() = endSlope(1, 0);
                slopes.back() = endSlope(count - 2, count - 2);
                for (std::size_t k = 0; k + 1 < count; ++k) {
                    const float third = (points[k + 1].x - points[k].x) / 3.0f;
                    linePath.cubicTo(
                        Point{points[k].x + third, points[k].y + slopes[k] * third},
                        Point{points[k + 1].x - third, points[k + 1].y - slopes[k + 1] * third},
                        points[k + 1]);
                }
            }
            if (item.fill.value_or(areaFill_) && points.size() > 1) {
                CanvasPath areaPath = linePath;
                areaPath.lineTo(Point{points.back().x, rect.y + rect.height});
                areaPath.lineTo(Point{points.front().x, rect.y + rect.height});
                areaPath.close();
                Color fillStart = color;
                fillStart.a = fillAlpha_;
                Color fillEnd = color;
                fillEnd.a = 0;
                Rect gradient = rect;
                if (fillExtent_ == TimeSeriesFillExtent::Series) {
                    float top = rect.y + rect.height;
                    for (const auto& point : points) top = std::min(top, point.y);
                    // A flat series still gets a few pixels of gradient.
                    top = std::min(top, rect.y + rect.height - 4.0f);
                    gradient = Rect{rect.x, top, rect.width, rect.y + rect.height - top};
                }
                // CSS angles: 180 degrees runs from the curve down to the baseline.
                canvas.fillPathLinearGradient(areaPath, gradient, fillStart, fillEnd, 180.0f);
            }
            if (item.dashed) {
                canvas.strokePathDashed(linePath, color, lineWidth_, 4.0f, 3.0f);
            } else {
                canvas.strokePath(linePath, color, lineWidth_, true);
            }
        }
        if (latestPointVisible_ && std::isfinite(item.values.back())) {
            const auto point = shift > 0.0f ? holdPoint() : pointAt(last);
            canvas.fillEllipse(Rect{point.x - latestPointWidth_ * 0.5f, point.y - latestPointHeight_ * 0.5f, latestPointWidth_, latestPointHeight_}, color);
        }
    }

    const auto sampleCount = maximumSampleCount();
    if (inspectionIndex_ >= 0 && sampleCount > 0) {
        const float progress = sampleProgress(static_cast<std::size_t>(inspectionIndex_), sampleCount) - shift;
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
