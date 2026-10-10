#pragma once
#include <functional>
#include "oneui/export.h"
namespace oneui::internal {
// Internal native acceptance instrumentation, no exported C symbols. Includes
// startup driver-census work; only SwapBuffers wall time is separated.
struct WorkspaceFrameTiming { double paintMs, presentMs, updateMs, censusMs; };
inline thread_local std::function<void(WorkspaceFrameTiming)> workspaceFrameProbe;
inline thread_local double workspacePresentMs=0;
inline thread_local double workspaceUpdateMs=0;
inline thread_local double workspaceCensusMs=0;
// Internal C++ test hook, deliberately absent from the public C/Rust ABI.
ONEUI_API void setWorkspaceFrameProbe(std::function<void(WorkspaceFrameTiming)>);
inline void recordWorkspaceFrame(double elapsed) {
    auto update=workspaceUpdateMs;workspaceUpdateMs=0;
    if(workspaceFrameProbe)workspaceFrameProbe({elapsed,workspacePresentMs,update,workspaceCensusMs});
    workspacePresentMs=0;
    workspaceCensusMs=0;
}
} // namespace oneui::internal
