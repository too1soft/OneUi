#pragma once
#include <cstdint>
#include <memory>
#include <string>
struct ANativeWindow;
namespace oneui {
class Canvas;
namespace mobile {
// UI-thread picture recording with one pending picture and one raster thread.
// Widgets stay on their owner thread. EGL/Ganesh and native-window operations
// belong exclusively to the raster thread. Release Canvas before present/reset.
class SkiaSurface final {
  public:
    SkiaSurface();
    ~SkiaSurface();
    SkiaSurface(const SkiaSurface &) = delete;
    SkiaSurface &operator=(const SkiaSurface &) = delete;
    bool attach(ANativeWindow *acquiredWindow, int width, int height);
    void reset();
    std::unique_ptr<Canvas> begin(float scale);
    // True means the picture was queued. Actual EGL swaps have separate stats.
    bool present();
    const std::string &renderer() const;
    const std::string &error() const;
    uint64_t contextCount() const;
    uint64_t resizeCount() const;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace mobile
} // namespace oneui
