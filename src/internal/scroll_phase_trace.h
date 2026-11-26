#pragma once

#include "internal/scroll_trace.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdlib>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace oneui::internal {

inline bool tablePhaseTraceEnabled() {
    static const bool enabled = [] {
        const auto* value = std::getenv("ONEUI_VIEW_TABLE_PHASE_TIMING");
        return value && value[0] == '1' && value[1] == '\0';
    }();
    return enabled && scrollTraceEnabled();
}

// Record boundaries before emitting any events, so the measured work excludes
// the trace writes. The same wall clock is used by the existing scroll trace.
template <std::size_t Phases>
class ScrollPhaseTrace {
public:
    explicit ScrollPhaseTrace(bool enabled) : enabled_(enabled) { mark(); }

    void mark() {
        if (!enabled_) return;
        assert(count_ < samples_.size());
        auto& sample = samples_[count_++];
        sample.time = scrollTraceNowMs();
#ifdef _WIN32
        sample.hasCycles = QueryThreadCycleTime(GetCurrentThread(), &sample.cycles) != FALSE;
#endif
    }

    void write(const char* component, std::uintptr_t object, double index,
               const std::array<const char*, Phases>& names) const {
        if (!enabled_) return;
        for (std::size_t i = 1; i < count_; ++i) {
            const auto& before = samples_[i - 1];
            const auto& after = samples_[i];
            const double cycles = before.hasCycles && after.hasCycles
                ? static_cast<double>(after.cycles - before.cycles) : -1.0;
            ScrollTraceEvent event;
            event.component = component;
            event.phase = names[i - 1];
            event.object = object;
            event.offset = before.time;
            event.target = after.time;
            event.durationMs = after.time - before.time;
            event.detailA = cycles;
            event.detailB = index;
            writeScrollTrace(event);
        }
    }

private:
    struct Sample {
        double time = 0.0;
        unsigned long long cycles = 0;
        bool hasCycles = false;
    };

    bool enabled_;
    std::size_t count_ = 0;
    std::array<Sample, Phases + 1> samples_{};
};

} // namespace oneui::internal
