#pragma once
#include <memory>
#include <string>
struct ANativeWindow;
namespace oneui {
class Canvas;
namespace mobile {
// UI-thread EGL/Ganesh surface. Widget ownership stays with WidgetHost across
// detach/attach. Each surface owns its GL context; EGL display ownership is shared.
class SkiaSurface final {
  public:
    SkiaSurface();
    ~SkiaSurface();
    SkiaSurface(const SkiaSurface &) = delete;
    SkiaSurface &operator=(const SkiaSurface &) = delete;
    bool attach(ANativeWindow *acquiredWindow, int width, int height);
    void reset();
    std::unique_ptr<Canvas> begin(float scale);
    bool present();
    const std::string &renderer() const;
    const std::string &error() const;
  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace mobile
} // namespace oneui
