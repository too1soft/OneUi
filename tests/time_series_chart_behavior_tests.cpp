#include "oneui/controls/time_series_chart.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void expectEqual(const char* name, int actual, int expected) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectTrue(const char* name, bool value) {
    expectEqual(name, value ? 1 : 0, 1);
}

} // namespace

int main() {
    oneui::TimeSeriesChart chart;
    chart.setFrame(oneui::Rect{0.0f, 0.0f, 240.0f, 120.0f});
    chart.setRange(0.0, 100.0);
    chart.setSmoothCurves(false);
    chart.setAreaFill(false);
    chart.setThresholds({
        oneui::TimeSeriesChartThreshold{80.0, oneui::Color{245, 158, 11, 128}},
        oneui::TimeSeriesChartThreshold{95.0, oneui::Color{239, 68, 68, 132}},
    });
    chart.setSeries({
        oneui::TimeSeriesChartSeries{
            L"CPU",
            oneui::Color{77, 163, 255},
            {20.0, 30.0, std::numeric_limits<double>::quiet_NaN(), 65.0, 72.0}},
    });

    expectEqual("time-series preserves series", static_cast<int>(chart.series().size()), 1);
    expectEqual("time-series preserves thresholds", static_cast<int>(chart.thresholds().size()), 2);
    expectTrue("time-series preserves gaps", !std::isfinite(chart.series().front().values[2]));

    int inspected = -1;
    bool pinned = false;
    chart.setOnInspectionChanged([&](int index, bool value) {
        inspected = index;
        pinned = value;
    });
    chart.onFocusChanged(true);
    chart.onKeyDown(oneui::KeyEvent{oneui::Key::End});
    expectEqual("End inspects latest sample", chart.inspectionIndex(), 4);
    expectEqual("inspection callback receives latest sample", inspected, 4);
    expectTrue("keyboard inspection pins sample", pinned);
    chart.onKeyDown(oneui::KeyEvent{oneui::Key::Left});
    expectEqual("Left browses previous sample", chart.inspectionIndex(), 3);
    chart.onKeyDown(oneui::KeyEvent{oneui::Key::Escape});
    expectEqual("Escape clears inspection", chart.inspectionIndex(), -1);

    return failures == 0 ? 0 : 1;
}
