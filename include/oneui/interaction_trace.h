#pragma once
#include "oneui/export.h"
#include <cstdint>

namespace oneui {
// Opt-in diagnostics: set ONEUI_INTERACTION_TRACE to a CSV path before startup.
// Capture on the UI thread inside the input/action callback, before deferring work.
// nullptr means the currently dispatched input; outside that scope it returns 0.
ONEUI_API std::uint64_t currentInteractionOperation(void* nativeWindow = nullptr);
// UI thread only. Keep the captured operation for all deferred route phases.
// Supported phases: action_start, route_requested, route_commit, route_ready,
// action_end, measurement_end. route_commit resets ready; route_ready means the
// actual target content has mounted and flushed. nullptr resolves by operation,
// never by the most recent input. SwapBuffers return is not screen presentation.
ONEUI_API void markInteractionPhase(void* nativeWindow, std::uint64_t operation,
                                   std::uint64_t generation, const char* phase);
// Call after the application's main window loop returns, before unloading the
// SDK or exiting the process. Drains the writer and emits its final drop count.
// No token is required; disabled tracing is a constant no-op.
ONEUI_API void finishInteractionTrace();
} // namespace oneui
