#pragma once

#include <string>

namespace oneui::internal {

// Retain a validated base for later independent mounts on this UI thread.
void retainViewMountStyles(const std::string& css);

} // namespace oneui::internal
