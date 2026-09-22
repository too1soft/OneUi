#include "oneui/controls/time_series_chart.h"

#include <cmath>
#include "support/recording_canvas.h"
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

    chart.setThresholds({});
    chart.setGridLines(3);
    chart.setGridStyle(0, 0.7f);
    chart.setAxesVisible(false);
    chart.setDashedGrid(false);
    chart.setPlotInsets({10, 10, 10, 40});
    chart.setAxisLabels({L"100%", L"50%", L"0%"});
    chart.setSeries({});
    oneui::StyleBox style;
    style.borderColor = oneui::Color{70, 80, 90, 60};
    style.placeholderColor = oneui::Color{150, 160, 170, 255};
    style.fontSize = 11.0f;
    chart.setStyleBox(style);
    oneui::test_support::RecordingCanvas grid;
    chart.paint(grid);
    expectEqual("horizontal-only chart has exactly three grid strokes", static_cast<int>(grid.lines.size()), 3);
    expectEqual("axis labels are rendered", static_cast<int>(grid.texts.size()), 3);
    expectTrue("top label content", grid.texts.front().text == L"100%");
    expectTrue("label uses stylesheet", grid.texts.front().color.r == 150);
    for (const auto& line : grid.lines) {
        expectTrue("horizontal grid", line.from.y == line.to.y);
        expectTrue("grid uses supplied thickness", std::abs(line.width - 0.7f) < 0.001f);
        expectTrue("grid uses stylesheet", line.color.a == 60);
    }
    chart.setSeries({{L"CPU", oneui::Color{49, 139, 234, 255}, {20, 40, 60}}});
    expectTrue("ordered positions accepted", chart.setSamplePositions({0.7, 0.75, 1.0}));
    oneui::MouseEvent mouse{};
    mouse.position = {192, 40}; // 80% of the actual plot; nearest sample is index 1.
    chart.onMouseMove(mouse);
    expectEqual("pointer inspection follows timestamps", chart.inspectionIndex(), 1);
    expectTrue("unordered positions rejected", !chart.setSamplePositions({0.8, 0.2, 1.0}));
    expectTrue("NaN positions rejected", !chart.setSamplePositions({std::numeric_limits<double>::quiet_NaN()}));
    chart.onMouseMove(mouse);
    expectEqual("bad positions preserve valid mapping", chart.inspectionIndex(), 1);
    chart.setInspectionIndex(-1);
    chart.setLatestPointVisible(true);
    oneui::test_support::RecordingCanvas latest;
    chart.paint(latest);
    expectEqual("latest point paints without hover", static_cast<int>(latest.fillEllipses.size()), 1);
    expectTrue("latest dot reaches domain end", std::abs(latest.fillEllipses.front().rect.x - 227.5f) < 0.001f);
    chart.setSamplePositions({});
    chart.onMouseMove(mouse);
    expectEqual("empty domain restores equal spacing", chart.inspectionIndex(), 2);
    return failures == 0 ? 0 : 1;
}
