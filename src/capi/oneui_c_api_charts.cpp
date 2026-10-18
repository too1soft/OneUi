#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/controls/slider.h"
#include "oneui/controls/sparkline.h"
#include "oneui/controls/time_series_chart.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <cwchar>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace oneui::capi;

OneUiWidget* oneui_slider_create() {
    return wrap(std::make_shared<oneui::Slider>());
}

void oneui_slider_set_range(OneUiWidget* slider, double minimum, double maximum) {
    if (auto* native = asWidget<oneui::Slider>(slider)) { native->setRange(minimum, maximum); }
}

void oneui_slider_set_step(OneUiWidget* slider, double step) {
    if (auto* native = asWidget<oneui::Slider>(slider)) { native->setStep(step); }
}

void oneui_slider_set_value(OneUiWidget* slider, double value) {
    if (auto* native = asWidget<oneui::Slider>(slider)) { native->setValue(value); }
}

double oneui_slider_value(OneUiWidget* slider) {
    if (auto* native = asWidget<oneui::Slider>(slider)) { return native->value(); }
    return 0.0;
}

int oneui_slider_is_dragging(OneUiWidget* slider) {
    if (auto* native = asWidget<oneui::Slider>(slider)) { return native->dragging() ? 1 : 0; }
    return 0;
}

void oneui_slider_cancel_interaction(OneUiWidget* slider) {
    if (auto* native = asWidget<oneui::Slider>(slider)) { native->cancelInteraction(); }
}

void oneui_slider_set_on_interaction(OneUiWidget* slider, OneUiSliderInteractionCallback callback, void* user_data) {
    if (auto* native = asWidget<oneui::Slider>(slider)) {
        if (!callback) { native->setOnInteraction({}); return; }
        native->setOnInteraction([callback, user_data](oneui::SliderInteraction phase, double value) {
            callback(static_cast<int>(phase), value, user_data);
        });
    }
}

int oneui_progress_bar_set_segments_utf8(OneUiWidget* bar, const OneUiProgressSegmentUtf8* segments, size_t count) {
    auto* native = asWidget<oneui::ProgressBar>(bar);
    if (!native || (!segments && count)) return 0;
    std::vector<oneui::ProgressSegment> copy;
    for (size_t i = 0; i < count; ++i) copy.push_back({segments[i].fraction, toNativeColor(segments[i].color), utf8OrEmpty(segments[i].label)});
    return native->setSegments(std::move(copy)) ? 1 : 0;
}

OneUiWidget* oneui_progress_bar_create() {
    return wrap(std::make_shared<oneui::ProgressBar>());
}

void oneui_progress_bar_set_value(OneUiWidget* progress_bar, double value) {
    if (auto* nativeProgressBar = asWidget<oneui::ProgressBar>(progress_bar)) {
        nativeProgressBar->setValue(value);
    }
}

double oneui_progress_bar_value(OneUiWidget* progress_bar) {
    if (auto* nativeProgressBar = asWidget<oneui::ProgressBar>(progress_bar)) {
        return nativeProgressBar->value();
    }
    return 0.0;
}

void oneui_progress_bar_set_smooth(OneUiWidget* progress_bar, int enabled, double duration_ms) {
    if (auto* nativeProgressBar = asWidget<oneui::ProgressBar>(progress_bar)) {
        nativeProgressBar->setSmooth(enabled != 0, duration_ms);
    }
}

void oneui_progress_bar_set_indeterminate(OneUiWidget* progress_bar, int indeterminate) {
    if (auto* nativeProgressBar = asWidget<oneui::ProgressBar>(progress_bar)) {
        nativeProgressBar->setIndeterminate(indeterminate != 0);
    }
}

int oneui_progress_bar_is_indeterminate(OneUiWidget* progress_bar) {
    if (auto* nativeProgressBar = asWidget<oneui::ProgressBar>(progress_bar)) {
        return nativeProgressBar->indeterminate() ? 1 : 0;
    }
    return 0;
}

