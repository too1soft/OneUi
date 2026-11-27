#pragma once

#include "internal/interaction_trace.h"
#include "include/core/SkPicture.h"

#include <windows.h>
#include <memory>

namespace oneui::win32 {

inline constexpr UINT glWorkerRecordingReadyMessage = WM_APP + 6;
inline constexpr UINT glWorkerResizeResumedMessage = WM_APP + 8;

struct StartupGlScene;

// Experimental Win32 GL renderer. The render thread owns the context and GPU
// surfaces. The window thread produces immutable recordings in client pixels.
class GlRenderWorker {
public:
    struct Frame {
        int width = 0;
        int height = 0;
        RECT damage{};
        bool retained = false;
        bool interactiveResize = false;
        std::uint64_t extent = 0;
        std::uint64_t serial = 0;
        bool softwareHandoff = false;
        bool skipResizeRaster = false;
        std::uint64_t resizeGeneration = 0;

        bool coversClient() const noexcept {
            return damage.left == 0 && damage.top == 0 && damage.right == width && damage.bottom == height;
        }
    };

    ~GlRenderWorker();
    bool start(HWND window);
    bool start(HWND window, StartupGlScene scene);
    bool startBeforeSoftwareFrame(HWND window);
    bool wantsStartupPicture() const noexcept;
    bool setStartupPicture(StartupGlScene scene);
    bool ready() const noexcept;
    bool softwareHandoffPending() const noexcept;
    bool vsync() const noexcept;
    // Register a single notification if a recorded scene is waiting for the worker.
    bool deferRecording();
    Frame plan(int width, int height, RECT damage, bool interactiveResize, std::uint64_t resizeGeneration);
    bool enqueue(sk_sp<SkPicture> picture, const Frame& frame,
                 const internal::InteractionSnapshot& trace);
    bool hasPresentedResize(std::uint64_t generation) const;
    bool drawsSoftwareResize() const;
    bool finishSoftwareResize();
    bool takeSoftwareResizeResume();
    void invalidateExtent(bool duringInteractiveSizeChange = false);
    void stop();

private:
    bool start(HWND window, StartupGlScene scene, bool awaitSoftwareFrame);
    struct State;

    std::shared_ptr<State> state_;
};

bool experimentalGlWorkerEnabled() noexcept;
bool experimentalAngleWorkerEnabled() noexcept;
bool experimentalAngleWorkerStartsEarly() noexcept;
bool experimentalWglWorkerStartsEarly() noexcept;
bool experimentalGlWorkerAfterFirstFrame() noexcept;
bool experimentalGlWorkerPrewarm() noexcept;
bool experimentalGlWorkerDefersCensus() noexcept;
bool experimentalGlWorkerPacesRecording() noexcept;
bool experimentalAngleCoalescesSoftwareHandoff() noexcept;
bool workerFoldsVirtualRowDamage() noexcept;

} // namespace oneui::win32
