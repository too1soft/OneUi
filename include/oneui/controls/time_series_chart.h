#pragma once

#include "oneui/export.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <string>
#include <functional>
#include <vector>

namespace oneui {

struct TimeSeriesChartSeries {
    std::wstring name;
    Color color{37, 99, 235, 255};
    std::vector<double> values;
};

struct TimeSeriesChartThreshold {
    double value = 0.0;
    Color color{245, 158, 11, 180};
};

/// Lightweight multi-series chart for operational dashboards.
///
/// The caller supplies samples in display units and a stable value range.
/// The widget owns a copy of all series and paints only lines/grid geometry.
class ONEUI_API TimeSeriesChart final : public Widget {
public:
    TimeSeriesChart();

    void setSeries(std::vector<TimeSeriesChartSeries> series);
    const std::vector<TimeSeriesChartSeries>& series() const;
    void setRange(double minimum, double maximum);
    double minimum() const;
    double maximum() const;
    void setGridLines(int count);
    int gridLines() const;
    void setSmoothCurves(bool enabled);
    void setAreaFill(bool enabled);
    void setDashedGrid(bool enabled);
    void setAxesVisible(bool visible);
    void setLineWidth(float width);
    void setFillAlpha(unsigned char alpha);
    void setPlotInsets(Insets insets);
    void setThresholds(std::vector<TimeSeriesChartThreshold> thresholds);
    const std::vector<TimeSeriesChartThreshold>& thresholds() const;
    void setInspectionIndex(int index, bool pinned = false);
    int inspectionIndex() const;
    bool inspectionPinned() const;
    void setOnInspectionChanged(std::function<void(int, bool)> callback);

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    CursorKind cursor(Point point) const override;

protected:
    bool hasInteractionState() const override;
    void resetInteractionState() override;

private:
    Rect plotRect() const;
    std::size_t maximumSampleCount() const;
    int inspectionIndexAt(Point point) const;
    void updateInspection(int index, bool pinned, bool notify);
    void refreshAccessibleValue();
    Rect inspectionDamageRect(int index) const;

    std::vector<TimeSeriesChartSeries> series_;
    std::vector<TimeSeriesChartThreshold> thresholds_;
    double minimum_ = 0.0;
    double maximum_ = 1.0;
    int gridLines_ = 4;
    bool smoothCurves_ = true;
    bool areaFill_ = true;
    bool dashedGrid_ = true;
    bool axesVisible_ = true;
    float lineWidth_ = 2.25f;
    unsigned char fillAlpha_ = 32;
    Insets plotInsets_{4.0f, 2.0f, 2.0f, 2.0f};
    int inspectionIndex_ = -1;
    bool inspectionPinned_ = false;
    bool pressed_ = false;
    std::function<void(int, bool)> onInspectionChanged_;
};

} // namespace oneui
