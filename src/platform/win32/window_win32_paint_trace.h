#pragma once

#include "internal/scroll_trace.h"

#include <windows.h>
#include <array>
#include <cstddef>

namespace oneui::internal {

// Capture checkpoints during paint and write them after presentation. The
// cycle delta records work on this thread independently of wall-clock waits.
class Win32PaintTrace {
public:
    enum Boundary : std::size_t { Begin, GlSetup, NativePaint, Surface, Canvas, Tree, End, Count };

    explicit Win32PaintTrace(bool enabled) : enabled_(enabled) { mark(Begin); }

    void mark(Boundary boundary) {
        if (!enabled_) return;
        auto& sample = checkpoints_[boundary];
        sample.timeMs = scrollTraceNowMs();
        sample.cycles = 0;
        sample.hasCycles = QueryThreadCycleTime(GetCurrentThread(), &sample.cycles) != FALSE;
    }

    void write(std::uintptr_t window, double paintSequence) const {
        if (!enabled_) return;
        constexpr const char* phases[]{ "paint_backend_setup", "paint_begin_native", "paint_surface",
                                       "paint_canvas_setup", "paint_widget_tree", "paint_record_finish" };
        for (std::size_t i = 1; i < Count; ++i) {
            const auto& before = checkpoints_[i - 1];
            const auto& after = checkpoints_[i];
            ScrollTraceEvent event;
            event.component = "win32";
            event.phase = phases[i - 1];
            event.object = window;
            event.durationMs = after.timeMs - before.timeMs;
            event.detailA = paintSequence;
            event.detailB = after.hasCycles && before.hasCycles
                ? static_cast<double>(after.cycles - before.cycles) : -1.0;
            writeScrollTrace(event);
        }
    }

private:
    struct Checkpoint {
        double timeMs;
        ULONG64 cycles;
        bool hasCycles;
    };
    bool enabled_;
    std::array<Checkpoint, Count> checkpoints_;
};

} // namespace oneui::internal
