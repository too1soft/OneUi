#pragma once

#include <cstdint>

namespace oneui::internal {

struct ScrollTraceEvent {
    const char* component = "";
    const char* phase = "";
    std::uintptr_t object = 0;
    double inputDelta = 0.0;
    double offset = 0.0;
    double target = 0.0;
    double velocity = 0.0;
    double intervalMs = 0.0;
    double maxOffset = 0.0;
    double durationMs = 0.0;
    double detailA = 0.0;
    double detailB = 0.0;
};

bool scrollTraceEnabled();
double scrollTraceNowMs();
void writeScrollTrace(const ScrollTraceEvent& event);

} // namespace oneui::internal
