#pragma once
#include "oneui/mobile/c_api.h"
#include "oneui/layout/scroll_view.h"
namespace oneui::mobile {
// Initial vertical-pan arbitration for retained ScrollView pages. No inertia,
// pinch, nested overscroll handoff or text-selection gesture policy yet.
class TouchScroll {
  public:
    bool dispatch(const std::shared_ptr<Widget> &root, const OneUiMobilePointerV1 &event);
    void reset();
  private:
    std::weak_ptr<ScrollView> target_;
    Point start_{};
    float offset_ = 0;
    bool dragging_ = false, horizontal_ = false;
};
} // namespace oneui::mobile
