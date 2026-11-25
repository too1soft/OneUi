#pragma once

#include "gl_flush_trace_win32.h"

namespace oneui::win32 {

class GlProgramReserve {
    class Interface final : public GrGLInterface {
    public:
        Interface(sk_sp<const GrGLInterface> native, HGLRC context)
            : native_(std::move(native)), context_(context) {
            fStandard = native_->fStandard;
            fExtensions = native_->fExtensions;
            fFunctions = native_->fFunctions;
            fFunctions.fCreateProgram = [this] {
                if (spare_ && wglGetCurrentContext() == context_) {
                    const internal::GlCallTrace::Span timer(internal::GlPhase::ProgramReserved);
                    return std::exchange(spare_, 0);
                }
                return native_->fFunctions.fCreateProgram();
            };
        }

        bool hasSpare() const { return spare_ != 0; }

        bool fill() {
            spare_ = native_->fFunctions.fCreateProgram();
            return hasSpare();
        }

        void discardWhenCurrent() {
            if (spare_ && wglGetCurrentContext() == context_) {
                native_->fFunctions.fDeleteProgram(std::exchange(spare_, 0));
            }
        }

    private:
        sk_sp<const GrGLInterface> native_;
        HGLRC context_;
        GrGLuint spare_ = 0;
    };

public:
    static constexpr UINT_PTR timer = 0x4f14;

    sk_sp<const GrGLInterface> attach(HGLRC context, bool nvidia) {
        if (!nvidia || !enabled()) return nullptr;
        auto native = GrGLMakeNativeInterface();
        if (!native || !native->fFunctions.fCreateProgram || !native->fFunctions.fDeleteProgram)
            return nullptr;
        if (internal::win32GlTraceEnabled())
            native = sk_make_sp<internal::TracedWin32GlInterface>(std::move(native));
        interface_ = sk_make_sp<Interface>(std::move(native), context);
        return interface_;
    }

    void arm(HWND window) {
        if (interface_ && !interface_->hasSpare() && !scheduled_) {
            scheduled_ = SetTimer(window, timer, 250, nullptr) != 0;
        }
    }

    void onTimer(HWND window, HGLRC context, HDC dc, bool resizing) {
        if (!scheduled_) return;
        if (!interface_ || !context || !dc || !IsWindowVisible(window) || IsIconic(window)) {
            cancel(window);
            return;
        }
        if (resizing || HIWORD(GetQueueStatus(QS_INPUT))) return;
        cancel(window);
        if (interface_->hasSpare()) return;

        // Painting leaves its context current. If another window painted
        // since this timer was armed, let the next swap rearm the reserve
        // instead of flushing that window's context from a timer callback.
        if (wglGetCurrentContext() != context || wglGetCurrentDC() != dc) return;
        const bool tracing = internal::scrollTraceEnabled();
        const double start = tracing ? internal::scrollTraceNowMs() : 0.0;
        const bool filled = interface_->fill();
        if (tracing) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "gl_program_reserve_fill", reinterpret_cast<std::uintptr_t>(window),
                0, 0, 0, 0, 0, 0, internal::scrollTraceNowMs() - start, filled ? 1.0 : 0.0, 0.0});
        }
    }

    void reset(HWND window) {
        cancel(window);
        if (interface_) interface_->discardWhenCurrent();
        interface_.reset();
    }

private:
    void cancel(HWND window) {
        if (scheduled_) KillTimer(window, timer);
        scheduled_ = false;
    }

    static bool enabled() {
        static const bool selected = [] {
            wchar_t value[2]{};
            return GetEnvironmentVariableW(L"ONEUI_TEST_RESERVE_GL_PROGRAM", value, 2) == 1 && value[0] == L'1';
        }();
        return selected;
    }

    sk_sp<Interface> interface_;
    bool scheduled_ = false;
};

} // namespace oneui::win32
