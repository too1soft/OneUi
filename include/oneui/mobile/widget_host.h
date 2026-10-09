#pragma once
#include "oneui/mobile/runtime.h"
#include "oneui/widget.h"
#include <memory>

namespace oneui::mobile {
// Embeds existing widgets without constructing a desktop Window. The native
// caller supplies a Canvas already scaled to logical pixels and clipped to its
// surface. This is not a GPU backend, IME, accessibility bridge or gesture arena.
// All operations/destruction use the UI thread. Pump input immediately after
// native input/lifecycle changes, even when no frame will be drawn.
class WidgetHost final {
  public:
    WidgetHost();
    ~WidgetHost();
    WidgetHost(const WidgetHost &) = delete;
    WidgetHost &operator=(const WidgetHost &) = delete;
    Runtime &runtime();
    // An already attached widget is rejected. Queue content replacement after
    // input/paint callbacks return; reentrant replacement reports INVALID_STATE.
    OneUiMobileResult setContent(std::shared_ptr<Widget> content);
    OneUiMobileResult pumpInput();
    // Opt-in initial ScrollView vertical pan. Horizontal gestures stay with the
    // pressed control. Inertia, pinch and nested overscroll are not yet provided.
    OneUiMobileResult setTouchScrolling(bool enabled);
    // EMPTY means no paint was needed. The host uses the legacy primary-pointer
    // mouse path for existing widgets. Full touch data remains available through
    // Runtime for native mobile controls; optional vertical ScrollView pan is
    // handled above. Full gesture/pinch arbitration is not provided.
    OneUiMobileResult paint(Canvas &canvas, uint64_t timeNs);

  private:
    struct State;
    std::shared_ptr<State> state_;
};
} // namespace oneui::mobile
