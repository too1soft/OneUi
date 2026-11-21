#pragma once

#include <windows.h>

#include <thread>

namespace oneui::win32 {

PIXELFORMATDESCRIPTOR openGlWindowPixelFormat() noexcept;

// Runs the driver-only startup work on its own Win32 window and GL context.
// Joining during destruction keeps the worker inside the owning SDK lifetime.
class NvidiaOpenGlWarmup {
public:
    ~NvidiaOpenGlWarmup();
    void start(HWND window);

private:
    std::thread worker_;
};

} // namespace oneui::win32
