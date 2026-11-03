#pragma once
#include "oneui/controls/time_series_chart.h"
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace oneui::ui::chart_property {
// Text-valued chart properties share one parser for compiled IR and live bindings.
inline bool applyText(TimeSeriesChart& chart, const std::string& key, const std::wstring& value) {
    if (key == "latest-style") {
        std::wistringstream input(value); int series; float width,height,outline; wchar_t extra;
        if (!(input>>series>>width>>height>>outline) || (input>>extra) || series< -1
            || !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(outline)
            || width<=0 || height<=0 || width>128 || height>128 || outline<0 || outline>32)
            throw std::invalid_argument("Chart latest-style requires series/-1, width, height and outline");
        chart.setLatestPointStyle(series,width,height,outline);return true;
    }
    if (key == "secondary-style") {
        std::wistringstream input(value); float width,opacity; wchar_t extra;
        if (!(input>>width>>opacity) || (input>>extra) || !std::isfinite(width) || !std::isfinite(opacity)
            || width<=0 || width>32 || opacity<0 || opacity>1)
            throw std::invalid_argument("Chart secondary-style requires width and opacity in 0..1");
        chart.setSecondarySeriesStyle(width,opacity);return true;
    }
    if (key == "thresholds") {
        std::wistringstream rows(value);
        std::wstring row;
        std::vector<TimeSeriesChartThreshold> thresholds;
        while (std::getline(rows, row, L';')) {
            if (row.find_first_not_of(L" \t\r\n") == row.npos) continue;
            std::wistringstream input(row);
            double point; int r,g,b,a; wchar_t extra;
            if (!(input>>point>>r>>g>>b>>a) || (input>>extra) || !std::isfinite(point)
                || r<0 || r>255 || g<0 || g>255 || b<0 || b>255 || a<0 || a>255 || thresholds.size()>=8)
                throw std::invalid_argument("Chart thresholds require up to eight finite value/R/G/B/A rows");
            thresholds.push_back({point, Color{static_cast<unsigned char>(r),static_cast<unsigned char>(g),static_cast<unsigned char>(b),static_cast<unsigned char>(a)}});
        }
        chart.setThresholds(std::move(thresholds));
        return true;
    }
    if (key == "threshold-style") {
        std::wistringstream input(value); float dash,gap; wchar_t extra;
        if (!(input>>dash>>gap) || (input>>extra) || !std::isfinite(dash) || !std::isfinite(gap)
            || dash<0.5f || gap<0.5f || dash>128 || gap>128)
            throw std::invalid_argument("Chart threshold-style requires dash/gap in 0.5..128px");
        chart.setThresholdDashStyle(dash,gap);return true;
    }
    if (key == "grid-style") {
        std::wistringstream input(value); int vertical; float width; wchar_t extra;
        if (!(input>>vertical>>width) || (input>>extra) || vertical<0 || vertical>16 || !std::isfinite(width) || width<=0)
            throw std::invalid_argument("Chart grid-style requires count and positive line width");
        chart.setGridStyle(vertical,width);return true;
    }
    if (key == "fill-extent") {
        if(value==L"plot") chart.setFillExtent(TimeSeriesFillExtent::Plot);
        else if(value==L"series") chart.setFillExtent(TimeSeriesFillExtent::Series);
        else if(value==L"solid") chart.setFillExtent(TimeSeriesFillExtent::Solid);
        else throw std::invalid_argument("Chart fill-extent must be plot, series or solid");
        return true;
    }
    if (key == "plot-insets") {
        std::wistringstream input(value); Insets insets{}; wchar_t extra;
        if (!(input>>insets.top>>insets.right>>insets.bottom>>insets.left) || (input>>extra)
            || !std::isfinite(insets.top) || !std::isfinite(insets.right) || !std::isfinite(insets.bottom) || !std::isfinite(insets.left)
            || insets.top<0 || insets.right<0 || insets.bottom<0 || insets.left<0)
            throw std::invalid_argument("Chart plot-insets requires four nonnegative numbers");
        chart.setPlotInsets(insets);return true;
    }
    return false;
}
}
