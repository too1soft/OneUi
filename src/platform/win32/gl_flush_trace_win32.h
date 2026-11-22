#pragma once

#include "internal/scroll_trace.h"

#include <windows.h>
#include "include/gpu/ganesh/gl/GrGLInterface.h"

#include <array>
#include <chrono>
#include <utility>

namespace oneui::internal {

inline bool win32GlTraceEnabled() {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_GL_TRACE", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

enum class GlPhase : std::size_t {
    Compile, Link, CompileStatus, LinkStatus, ProgramQuery, BinaryRead, BinaryWrite, ProgramUse,
    TextureAllocate, TextureUpload, BufferUpload, Draw, Sync, Count
};

class GlFlushTrace {
    struct Sample {
        double milliseconds = 0.0;
        unsigned calls = 0;
    };

    static inline thread_local GlFlushTrace* active_ = nullptr;
    std::array<Sample, static_cast<std::size_t>(GlPhase::Count)> stages_{};

public:
    class Scope {
        GlFlushTrace* previous_ = active_;

    public:
        explicit Scope(GlFlushTrace* trace) { active_ = trace; }
        ~Scope() { active_ = previous_; }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };

    class Span {
        Sample* sample_;
        std::chrono::steady_clock::time_point start_;

    public:
        explicit Span(GlPhase phase)
            : sample_(active_ ? &active_->stages_[static_cast<std::size_t>(phase)] : nullptr) {
            if (sample_) start_ = std::chrono::steady_clock::now();
        }

        ~Span() {
            if (!sample_) return;
            ++sample_->calls;
            sample_->milliseconds += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start_).count();
        }
    };

    void write(std::uintptr_t window, double sequence) const {
        constexpr std::array phases{
            "gl_compile", "gl_link", "gl_compile_status", "gl_link_status", "gl_program_query",
            "gl_binary_read", "gl_binary_write",
            "gl_program_use", "gl_texture_allocate", "gl_texture_upload", "gl_buffer_upload",
            "gl_draw", "gl_sync"
        };
        static_assert(phases.size() == static_cast<std::size_t>(GlPhase::Count));
        for (std::size_t i = 0; i < phases.size(); ++i) {
            const auto& sample = stages_[i];
            if (!sample.calls) continue;
            ScrollTraceEvent event;
            event.component = "win32";
            event.phase = phases[i];
            event.object = window;
            event.durationMs = sample.milliseconds;
            event.detailA = sequence;
            event.detailB = sample.calls;
            writeScrollTrace(event);
        }
    }
};

// Retain the original interface for exactly the lifetime of the wrapped
// functions. Windows may expose different function pointers by GL context.
class TracedWin32GlInterface final : public GrGLInterface {
    sk_sp<const GrGLInterface> native_;

    template <GlPhase Phase, auto Function>
    void wrap() {
        if (!(fFunctions.*Function)) return;
        fFunctions.*Function = [native = native_.get()](auto... args) -> decltype(auto) {
            const GlFlushTrace::Span timer(Phase);
            return (native->fFunctions.*Function)(args...);
        };
    }

public:
    explicit TracedWin32GlInterface(sk_sp<const GrGLInterface> native) : native_(std::move(native)) {
        fStandard = native_->fStandard;
        fExtensions = native_->fExtensions;
        fFunctions = native_->fFunctions;
        wrap<GlPhase::Compile, &Functions::fCompileShader>();
        wrap<GlPhase::Link, &Functions::fLinkProgram>();
        if (fFunctions.fGetProgramiv) {
            fFunctions.fGetProgramiv = [native = native_.get()](GrGLuint program, GrGLenum name, GrGLint* result) {
                constexpr GrGLenum linkStatus = 0x8B82; // GL_LINK_STATUS
                const GlFlushTrace::Span timer(name == linkStatus ? GlPhase::LinkStatus : GlPhase::ProgramQuery);
                native->fFunctions.fGetProgramiv(program, name, result);
            };
        }
        wrap<GlPhase::CompileStatus, &Functions::fGetShaderiv>();
        wrap<GlPhase::BinaryRead, &Functions::fProgramBinary>();
        wrap<GlPhase::BinaryWrite, &Functions::fGetProgramBinary>();
        wrap<GlPhase::ProgramUse, &Functions::fUseProgram>();
        wrap<GlPhase::TextureAllocate, &Functions::fTexImage2D>();
        wrap<GlPhase::TextureAllocate, &Functions::fTexStorage2D>();
        wrap<GlPhase::TextureUpload, &Functions::fTexSubImage2D>();
        wrap<GlPhase::BufferUpload, &Functions::fBufferData>();
        wrap<GlPhase::BufferUpload, &Functions::fBufferSubData>();
        wrap<GlPhase::Draw, &Functions::fDrawArrays>();
        wrap<GlPhase::Draw, &Functions::fDrawElements>();
        wrap<GlPhase::Draw, &Functions::fDrawRangeElements>();
        wrap<GlPhase::Draw, &Functions::fDrawArraysInstanced>();
        wrap<GlPhase::Draw, &Functions::fDrawElementsInstanced>();
        wrap<GlPhase::Sync, &Functions::fFlush>();
        wrap<GlPhase::Sync, &Functions::fFinish>();
        wrap<GlPhase::Sync, &Functions::fWaitSync>();
        wrap<GlPhase::Sync, &Functions::fClientWaitSync>();
    }
};

inline sk_sp<const GrGLInterface> makeWin32GlTraceInterface() {
    auto native = GrGLMakeNativeInterface();
    if (!native) return nullptr;
    return sk_make_sp<TracedWin32GlInterface>(std::move(native));
}

} // namespace oneui::internal
