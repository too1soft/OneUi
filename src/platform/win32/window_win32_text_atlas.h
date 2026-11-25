#pragma once

#include "text/atlas_warmup_capture.h"
#include "internal/scroll_trace.h"
#include "gl_flush_trace_win32.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"

#include <windows.h>
#include <algorithm>
#include <cmath>

namespace oneui::win32 {

class TextAtlasWarmup {
public:
    static constexpr UINT_PTR timer = 0x4f13;

    text::AtlasWarmupCapture::Runs* capture(bool eligible) {
        if (eligible && enabled() && !started_) return &runs_;
        return nullptr;
    }

    void arm(HWND window) {
        if (!scheduled_ && !started_ && !runs_.empty()) {
            scheduled_ = SetTimer(window, timer, 250, nullptr) != 0;
            scheduledAt_ = GetTickCount();
            if (!scheduled_) runs_.clear();
        }
    }

    void onTimer(HWND window, HGLRC gl, HDC dc, GrDirectContext* context, SkSurface* source, bool resizing) {
        if (started_ || !context || !source || !gl || !dc) {
            cancel(window);
            return;
        }
        if (static_cast<DWORD>(GetTickCount() - scheduledAt_) >= 5000) {
            started_ = true;
            cancel(window);
            return;
        }
        LASTINPUTINFO input{sizeof(input)};
        if (resizing || GetForegroundWindow() != window || !GetLastInputInfo(&input) ||
            static_cast<DWORD>(GetTickCount() - input.dwTime) < 250) return;

        KillTimer(window, timer);
        scheduled_ = false;
        started_ = true;
        if (wglGetCurrentContext() != gl || wglGetCurrentDC() != dc) {
            if (!wglMakeCurrent(dc, gl)) {
                runs_.clear();
                return;
            }
        }
        const bool trace = internal::scrollTraceEnabled();
        const double begin = trace ? internal::scrollTraceNowMs() : 0.0;
        // A compatible offscreen surface shares the window's Ganesh context
        // and glyph atlas, without drawing into the visible back buffer.
        auto surface = source->makeSurface(1536, 128);
        int runs = 0;
        if (surface) {
            auto& canvas = *surface->getCanvas();
            canvas.clear(SK_ColorTRANSPARENT);
            for (const auto& run : runs_) {
                const auto bounds = run.blob->bounds();
                const float x = 8.0f - std::floor(bounds.left() * run.scaleX);
                const float y = 8.0f - std::floor(bounds.top() * run.scaleY) + run.verticalPhase;
                canvas.setMatrix(SkMatrix::Scale(run.scaleX, run.scaleY));
                // Ganesh caches four horizontal quarter-pixel phases. Keep
                // the original paint, whose luminance participates in the key.
                for (int quarter = 0; quarter < 4; ++quarter) {
                    canvas.drawTextBlob(run.blob, (x + quarter * 0.25f) / run.scaleX,
                        y / run.scaleY, run.paint);
                }
                ++runs;
            }
            if (trace && internal::win32GlTraceEnabled()) {
                internal::GlCallTrace calls;
                {
                    internal::GlCallTrace::Scope record(&calls);
                    context->flushAndSubmit(surface.get(), GrSyncCpu::kNo);
                }
                calls.write(reinterpret_cast<std::uintptr_t>(window), -1.0);
            } else {
                context->flushAndSubmit(surface.get(), GrSyncCpu::kNo);
            }
        }
        runs_.clear();
        if (trace) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{"win32", "paint_text_atlas_warmup",
                reinterpret_cast<std::uintptr_t>(window), 0, 0, 0, 0, 0, 0,
                internal::scrollTraceNowMs() - begin, static_cast<double>(runs), surface ? 1.0 : 0.0});
        }
    }

    void cancel(HWND window) {
        if (scheduled_) KillTimer(window, timer);
        scheduled_ = false;
        runs_.clear();
    }

private:
    static bool enabled() {
        static const bool choice = [] {
            wchar_t value[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_TEXT_PHASE_WARMUP", value, 2) != 1 ||
                value[0] != L'0';
        }();
        return choice;
    }

    text::AtlasWarmupCapture::Runs runs_;
    bool started_ = false;
    bool scheduled_ = false;
    DWORD scheduledAt_ = 0;
};

} // namespace oneui::win32