void oneui_progress_bar_set_animations_enabled(OneUiWidget* progress_bar, int enabled) {
    if (auto* nativeProgressBar = asWidget<oneui::ProgressBar>(progress_bar)) {
        nativeProgressBar->setAnimationsEnabled(enabled != 0);
    }
}

void oneui_progress_bar_set_colors(
    OneUiWidget* progress_bar,
    uint8_t track_r,
    uint8_t track_g,
    uint8_t track_b,
    uint8_t track_a,
    uint8_t fill_r,
    uint8_t fill_g,
    uint8_t fill_b,
    uint8_t fill_a,
    float radius) {
    auto* nativeProgress = asWidget<oneui::ProgressBar>(progress_bar);
    if (!nativeProgress) return;
    oneui::ProgressBarStyleOverride style;
    style.trackBackground = oneui::Color{track_r, track_g, track_b, track_a};
    style.fill = oneui::Color{fill_r, fill_g, fill_b, fill_a};
    style.radius = std::clamp(radius, 0.0f, 100.0f);
    nativeProgress->setStyleOverride(std::move(style));
}

OneUiWidget* oneui_sparkline_create() {
    return wrap(std::make_shared<oneui::Sparkline>());
}

void oneui_sparkline_set_values(OneUiWidget* sparkline, const double* values, size_t count) {
    if (auto* nativeSparkline = asWidget<oneui::Sparkline>(sparkline)) {
        if (!values || count == 0) {
            nativeSparkline->setValues({});
            return;
        }
        nativeSparkline->setValues(std::vector<double>(values, values + count));
    }
}

OneUiWidget* oneui_time_series_chart_create() {
    return wrap(std::make_shared<oneui::TimeSeriesChart>());
}

void oneui_time_series_chart_set_series(
    OneUiWidget* chart,
    const OneUiTimeSeriesUtf8* series,
    size_t count) {
    auto* nativeChart = asWidget<oneui::TimeSeriesChart>(chart);
    if (!nativeChart) {
        return;
    }
    std::vector<oneui::TimeSeriesChartSeries> copied;
    if (series && count > 0) {
        copied.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            oneui::TimeSeriesChartSeries item;
            item.name = utf8OrEmpty(series[index].name);
            item.color = toNativeColor(series[index].color);
            if (series[index].values && series[index].value_count > 0) {
                item.values.assign(
                    series[index].values,
                    series[index].values + series[index].value_count);
            }
            copied.push_back(std::move(item));
        }
    }
    nativeChart->setSeries(std::move(copied));
}

void oneui_time_series_chart_set_range(OneUiWidget* chart, double minimum, double maximum) {
    if (auto* nativeChart = asWidget<oneui::TimeSeriesChart>(chart)) {
        nativeChart->setRange(minimum, maximum);
    }
}

void oneui_time_series_chart_set_grid_lines(OneUiWidget* chart, int count) {
    if (auto* nativeChart = asWidget<oneui::TimeSeriesChart>(chart)) {
        nativeChart->setGridLines(count);
    }
}

void oneui_time_series_chart_set_visual_style(
    OneUiWidget* chart,
    int smooth_curves,
    int area_fill,
    int dashed_grid,
    int axes_visible,
    float line_width,
    unsigned char fill_alpha) {
    if (auto* nativeChart = asWidget<oneui::TimeSeriesChart>(chart)) {
        nativeChart->setSmoothCurves(smooth_curves != 0);
        nativeChart->setAreaFill(area_fill != 0);
        nativeChart->setDashedGrid(dashed_grid != 0);
        nativeChart->setAxesVisible(axes_visible != 0);
        nativeChart->setLineWidth(line_width);
        nativeChart->setFillAlpha(fill_alpha);
    }
}

