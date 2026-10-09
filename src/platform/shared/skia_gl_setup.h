#pragma once

#include "include/gpu/ganesh/GrContextOptions.h"

namespace oneui::rendering {

// Process-wide on-disk cache of compiled GPU programs for Skia Ganesh.
//
// Without it every launch compiles each shader variant the first time it is
// drawn. With it, only the first run on a machine (or after a driver update
// invalidates the binaries) compiles.
//
// Location: ONEUI_SHADER_CACHE_DIR, else %LOCALAPPDATA%\OneUI\ShaderCache\<exe>
// on Windows and $XDG_CACHE_HOME/oneui/shaders/<exe> elsewhere. Returns null
// when no writable location exists; callers then run without a cache.
GrContextOptions::PersistentCache* shaderCache();

} // namespace oneui::rendering
