#pragma once

#include "internal/interaction_trace.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPicture.h"
#include "include/core/SkSurface.h"

#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <utility>
#include <windows.h>

namespace oneui::win32 {

inline constexpr UINT startupRasterReadyMessage = WM_APP + 7;

// The window thread owns the mailbox. The callback can outlive the HWND and
// only owns an immutable recording and a private raster surface.
class StartupRaster {
public:
    struct Frame {
        sk_sp<SkPicture> picture;
        int width = 0;
        int height = 0;
        float scale = 0;
        internal::InteractionSnapshot trace;
        sk_sp<SkSurface> surface;
    };

    static bool enabled() noexcept {
        static const bool value = [] {
            wchar_t flag[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_ASYNC_STARTUP_RASTER", flag, 2) == 1 && flag[0] == L'1';
        }();
        return value;
    }

    ~StartupRaster() { cancel(); }

    bool submit(HWND window, Frame frame, const SkSurface& backing) {
        if (!flight_.expired() || !frame.picture) return false;
        try {
            auto state = std::make_shared<Job>();
            state->window = window;
            state->ticket = nextTicket_.fetch_add(1, std::memory_order_relaxed);
            state->frame = std::move(frame);
            state->imageInfo = backing.imageInfo();
            state->props = backing.props();
            auto context = std::make_unique<Callback>();
            context->job = state;
            if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                    reinterpret_cast<LPCWSTR>(&Callback::run), &context->module)) return false;
            auto* submitted = context.release();
            if (!TrySubmitThreadpoolCallback(Callback::run, submitted, nullptr)) {
                FreeLibrary(submitted->module);
                delete submitted;
                return false;
            }
            flight_ = state;
            pending_ = std::move(state);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    bool pending() const noexcept { return bool(pending_); }
    bool matches(WPARAM ticket) const noexcept { return pending_ && ticket == pending_->ticket; }

    Frame takeOrCancel() {
        auto job = std::exchange(pending_, {});
        if (!job) return {};
        std::lock_guard lock(job->mutex);
        job->window = nullptr;
        if (job->complete) return std::move(job->frame);
        internal::traceInteractionPaint(job->frame.trace, "startup_raster_discard", "software");
        return {};
    }

    bool cancel() {
        auto job = std::exchange(pending_, {});
        if (!job) return false;
        std::lock_guard lock(job->mutex);
        job->window = nullptr;
        internal::traceInteractionPaint(job->frame.trace, "startup_raster_discard", "software");
        return true;
    }

private:
    struct Job {
        std::mutex mutex;
        HWND window = nullptr;
        WPARAM ticket = 0;
        Frame frame;
        SkImageInfo imageInfo;
        SkSurfaceProps props;
        bool complete = false;
    };

    struct Callback {
        std::shared_ptr<Job> job;
        HMODULE module = nullptr;

        static void CALLBACK run(PTP_CALLBACK_INSTANCE instance, void* context) noexcept {
            std::unique_ptr<Callback> callback(static_cast<Callback*>(context));
            FreeLibraryWhenCallbackReturns(instance, callback->module);
            auto& job = *callback->job;
            {
                std::lock_guard lock(job.mutex);
                if (!job.window) return;
            }
            internal::traceInteractionPaint(job.frame.trace, "startup_raster_begin", "software");
            sk_sp<SkSurface> surface;
            try {
                surface = SkSurfaces::Raster(job.imageInfo, &job.props);
                if (surface) surface->getCanvas()->drawPicture(job.frame.picture);
            } catch (const std::exception&) {
                surface.reset();
            }
            internal::traceInteractionPaint(job.frame.trace, "startup_raster_end", "software", surface ? 1 : 0);
            std::lock_guard lock(job.mutex);
            if (!job.window) return;
            job.frame.surface = std::move(surface);
            job.complete = true;
            if (!PostMessageW(job.window, startupRasterReadyMessage, job.ticket, 0))
                InvalidateRect(job.window, nullptr, FALSE);
        }
    };

    inline static std::atomic<WPARAM> nextTicket_{1};
    std::weak_ptr<Job> flight_;
    std::shared_ptr<Job> pending_;
};

} // namespace oneui::win32