void oneui_time_series_chart_set_plot_insets(OneUiWidget* chart, OneUiInsets insets) {
    if (auto* nativeChart = asWidget<oneui::TimeSeriesChart>(chart)) {
        nativeChart->setPlotInsets(toNativeInsets(insets));
    }
}

void oneui_time_series_chart_set_grid_style(OneUiWidget* chart, int vertical_lines, float line_width) {
    if (auto* native = asWidget<oneui::TimeSeriesChart>(chart)) native->setGridStyle(vertical_lines, line_width);
}

void oneui_time_series_chart_set_axis_labels_utf8(OneUiWidget* chart, const OneUiUtf8String* labels, size_t count) {
    auto* native = asWidget<oneui::TimeSeriesChart>(chart);
    if (!native || (!labels && count)) return;
    std::vector<std::wstring> copied;
    for (size_t i = 0; i < count; ++i) copied.push_back(utf8OrEmpty(labels[i]));
    native->setAxisLabels(std::move(copied));
}

int oneui_time_series_chart_set_sample_positions(OneUiWidget* chart, const double* positions, size_t count) {
    auto* native = asWidget<oneui::TimeSeriesChart>(chart);
    if (!native || (!positions && count)) return 0;
    return native->setSamplePositions(count ? std::vector<double>(positions, positions + count) : std::vector<double>{}) ? 1 : 0;
}

void oneui_time_series_chart_set_latest_point_visible(OneUiWidget* chart, int visible) {
    if (auto* native = asWidget<oneui::TimeSeriesChart>(chart)) native->setLatestPointVisible(visible != 0);
}

void oneui_time_series_chart_set_latest_point_size(OneUiWidget* chart, float width, float height) {
    if (auto* native = asWidget<oneui::TimeSeriesChart>(chart)) native->setLatestPointSize(width, height);
}

void oneui_time_series_chart_set_thresholds(
    OneUiWidget* chart,
    const OneUiTimeSeriesThreshold* thresholds,
    size_t count) {
    auto* nativeChart = asWidget<oneui::TimeSeriesChart>(chart);
    if (!nativeChart) {
        return;
    }
    std::vector<oneui::TimeSeriesChartThreshold> copied;
    if (thresholds && count > 0) {
        copied.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            copied.push_back(oneui::TimeSeriesChartThreshold{
                thresholds[index].value,
                toNativeColor(thresholds[index].color)});
        }
    }
    nativeChart->setThresholds(std::move(copied));
}

void oneui_time_series_chart_set_inspection(OneUiWidget* chart, int index, int pinned) {
    if (auto* nativeChart = asWidget<oneui::TimeSeriesChart>(chart)) {
        nativeChart->setInspectionIndex(index, pinned != 0);
    }
}

int oneui_time_series_chart_inspection_index(const OneUiWidget* chart) {
    if (chart && chart->widget) {
        const auto* nativeChart = dynamic_cast<const oneui::TimeSeriesChart*>(chart->widget.get());
        if (!nativeChart) {
            return -1;
        }
        return nativeChart->inspectionIndex();
    }
    return -1;
}

int oneui_time_series_chart_inspection_pinned(const OneUiWidget* chart) {
    if (chart && chart->widget) {
        const auto* nativeChart = dynamic_cast<const oneui::TimeSeriesChart*>(chart->widget.get());
        if (!nativeChart) {
            return 0;
        }
        return nativeChart->inspectionPinned() ? 1 : 0;
    }
    return 0;
}

void oneui_time_series_chart_set_on_inspection_changed(
    OneUiWidget* chart,
    OneUiTimeSeriesInspectionCallback callback,
    void* user_data) {
    auto* nativeChart = asWidget<oneui::TimeSeriesChart>(chart);
    if (!nativeChart) {
        return;
    }
    if (!callback) {
        nativeChart->setOnInspectionChanged(nullptr);
        return;
    }
    nativeChart->setOnInspectionChanged([callback, user_data](int index, bool pinned) {
        callback(index, pinned ? 1 : 0, user_data);
    });
}
