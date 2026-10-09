#include <memory>
#include <oneui/controls/time_series_chart.h>

std::shared_ptr<oneui::TimeSeriesChart> example_chart() {
    auto chart = std::make_shared<oneui::TimeSeriesChart>();
    chart->setRange(0, 100);
    chart->setSeries({{L"本地模拟", {40, 97, 61}, {25, 50, 40, 75, 60}}});
    chart->setSamplePositions({0, .25, .5, .75, 1});
    chart->setLatestPointVisible(true);
    // Supply new samples from the UI dispatcher, and stop sampling when hidden.
    return chart;
}
