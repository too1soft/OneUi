#pragma once

#include "oneui/export.h"
#include <string>

namespace oneui::internal {

// Snapshot at window construction. Environment edits apply to newly created
// windows; renderer setup and diagnostics use the same snapshot for a window.
struct ONEUI_API RuntimeConfig {
    bool gpuEnabled = true;
    bool developerTools = false;
    bool renderTrace = false;
    std::wstring renderTraceFile;

    static RuntimeConfig fromEnvironment();
};

} // namespace oneui::internal
