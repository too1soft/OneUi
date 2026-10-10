#include "internal/workspace_frame_probe.h"
namespace oneui::internal {
void setWorkspaceFrameProbe(std::function<void(WorkspaceFrameTiming)> callback) {
    workspaceFrameProbe=std::move(callback);
}
}
