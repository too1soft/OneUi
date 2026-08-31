#include "oneui/oneui_c_api.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {

int failures = 0;

OneUiUtf8String utf8View(const std::string& value) {
    return OneUiUtf8String{value.data(), value.size()};
}

void expectEqual(const char* name, int actual, int expected) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    OneUiWidget* chart = oneui_time_series_chart_create();
    if (!chart) {
        std::cerr << "time-series C ABI failed to create the chart\n";
        return 1;
    }

    const std::string cpuName = "CPU";
    const double cpu[] = {22.0, std::numeric_limits<double>::quiet_NaN(), 48.0};
    const OneUiTimeSeriesUtf8 series[] = {
        {utf8View(cpuName), OneUiColor{77, 163, 255, 255}, cpu, 3},
    };
    const OneUiTimeSeriesThreshold thresholds[] = {
        {80.0, OneUiColor{245, 158, 11, 128}},
        {95.0, OneUiColor{239, 68, 68, 132}},
    };
    oneui_time_series_chart_set_series(chart, series, 1);
    oneui_time_series_chart_set_range(chart, 0.0, 100.0);
    oneui_time_series_chart_set_grid_lines(chart, 4);
    oneui_time_series_chart_set_visual_style(chart, 0, 1, 1, 0, 1.0f, 20);
    oneui_time_series_chart_set_plot_insets(chart, OneUiInsets{4.0f, 4.0f, 4.0f, 4.0f});
    oneui_time_series_chart_set_thresholds(chart, thresholds, 2);
    oneui_time_series_chart_set_inspection(chart, 2, 1);
    expectEqual("time-series C ABI inspection index", oneui_time_series_chart_inspection_index(chart), 2);
    expectEqual("time-series C ABI inspection pin", oneui_time_series_chart_inspection_pinned(chart), 1);
    oneui_time_series_chart_set_on_inspection_changed(chart, nullptr, nullptr);
    oneui_widget_destroy(chart);

    return failures == 0 ? 0 : 1;
}
