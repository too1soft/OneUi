#pragma once
#include "oneui/canvas.h"
#include <cmath>
#include <vector>

namespace oneui::chart_curve {
// Weighted harmonic tangents preserve monotonicity for uneven sample spacing.
// A flat run or an extremum has a zero tangent; endpoints use their one-sided secant.
inline std::vector<float> tangents(const std::vector<Point>& points) {
    const auto count = points.size();
    if (count < 2) return std::vector<float>(count, 0.0f);
    std::vector<float> dx(count-1), secant(count-1), slope(count);
    for (std::size_t i=0; i+1<count; ++i) {
        dx[i]=points[i+1].x-points[i].x;
        secant[i]=dx[i]>0.0001f ? (points[i+1].y-points[i].y)/dx[i] : 0.0f;
    }
    slope.front()=secant.front(); slope.back()=secant.back();
    for (std::size_t i=1; i+1<count; ++i) {
        if (secant[i-1]*secant[i]<=0.0f) slope[i]=0.0f;
        else slope[i]=3.0f*(dx[i-1]+dx[i]) /
            ((2.0f*dx[i]+dx[i-1])/secant[i-1]+(dx[i]+2.0f*dx[i-1])/secant[i]);
    }
    return slope;
}
inline void appendMonotone(CanvasPath& path, const std::vector<Point>& points) {
    const auto slope=tangents(points);
    for (std::size_t i=0; i+1<points.size(); ++i) {
        const float third=(points[i+1].x-points[i].x)/3.0f;
        path.cubicTo({points[i].x+third,points[i].y+slope[i]*third},
            {points[i+1].x-third,points[i+1].y-slope[i+1]*third},points[i+1]);
    }
}
}
