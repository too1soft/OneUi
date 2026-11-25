#pragma once

#include "internal/frame_profile.h"
#include "internal/scroll_trace.h"
#include "platform/shared/skia_canvas.h"

#include <windows.h>
#include <array>
#include <cstddef>
#include <utility>

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

    void write(std::uintptr_t window, double paintSequence, const RECT& damage,
               const FrameProfile& profile, const rendering::PrimitivePaintTrace& primitives) const {
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

        ScrollTraceEvent event;
        event.component = "win32";
        event.object = window;
        event.detailA = paintSequence;
        constexpr std::pair<FrameStage, const char*> stages[] = {
            {FrameStage::Fill, "paint_fill"},
            {FrameStage::Path, "paint_path"},
            {FrameStage::TextCache, "text_cache"},
            {FrameStage::TextIndex, "text_index"},
            {FrameStage::TextBuild, "text_build"},
            {FrameStage::TextBidi, "text_bidi"},
            {FrameStage::TextShape, "text_shape"},
            {FrameStage::TextBlob, "text_blob"},
        };
        for (const auto& [stage, name] : stages) {
            event.phase = name;
            event.durationMs = profile.ms[static_cast<std::size_t>(stage)];
            if (event.durationMs > 0.0) writeScrollTrace(event);
        }
        const auto writePrimitive = [&](const char* name, std::uint64_t calls, double ms) {
            if (!calls) return;
            event.phase = name;
            event.durationMs = ms;
            event.detailB = static_cast<double>(calls);
            writeScrollTrace(event);
        };
        writePrimitive("paint_text_draw", primitives.textCalls, primitives.textMs);
        writePrimitive("paint_text_measure", primitives.textMeasureCalls, primitives.textMeasureMs);
        writePrimitive("paint_shadow", primitives.shadowCalls, primitives.shadowMs);
        writePrimitive("paint_gradient", primitives.gradientCalls, primitives.gradientMs);
        event.phase = "paint_damage";
        event.detailB = 0.0;
        event.inputDelta = damage.left;
        event.offset = damage.top;
        event.target = damage.right - damage.left;
        event.velocity = damage.bottom - damage.top;
        event.durationMs = 0.0;
        writeScrollTrace(event);
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
