#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "internal/scroll_trace.h"

#include <array>
#include <cstddef>
#include <memory>
#include <new>

namespace oneui::internal {

// Keep the layout samples local to the painting thread. Trace I/O takes place
// after presentation so it cannot extend the paragraph spans being measured.
class Win32ParagraphTrace {
    struct Sample {
        LONGLONG firstTick, lastTick;
        ULONG64 cycles;
        std::size_t utf16Units;
        float fontSize, layoutWidth;
        bool realignment, hasCycles;
    };
    struct Buffer {
        std::array<Sample, 512> samples;
        std::size_t count = 0;
        std::size_t dropped = 0;
    };
    inline static thread_local Buffer* active_ = nullptr;
    inline static thread_local bool storageInUse_ = false;

    static bool enabled() noexcept {
        static const bool requested = [] {
            wchar_t value[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_PARAGRAPH_TRACE", value, 2) == 1 &&
                value[0] == L'1';
        }();
        return requested;
    }

public:
    explicit Win32ParagraphTrace(bool tracing) noexcept : previous_(active_) {
        active_ = nullptr;
        if (!tracing || !enabled() || storageInUse_) return;
        static thread_local std::unique_ptr<Buffer> buffer;
        if (!buffer) buffer.reset(new (std::nothrow) Buffer);
        if (!buffer) return;
        buffer->count = buffer->dropped = 0;
        buffer_ = active_ = buffer.get();
        storageInUse_ = true;
    }

    ~Win32ParagraphTrace() {
        if (buffer_) storageInUse_ = false;
        active_ = previous_;
    }
    Win32ParagraphTrace(const Win32ParagraphTrace&) = delete;
    Win32ParagraphTrace& operator=(const Win32ParagraphTrace&) = delete;

    class Span {
    public:
        Span(bool sensitive, std::size_t utf16Units, float fontSize,
             float layoutWidth, bool realignment) noexcept {
            auto* buffer = sensitive ? nullptr : active_;
            if (!buffer) return;
            if (buffer->count == buffer->samples.size()) {
                ++buffer->dropped;
                return;
            }
            sample_ = &buffer->samples[buffer->count++];
            sample_->utf16Units = utf16Units;
            sample_->fontSize = fontSize;
            sample_->layoutWidth = layoutWidth;
            sample_->realignment = realignment;
            sample_->hasCycles = QueryThreadCycleTime(GetCurrentThread(), &firstCycles_) != FALSE;
            LARGE_INTEGER clock{};
            QueryPerformanceCounter(&clock);
            sample_->firstTick = clock.QuadPart;
        }

        ~Span() {
            if (!sample_) return;
            LARGE_INTEGER clock{};
            QueryPerformanceCounter(&clock);
            sample_->lastTick = clock.QuadPart;
            ULONG64 lastCycles = 0;
            sample_->hasCycles = sample_->hasCycles &&
                QueryThreadCycleTime(GetCurrentThread(), &lastCycles) != FALSE &&
                lastCycles >= firstCycles_;
            sample_->cycles = sample_->hasCycles ? lastCycles - firstCycles_ : 0;
        }
        Span(const Span&) = delete;
        Span& operator=(const Span&) = delete;

    private:
        Sample* sample_ = nullptr;
        ULONG64 firstCycles_ = 0;
    };

    void write(std::uintptr_t window, double sequence) const {
        if (!buffer_ || (!buffer_->count && !buffer_->dropped)) return;
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        for (std::size_t i = 0; i < buffer_->count; ++i) {
            const auto& s = buffer_->samples[i];
            writeScrollTrace({
                "win32_paragraph", s.realignment ? "realign" : "layout", window,
                static_cast<double>(s.utf16Units), s.fontSize, s.layoutWidth,
                static_cast<double>(s.firstTick), static_cast<double>(s.lastTick),
                static_cast<double>(frequency.QuadPart),
                static_cast<double>(s.lastTick - s.firstTick) * 1000.0 / frequency.QuadPart,
                sequence, s.hasCycles ? static_cast<double>(s.cycles) : -1.0});
        }
        if (buffer_->dropped) {
            writeScrollTrace({"win32_paragraph", "dropped", window, 0, 0, 0, 0, 0, 0, 0,
                sequence, static_cast<double>(buffer_->dropped)});
        }
    }

private:
    Buffer* previous_;
    Buffer* buffer_ = nullptr;
};

} // namespace oneui::internal
