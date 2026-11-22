#pragma once

#include <windows.h>

#include <thread>

namespace oneui::win32 {

PIXELFORMATDESCRIPTOR openGlWindowPixelFormat() noexcept;

// Warms the driver and a common coverage shader on its own Win32 window
// and GL context. Destruction joins the worker before SDK resources can unload.
class NvidiaOpenGlWarmup {
public:
    ~NvidiaOpenGlWarmup();
    void start(HWND window);

private:
    std::thread worker_;
};

} // namespace oneui::win32
