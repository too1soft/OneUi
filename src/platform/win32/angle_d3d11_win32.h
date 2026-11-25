#pragma once

#include "include/core/SkRefCnt.h"

#include <memory>
#include <string>
#include <windows.h>

class GrDirectContext;
class SkSurface;

namespace oneui::win32 {

// ANGLE owns the D3D11 swap chain. Win32Window still owns the Skia surfaces.
class AngleD3D11Context {
public:
    static bool requested() noexcept;
    static std::unique_ptr<AngleD3D11Context> open(HWND window, sk_sp<GrDirectContext>& skia);

    ~AngleD3D11Context();
    bool makeCurrent();
    bool refreshBufferSize(int& width, int& height, bool clientSizeChanged);
    bool present();
    bool vsync() const noexcept;
    bool invertedSurface() const noexcept;
    int stencilBits() const noexcept;
    const std::string& device() const noexcept;
    void releaseSkia(sk_sp<SkSurface>& window, sk_sp<SkSurface>& retained, sk_sp<GrDirectContext>& skia);

private:
    struct State;
    explicit AngleD3D11Context(std::unique_ptr<State> state);
    std::unique_ptr<State> state_;
};

} // namespace oneui::win32
