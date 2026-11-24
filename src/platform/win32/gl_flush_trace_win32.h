#pragma once

#include "internal/interaction_trace.h"
#include "internal/scroll_trace.h"

#include <windows.h>
#include "include/gpu/ganesh/gl/GrGLInterface.h"

#include <array>
#include <chrono>
#include <utility>

namespace oneui::internal {

inline bool win32GlResourceLabelsEnabled() {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_WIN32_GL_RESOURCE_LABELS", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

inline bool win32GlTraceEnabled() {
    static const bool enabled = [] {
        wchar_t value[2]{};
        return GetEnvironmentVariableW(L"ONEUI_GL_TRACE", value, 2) == 1 && value[0] == L'1';
    }();
    return enabled;
}

enum class GlPhase : std::size_t {
    Compile, Link, CompileStatus, LinkStatus, ProgramQuery, BinaryRead, BinaryWrite, ProgramUse,
    TextureAllocate, TextureUpload, BufferUpload, Draw, Flush, Finish, Sync, ErrorQuery,
    Uniform, BufferMap, VertexSetup, Framebuffer, TextureState, RasterState, ObjectLabel, Count
};

class GlCallTrace {
    static constexpr std::array flushPhases{
        "gl_compile", "gl_link", "gl_compile_status", "gl_link_status", "gl_program_query",
        "gl_binary_read", "gl_binary_write",
        "gl_program_use", "gl_texture_allocate", "gl_texture_upload", "gl_buffer_upload",
        "gl_draw", "gl_flush", "gl_finish", "gl_sync", "gl_error_query",
        "gl_uniform", "gl_buffer_map", "gl_vertex_setup", "gl_framebuffer", "gl_texture_state", "gl_raster_state", "gl_object_label"
    };
    static_assert(flushPhases.size() == static_cast<std::size_t>(GlPhase::Count));

    struct Sample {
        double milliseconds = 0.0;
        unsigned calls = 0;
    };
    struct TextureUpload {
        double milliseconds = 0.0;
        GrGLint x, y;
        GrGLsizei width, height;
        GrGLenum format, type;
    };

    static inline thread_local GlCallTrace* active_ = nullptr;
    std::array<Sample, static_cast<std::size_t>(GlPhase::Count)> stages_{};
    TextureUpload slowestUpload_{};

public:
    class Scope {
        GlCallTrace* previous_ = active_;

    public:
        explicit Scope(GlCallTrace* trace) { active_ = trace; }
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

    class TextureUploadSpan {
        using Clock = std::chrono::steady_clock;
        GlCallTrace* trace_ = active_;
        TextureUpload upload_;
        Clock::time_point start_;

    public:
        TextureUploadSpan(GrGLint x, GrGLint y, GrGLsizei width, GrGLsizei height,
                          GrGLenum format, GrGLenum type)
            : upload_{0.0, x, y, width, height, format, type} {
            if (trace_) start_ = Clock::now();
        }

        ~TextureUploadSpan() {
            if (!trace_) return;
            upload_.milliseconds = std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
            auto& total = trace_->stages_[static_cast<std::size_t>(GlPhase::TextureUpload)];
            total.milliseconds += upload_.milliseconds;
            ++total.calls;
            if (upload_.milliseconds > trace_->slowestUpload_.milliseconds)
                trace_->slowestUpload_ = upload_;
        }
    };

    void write(std::uintptr_t window, double sequence, bool duringSubmit = false) const {
        constexpr std::array submitPhases{
            "gl_submit_compile", "gl_submit_link", "gl_submit_compile_status", "gl_submit_link_status",
            "gl_submit_program_query", "gl_submit_binary_read", "gl_submit_binary_write",
            "gl_submit_program_use", "gl_submit_texture_allocate", "gl_submit_texture_upload",
            "gl_submit_buffer_upload", "gl_submit_draw", "gl_submit_flush", "gl_submit_finish",
            "gl_submit_sync", "gl_submit_error_query", "gl_submit_uniform", "gl_submit_buffer_map",
            "gl_submit_vertex_setup", "gl_submit_framebuffer", "gl_submit_texture_state", "gl_submit_raster_state",
            "gl_submit_object_label"
        };
        const auto& phases = duringSubmit ? submitPhases : flushPhases;
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
        if (slowestUpload_.milliseconds >= 1.0) {
            ScrollTraceEvent event;
            event.component = "win32";
            event.phase = duringSubmit ? "gl_submit_texture_upload_peak" : "gl_texture_upload_peak";
            event.object = window;
            event.durationMs = slowestUpload_.milliseconds;
            event.detailA = sequence;
            event.inputDelta = slowestUpload_.x;
            event.offset = slowestUpload_.y;
            event.target = slowestUpload_.width;
            event.velocity = slowestUpload_.height;
            event.intervalMs = slowestUpload_.format;
            event.maxOffset = slowestUpload_.type;
            writeScrollTrace(event);
        }
    }

    template <typename Flush>
    static void measureWorkerFlush(const InteractionSnapshot& snapshot, bool scrollTrace, Flush flush) {
        using Clock = std::chrono::steady_clock;
        GlCallTrace calls;
        const auto cpuStart = threadCpuTicks();
        const auto start = Clock::now();
        {
            const Scope scope(&calls);
            flush();
        }
        const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        if (elapsed >= 8.0) {
            constexpr auto backend = "opengl-worker";
            const auto cpuEnd = threadCpuTicks();
            traceInteractionRenderTiming(snapshot, "worker_flush_wall", backend, elapsed);
            if (cpuStart != unavailableCpuTime && cpuEnd != unavailableCpuTime && cpuEnd >= cpuStart) {
                traceInteractionRenderTiming(snapshot, "worker_flush_cpu", backend, (cpuEnd - cpuStart) / 10000.0);
            }
            for (std::size_t i = 0; i < flushPhases.size(); ++i) {
                const auto& sample = calls.stages_[i];
                if (sample.milliseconds >= 0.5) {
                    traceInteractionRenderTiming(snapshot, flushPhases[i], backend, sample.milliseconds, sample.calls);
                }
            }
        }
        if (scrollTrace) calls.write(snapshot.window, static_cast<double>(snapshot.paintSequence));
    }

private:
    static constexpr std::uint64_t unavailableCpuTime = UINT64_MAX;

    static std::uint64_t threadCpuTicks() {
        FILETIME created{}, exited{}, kernel{}, user{};
        if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) return unavailableCpuTime;
        const auto ticks = [](FILETIME time) {
            return (std::uint64_t{time.dwHighDateTime} << 32) | time.dwLowDateTime;
        };
        return ticks(kernel) + ticks(user);
    }
};

// Retain the original interface for exactly the lifetime of the wrapped
// functions. Windows may expose different function pointers by GL context.
class Win32GaneshGlInterface final : public GrGLInterface {
    sk_sp<const GrGLInterface> native_;

    template <GlPhase Phase, auto Function>
    void wrap() {
        if (!(fFunctions.*Function)) return;
        fFunctions.*Function = [native = native_.get()](auto... args) -> decltype(auto) {
            const GlCallTrace::Span timer(Phase);
            return (native->fFunctions.*Function)(args...);
        };
    }

public:
    Win32GaneshGlInterface(sk_sp<const GrGLInterface> native, bool trace, bool suppressLabels)
        : native_(std::move(native)) {
        fStandard = native_->fStandard;
        fExtensions = native_->fExtensions;
        fFunctions = native_->fFunctions;
        // Keep KHR_debug and its required entry point. Driver-side resource
        // names are optional and labeling has stalled NVIDIA's render thread.
        if (suppressLabels && fFunctions.fObjectLabel) {
            fFunctions.fObjectLabel = [](GrGLenum, GrGLuint, GrGLsizei, const GrGLchar*) {};
        }
        if (!trace) return;
        wrap<GlPhase::Compile, &Functions::fCompileShader>();
        wrap<GlPhase::Link, &Functions::fLinkProgram>();
        if (fFunctions.fGetProgramiv) {
            fFunctions.fGetProgramiv = [native = native_.get()](GrGLuint program, GrGLenum name, GrGLint* result) {
                constexpr GrGLenum linkStatus = 0x8B82; // GL_LINK_STATUS
                const GlCallTrace::Span timer(name == linkStatus ? GlPhase::LinkStatus : GlPhase::ProgramQuery);
                native->fFunctions.fGetProgramiv(program, name, result);
            };
        }
        wrap<GlPhase::CompileStatus, &Functions::fGetShaderiv>();
        wrap<GlPhase::BinaryRead, &Functions::fProgramBinary>();
        wrap<GlPhase::BinaryWrite, &Functions::fGetProgramBinary>();
        wrap<GlPhase::ProgramUse, &Functions::fUseProgram>();
        wrap<GlPhase::ProgramQuery, &Functions::fGetUniformLocation>();
        wrap<GlPhase::Uniform, &Functions::fUniform1i>();
        wrap<GlPhase::Uniform, &Functions::fUniform1fv>();
        wrap<GlPhase::Uniform, &Functions::fUniform2fv>();
        wrap<GlPhase::Uniform, &Functions::fUniform3fv>();
        wrap<GlPhase::Uniform, &Functions::fUniform4fv>();
        wrap<GlPhase::Uniform, &Functions::fUniformMatrix3fv>();
        wrap<GlPhase::Uniform, &Functions::fUniformMatrix4fv>();
        wrap<GlPhase::TextureAllocate, &Functions::fTexImage2D>();
        wrap<GlPhase::TextureAllocate, &Functions::fTexStorage2D>();
        if (fFunctions.fTexSubImage2D) {
            fFunctions.fTexSubImage2D = [native = native_.get()](GrGLenum target, GrGLint level,
                    GrGLint x, GrGLint y, GrGLsizei width, GrGLsizei height, GrGLenum format,
                    GrGLenum type, const GrGLvoid* data) {
                const GlCallTrace::TextureUploadSpan timer(x, y, width, height, format, type);
                native->fFunctions.fTexSubImage2D(target, level, x, y, width, height, format, type, data);
            };
        }
        wrap<GlPhase::TextureState, &Functions::fActiveTexture>();
        wrap<GlPhase::TextureState, &Functions::fBindTexture>();
        wrap<GlPhase::TextureState, &Functions::fTexParameteri>();
        wrap<GlPhase::BufferUpload, &Functions::fBufferData>();
        wrap<GlPhase::BufferUpload, &Functions::fBufferSubData>();
        wrap<GlPhase::BufferMap, &Functions::fMapBuffer>();
        wrap<GlPhase::BufferMap, &Functions::fMapBufferRange>();
        wrap<GlPhase::BufferMap, &Functions::fUnmapBuffer>();
        wrap<GlPhase::VertexSetup, &Functions::fBindBuffer>();
        wrap<GlPhase::VertexSetup, &Functions::fBindVertexArray>();
        wrap<GlPhase::VertexSetup, &Functions::fEnableVertexAttribArray>();
        wrap<GlPhase::VertexSetup, &Functions::fDisableVertexAttribArray>();
        wrap<GlPhase::VertexSetup, &Functions::fVertexAttribPointer>();
        wrap<GlPhase::VertexSetup, &Functions::fVertexAttribIPointer>();
        wrap<GlPhase::Framebuffer, &Functions::fBindFramebuffer>();
        wrap<GlPhase::Framebuffer, &Functions::fFramebufferTexture2D>();
        wrap<GlPhase::Framebuffer, &Functions::fCheckFramebufferStatus>();
        wrap<GlPhase::Framebuffer, &Functions::fBlitFramebuffer>();
        wrap<GlPhase::Framebuffer, &Functions::fReadPixels>();
        wrap<GlPhase::RasterState, &Functions::fClear>();
        wrap<GlPhase::RasterState, &Functions::fEnable>();
        wrap<GlPhase::RasterState, &Functions::fDisable>();
        wrap<GlPhase::RasterState, &Functions::fScissor>();
        wrap<GlPhase::RasterState, &Functions::fViewport>();
        wrap<GlPhase::Draw, &Functions::fDrawArrays>();
        wrap<GlPhase::Draw, &Functions::fDrawElements>();
        wrap<GlPhase::Draw, &Functions::fDrawRangeElements>();
        wrap<GlPhase::Draw, &Functions::fDrawArraysInstanced>();
        wrap<GlPhase::Draw, &Functions::fDrawElementsInstanced>();
        wrap<GlPhase::Flush, &Functions::fFlush>();
        wrap<GlPhase::Finish, &Functions::fFinish>();
        wrap<GlPhase::Sync, &Functions::fFenceSync>();
        wrap<GlPhase::Sync, &Functions::fWaitSync>();
        wrap<GlPhase::Sync, &Functions::fClientWaitSync>();
        wrap<GlPhase::ErrorQuery, &Functions::fGetError>();
        if (!suppressLabels) wrap<GlPhase::ObjectLabel, &Functions::fObjectLabel>();
    }
};

inline sk_sp<const GrGLInterface> makeWin32GaneshGlInterface(bool nvidia) {
    auto native = GrGLMakeNativeInterface();
    const bool trace = win32GlTraceEnabled();
    const bool suppressLabels = nvidia && !win32GlResourceLabelsEnabled();
    if (!native || (!trace && !suppressLabels)) return native;
    return sk_make_sp<Win32GaneshGlInterface>(std::move(native), trace, suppressLabels);
}

} // namespace oneui::internal
