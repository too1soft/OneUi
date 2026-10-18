    template <typename Handler>
    void dispatchMouse(
        LPARAM lParam,
        MouseButton button,
        int clickCount,
        const char* phase,
        Handler handler) {
        if (!content_ || !content_->visible()) {
            return;
        }

        MouseEvent event{logicalPointFromLParam(lParam)};
        event.button = button;
        event.shift = trackedModifierDown(VK_SHIFT, VK_LSHIFT, VK_RSHIFT) ||
                      (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        event.control = trackedModifierDown(VK_CONTROL, VK_LCONTROL, VK_RCONTROL) ||
                        (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        event.alt = trackedModifierDown(VK_MENU, VK_LMENU, VK_RMENU) ||
                    (GetKeyState(VK_MENU) & 0x8000) != 0;
        event.clickCount = clickCount;
        if (internal::scrollTraceEnabled()) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", phase, reinterpret_cast<std::uintptr_t>(hwnd_),
                static_cast<double>(static_cast<int>(button)), 0.0,
                static_cast<double>(clickCount), 0.0, 0.0, 0.0, 0.0,
                static_cast<double>(event.position.x),
                static_cast<double>(event.position.y)});
        }
        const auto alive = callbackWindowAlive_;
        const auto content = content_;
        const bool handled = handler(*content, event);
        if (!alive->load(std::memory_order_acquire)) return;
        if (internal::scrollTraceEnabled()) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", handled ? "pointer_handled" : "pointer_unhandled",
                reinterpret_cast<std::uintptr_t>(hwnd_),
                static_cast<double>(static_cast<int>(button)), 0.0,
                static_cast<double>(clickCount), 0.0, 0.0, 0.0, 0.0,
                static_cast<double>(event.position.x),
                static_cast<double>(event.position.y)});
        }
        if (handled) {
            requestInteractiveRedraw();
        }
        updateImePosition();
    }

    void trackMouseLeave() {
        if (mouseLeaveTracking_ || !hwnd_) {
            return;
        }
        TRACKMOUSEEVENT event{};
        event.cbSize = sizeof(TRACKMOUSEEVENT);
        event.dwFlags = TME_LEAVE;
        event.hwndTrack = hwnd_;
        if (TrackMouseEvent(&event)) {
            mouseLeaveTracking_ = true;
        }
    }
