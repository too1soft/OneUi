    template <typename Handler>
    void dispatchMouse(
        LPARAM lParam,
        MouseButton button,
        int clickCount,
        const char* phase,
        Handler handler) {
        const auto point = logicalPointFromLParam(lParam);
        if (!prepareInputLayout(point)) {
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
        const bool tracing = internal::scrollTraceEnabled();
        if (tracing) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", phase, reinterpret_cast<std::uintptr_t>(hwnd_),
                static_cast<double>(static_cast<int>(button)), 0.0,
                static_cast<double>(clickCount), 0.0, 0.0, 0.0, 0.0,
                static_cast<double>(event.position.x),
                static_cast<double>(event.position.y)});
        }
        const auto alive = callbackWindowAlive_;
        const auto content = content_;
        const double started = tracing ? internal::scrollTraceNowMs() : 0.0;
        const bool handled = handler(*content, event);
        if (!alive->load(std::memory_order_acquire)) return;
        if (tracing) {
            const double elapsed = internal::scrollTraceNowMs() - started;
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", handled ? "pointer_handled" : "pointer_unhandled",
                reinterpret_cast<std::uintptr_t>(hwnd_),
                static_cast<double>(static_cast<int>(button)), 0.0,
                static_cast<double>(clickCount), 0.0, 0.0, 0.0, elapsed,
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

    void dispatchMouseLeave() {
        mouseLeaveTracking_ = false;
        lastCursorKind_ = CursorKind::Default;
        hasLastCursorPoint_ = false;
        SetCursor(cursorForKind(CursorKind::Default));
        clearTooltip();
        tooltipWaitForMove_ = false;
        internal::writeScrollTrace({
            "win32", leftMouseCaptured_ ? "pointer_leave_captured" : "pointer_leave",
            reinterpret_cast<std::uintptr_t>(hwnd_)});
        // The captured gesture still owns its release, including outside the client area.
        if (!leftMouseCaptured_) clearPointerInteraction();
    }

    void dispatchLeftCaptureLost() {
        if (!std::exchange(leftMouseCaptured_, false)) return;
        internal::writeScrollTrace({
            "win32", "pointer_capture_lost", reinterpret_cast<std::uintptr_t>(hwnd_)});
        clearPointerInteraction();
    }

    void clearPointerInteraction() {
        const auto alive = callbackWindowAlive_;
        const auto content = content_;
        if (content && content->clearInteractionState() && alive->load(std::memory_order_acquire)) {
            requestInteractiveRedraw();
        }
    }

    bool prepareInputLayout(std::optional<Point> pointer = {}) {
        const auto alive = callbackWindowAlive_;
        // Keep snapshot state alive if a size observer destroys this window.
        const auto layout = inputLayout_;
        for (int attempt = 0; attempt < 4; ++attempt) {
            if (!hwnd_ || !content_ || !content_->visible()) return false;
            const auto content = content_;
            const Size size = clientSize();
            const bool resolved = layout->prepare(content, Rect{0, 0, size.width, size.height},
                startupTablePreparationEnabled() ? pointer : std::nullopt);
            if (!alive->load(std::memory_order_acquire)) return false;
            if (content_ != content) continue;
            return resolved && content->visible();
        }
        return false;
    }
