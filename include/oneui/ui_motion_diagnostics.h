#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace oneui::ui {
// Opt-in, allocation-free temporal checks. Feed completed native paints, not
// timer callbacks. Expected geometry belongs to the scenario being checked.
struct MotionReport {
    std::size_t samples=0, geometryErrors=0, reverseSteps=0, lateFrames=0, stalls=0;
    double maxGeometryError=0, maxIntervalMs=0, maxPaintMs=0;
    bool continuous() const {return geometryErrors==0 && reverseSteps==0;}
};
class MotionAudit {
    double budget_,tolerance_,lastMs_=0,lastProgress_=0,direction_=1;
    bool begun_=false;
    MotionReport report_;
public:
    explicit MotionAudit(double frameBudgetMs=1000.0/60.0,double geometryTolerance=1.5)
        :budget_(frameBudgetMs),tolerance_(geometryTolerance) {
        if(!std::isfinite(budget_) || budget_<=0 || !std::isfinite(tolerance_) || tolerance_<0)
            throw std::invalid_argument("Motion audit requires a finite positive frame budget and nonnegative tolerance");
    }
    void begin(double nowMs,double progress,bool opening) {
        lastMs_=nowMs;lastProgress_=progress;direction_=opening?1:-1;begun_=true;
    }
    void observe(double nowMs,double progress,double extent,double expectedExtent,double paintMs) {
        if(!begun_)throw std::logic_error("Begin a motion segment before observing it");
        const double error=std::abs(extent-expectedExtent);
        if(!std::isfinite(error) || error>tolerance_)++report_.geometryErrors;
        report_.maxGeometryError=(std::max)(report_.maxGeometryError,error);
        if(direction_*(progress-lastProgress_)<-.0001)++report_.reverseSteps;
        const double interval=nowMs-lastMs_;
        report_.maxIntervalMs=(std::max)(report_.maxIntervalMs,interval);
        report_.maxPaintMs=(std::max)(report_.maxPaintMs,paintMs);
        if(interval>budget_*1.5)++report_.lateFrames;
        if(interval>budget_*3)++report_.stalls;
        ++report_.samples;lastMs_=nowMs;lastProgress_=progress;
    }
    const MotionReport& report()const{return report_;}
};
}
