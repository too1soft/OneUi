#pragma once
#include "oneui/export.h"
#include <cstdint>
#include <string>
#include <vector>

namespace oneui {
// Developer metadata never contains widget text, input values or IME content.
struct DiagnosticSource {
    std::string component, file;
    int line = 0;
};
struct DeveloperOptions {
    bool enabled = true;
    bool overlay = true;
    double frameBudgetMs = 1000.0 / 60.0;
};
enum DiagnosticFlag : unsigned {
    SlowPaint = 1, AnimationDelay = 2, MotionReversal = 4
};
struct DeveloperFrame {
    double paintMs=0, intervalMs=0, contentMs=0, submitMs=0, blitMs=0;
    double layoutMs=0, textLayoutMs=0, fillMs=0, pathMs=0;
    double hottestSelfMs=0;
    DiagnosticSource hottest;
};
struct DeveloperIssue {
    std::uint64_t frame=0;
    unsigned flags=0;
    DeveloperFrame timing;
    DiagnosticSource source;
};
struct DeveloperSnapshot {
    bool supported=false, enabled=false, overlay=false;
    double frameBudgetMs=0;
    std::string backend="unknown";
    std::uint64_t frames=0, slowPaints=0, animationDelays=0, motionReversals=0;
    std::uint64_t discardedIssues=0, motionEvictions=0, motionSamples=0;
    DeveloperFrame last;
    std::vector<DeveloperIssue> issues; // Oldest first, at most 128 entries.
};
// UTF-8, machine-readable JSON. CPU wall timings, not GPU/display timestamps.
ONEUI_API std::string developerReportJson(const DeveloperSnapshot& snapshot);
}
