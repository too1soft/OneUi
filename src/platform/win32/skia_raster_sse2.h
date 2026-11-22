#pragma once

namespace oneui::win32 {
// Installs the bundled raster functions during startup, before rendering threads run.
void installSkiaRasterSse2();
}
