#pragma once

#include "oneui/export.h"
#include <cstdint>
#include <optional>
#include <string>

namespace oneui::mobile {

struct RenderStatistics {
    // Picture/swap counters reset per generation; context/resize counts belong
    // to the backend lifetime. Source changes compare adjacent swapped pictures.
    std::uint64_t generation = 0;
    std::uint64_t submitted = 0, coalesced = 0, rendered = 0;
    std::uint64_t swapSuccess = 0, swapFailure = 0;
    std::uint64_t frameBearingSwaps = 0, sourceFrameChangesSwapped = 0;
    std::uint64_t lastSource = 0, lastFrame = 0, lastSwapTimeNs = 0;
    std::uint64_t rasterWallUs = 0, contexts = 0, resizes = 0;
    bool attached = false, traceOverflow = false;
    std::string renderer, error;
};

// Snapshot for the most recently painted surface on this UI thread. Reset and
// destruction retire its observation. A successful EGL swap is not a compositor
// presentation fence and must not be reported as unique displayed video FPS.
#if defined(__ANDROID__)
ONEUI_API std::optional<RenderStatistics> latestThreadRenderStatistics();
#else
inline std::optional<RenderStatistics> latestThreadRenderStatistics() { return std::nullopt; }
#endif

} // namespace oneui::mobile
