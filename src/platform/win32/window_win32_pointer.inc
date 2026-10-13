    void updateCursor(Point point, bool force = false) {
        if (force && hasLastCursorPoint_ && point.x == lastCursorPoint_.x && point.y == lastCursorPoint_.y) {
            SetCursor(cursorForKind(lastCursorKind_));
            return;
        }

        CursorKind next = CursorKind::Default;
        if (content_ && content_->visible()) {
            next = content_->cursor(point);
        }

        if (!force && next == lastCursorKind_) {
            lastCursorPoint_ = point;
            hasLastCursorPoint_ = true;
            return;
        }

        lastCursorKind_ = next;
        lastCursorPoint_ = point;
        hasLastCursorPoint_ = true;
        SetCursor(cursorForKind(next));
    }

    void dispatchMouseMove(Point point) {
        trackMouseLeave();
        if (!content_ || !content_->visible()) {
            clearTooltip();
            return;
        }

        updateTooltip(point);

        MouseEvent event{point};
        const bool changed = content_->onMouseMove(event);
        if (changed) {
            flushInteractivePaint();
        }
    }

    void updateTooltip(Point point) {
        const std::wstring* next = content_ ? content_->tooltipAt(point) : nullptr;
        const std::wstring nextText = next ? *next : std::wstring{};
        tooltipPoint_ = point;
        if (nextText == hoveredTooltip_) {
            return;
        }
        if (hwnd_) {
            KillTimer(hwnd_, kOneUiTooltipTimer);
        }
        hoveredTooltip_ = nextText;
        tooltipVisible_ = false;
        if (!hoveredTooltip_.empty() && hwnd_) {
            SetTimer(hwnd_, kOneUiTooltipTimer, kOneUiTooltipDelayMs, nullptr);
        }
        requestRedraw();
    }

    void clearTooltip() {
        if (hwnd_) {
            KillTimer(hwnd_, kOneUiTooltipTimer);
        }
        if (hoveredTooltip_.empty() && !tooltipVisible_) {
            return;
        }
        hoveredTooltip_.clear();
        tooltipVisible_ = false;
        requestRedraw();
    }

    void paintTooltip(Canvas& canvas) const {
        if (!tooltipVisible_ || hoveredTooltip_.empty()) {
            return;
        }
        const auto viewport = canvas.viewportBounds();
        if (!viewport) {
            return;
        }
        constexpr float fontSize = 12.0f;
        constexpr float horizontalPadding = 10.0f;
        constexpr float height = 30.0f;
        const float width = std::clamp(
            canvas.measureTextWidth(hoveredTooltip_, fontSize, 500) + horizontalPadding * 2.0f,
            44.0f,
            360.0f);
        float x = tooltipPoint_.x + 12.0f;
        float y = tooltipPoint_.y + 18.0f;
        x = std::clamp(x, viewport->x + 6.0f, viewport->x + viewport->width - width - 6.0f);
        if (y + height > viewport->y + viewport->height - 6.0f) {
            y = tooltipPoint_.y - height - 10.0f;
        }
        y = std::max(viewport->y + 6.0f, y);
        const Rect frame{x, y, width, height};
        canvas.drawBoxShadow(frame, BoxShadow{Color{0, 0, 0, 92}, Point{0.0f, 4.0f}, 12.0f, 0.0f}, 5.0f);
        canvas.fillRect(frame, Color{37, 39, 49}, 5.0f);
        canvas.strokeRect(frame, Color{78, 82, 101}, 5.0f, 1.0f);
        canvas.drawTextStyledEllipsized(
            hoveredTooltip_,
            Rect{x + horizontalPadding, y, width - horizontalPadding * 2.0f, height},
            Color{235, 237, 245},
            fontSize,
            TextAlign::Left,
            500);
    }

    void flushInteractivePaint() {
        if (!hwnd_) {
            return;
        }

        // User input and live resize feedback should be rendered in the same message turn.
        // Timer coalescing remains available for animation callbacks and non-interactive work,
        // but pointer hover/focus/pressed states need immediate feedback to feel attached.
        const bool traceScroll = internal::scrollTraceEnabled();
        const double startMs = traceScroll ? internal::scrollTraceNowMs() : 0.0;
        flushPendingPaint();
        if (traceScroll) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "interactive_flush", reinterpret_cast<std::uintptr_t>(hwnd_),
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                internal::scrollTraceNowMs() - startMs, 0.0, 0.0});
        }
    }

    void scheduleInteractivePaint() {
        if (!hwnd_) {
            return;
        }

        if (!interactivePaintTimerActive_) {
            SetTimer(hwnd_, kOneUiInteractivePaintTimer, kOneUiInteractivePaintIntervalMs, nullptr);
            interactivePaintTimerActive_ = true;
        }
    }

    void flushPendingPaint() {
        if (!hwnd_) {
            return;
        }
        submitInvalidations();
        RECT updateRect{};
        if (GetUpdateRect(hwnd_, &updateRect, FALSE)) {
            UpdateWindow(hwnd_);
        }
    }

    void dispatchMouseDown(
        LPARAM lParam,
        MouseButton button = MouseButton::Left,
        int clickCount = 1) {
        dispatchMouse(lParam, button, clickCount, "pointer_down", [](Widget& widget, const MouseEvent& event) {
            return widget.onMouseDown(event);
        });
    }

    void dispatchMouseUp(LPARAM lParam, MouseButton button = MouseButton::Left) {
        dispatchMouse(lParam, button, 1, "pointer_up", [](Widget& widget, const MouseEvent& event) {
            return widget.onMouseUp(event);
        });
    }

    void dispatchMouseWheel(WPARAM wParam, LPARAM lParam) {
        if (!content_ || !content_->visible()) {
            return;
        }

        const bool traceScroll = internal::scrollTraceEnabled();
        const double inputStartMs = traceScroll ? internal::scrollTraceNowMs() : 0.0;
        const double inputIntervalMs = traceScroll && scrollTraceLastWheelMessageMs_ > 0.0
            ? inputStartMs - scrollTraceLastWheelMessageMs_
            : 0.0;
        if (traceScroll) {
            scrollTraceLastWheelMessageMs_ = inputStartMs;
        }

        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd_, &point);
        const short rawDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        const float delta = static_cast<float>(rawDelta) / static_cast<float>(WHEEL_DELTA);
        if (traceScroll) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "wheel_received", reinterpret_cast<std::uintptr_t>(hwnd_),
                delta, 0.0, 0.0, 0.0, inputIntervalMs, 0.0, 0.0,
                static_cast<double>(rawDelta), static_cast<double>(WHEEL_DELTA)});
        }
        MouseWheelEvent event{
            logicalPointFromClientPixels(point.x, point.y),
            delta,
            (GET_KEYSTATE_WPARAM(wParam) & MK_SHIFT) != 0,
            (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0,
            (GetKeyState(VK_MENU) & 0x8000) != 0,
            currentTimeMs(),
        };
        const bool handled = content_->onMouseWheel(event);
        if (traceScroll) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "wheel_dispatched", reinterpret_cast<std::uintptr_t>(hwnd_),
                delta, 0.0, 0.0, 0.0, inputIntervalMs, 0.0,
                internal::scrollTraceNowMs() - inputStartMs,
                handled ? 1.0 : 0.0, 0.0});
        }
        // Scroll animations are sampled on the display-synchronized frame.
        // A synchronous paint here would present an unchanged frame, block input
        // dispatch, and shift the first useful sample away from vsync.
    }

    void dispatchFocusChanged(bool focused) {
        const auto alive = callbackWindowAlive_;
        if (!focused) cancelImeComposition();
        if (!alive->load(std::memory_order_acquire)) return;
        const auto content = content_;
        if (content && content->onFocusChanged(focused) && alive->load(std::memory_order_acquire)) {
            requestRedraw();
        }
    }

    void dispatchActivationChanged(bool active) {
        if (windowActive_ == active) {
            return;
        }
        windowActive_ = active;
        const auto alive = callbackWindowAlive_;
        const auto handler = activationChangedHandler_;
        if (handler && alive->load(std::memory_order_acquire)) {
            handler(active);
        }
    }
