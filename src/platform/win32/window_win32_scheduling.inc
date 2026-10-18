    void runPostedCallbacks() {
        const auto alive = callbackWindowAlive_;
        std::queue<std::function<void()>> callbacks;
        {
            std::lock_guard<std::mutex> lock(postedCallbacksMutex_);
            callbacks.swap(postedCallbacks_);
        }

        while (!callbacks.empty()) {
            if (!alive->load(std::memory_order_acquire)) return;
            auto callback = std::move(callbacks.front());
            callbacks.pop();
            callback();
        }
    }

    void runAnimationFrameCallbacks() {
        const auto alive = callbackWindowAlive_;
        const bool traceScroll = internal::scrollTraceEnabled();
        const double frameStartMs = traceScroll ? internal::scrollTraceNowMs() : 0.0;
        const double frameIntervalMs = traceScroll && scrollTraceLastAnimationFrameMs_ > 0.0
            ? frameStartMs - scrollTraceLastAnimationFrameMs_
            : 0.0;
        if (traceScroll) {
            scrollTraceLastAnimationFrameMs_ = frameStartMs;
        }
        if (hwnd_ && animationTimerActive_) {
            KillTimer(hwnd_, kOneUiAnimationTimer);
            animationTimerActive_ = false;
        }
        animationFramePending_ = false;
        deferredAnimationFrameMs_ = 0.0;

        std::queue<std::function<void(double)>> callbacks;
        {
            std::lock_guard<std::mutex> lock(animationCallbacksMutex_);
            callbacks.swap(animationCallbacks_);
        }
        if (traceScroll) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "animation_frame_begin", reinterpret_cast<std::uintptr_t>(hwnd_),
                0.0, 0.0, 0.0, 0.0, frameIntervalMs, 0.0, 0.0,
                static_cast<double>(callbacks.size()), swapIntervalEnabled_ ? 1.0 : 0.0});
        }

        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const double nowMs = std::chrono::duration<double, std::milli>(now).count();
        lastAnimationFrameMs_ = nowMs;
        while (!callbacks.empty()) {
            if (!alive->load(std::memory_order_acquire)) return;
            auto callback = std::move(callbacks.front());
            callbacks.pop();
            callback(nowMs);
        }
        if (!alive->load(std::memory_order_acquire)) return;
        flushPendingPaint();
        if (traceScroll) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "animation_frame_presented", reinterpret_cast<std::uintptr_t>(hwnd_),
                0.0, 0.0, 0.0, 0.0, frameIntervalMs, 0.0,
                internal::scrollTraceNowMs() - frameStartMs, 0.0,
                swapIntervalEnabled_ ? 1.0 : 0.0});
        }
    }

    // Arms the frame timer for the next display refresh, or for `notBeforeMs`
    // when that is later.
    bool armAnimationFrameTimer(double notBeforeMs = 0.0) {
        if (!animationFrameTimer_) {
            return false;
        }

        const double nowMs = currentTimeMs();
        const double refreshIntervalMs = animationFrameIntervalMs();
        const double scheduledMs = std::max(notBeforeMs, lastAnimationFrameMs_ > 0.0
            ? lastAnimationFrameMs_ + refreshIntervalMs
            : nowMs);
        const double delayMs = std::max(0.5, scheduledMs - nowMs);
        LARGE_INTEGER dueTime{};
        dueTime.QuadPart = -std::max<LONGLONG>(
            1,
            static_cast<LONGLONG>(std::llround(delayMs * 10000.0)));
        return SetWaitableTimer(
            animationFrameTimer_,
            &dueTime,
            0,
            nullptr,
            nullptr,
            FALSE) == TRUE;
    }

    double animationFrameIntervalMs() {
        const HMONITOR monitor = hwnd_
            ? MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST)
            : nullptr;
        if (monitor && monitor != animationFrameMonitor_) {
            animationFrameMonitor_ = monitor;
            animationFrameIntervalMs_ = 1000.0 / 60.0;

            MONITORINFOEXW monitorInfo{};
            monitorInfo.cbSize = sizeof(monitorInfo);
            DEVMODEW displayMode{};
            displayMode.dmSize = sizeof(displayMode);
            if (GetMonitorInfoW(monitor, &monitorInfo)
                && EnumDisplaySettingsW(
                    monitorInfo.szDevice,
                    ENUM_CURRENT_SETTINGS,
                    &displayMode)
                && displayMode.dmDisplayFrequency > 1) {
                animationFrameIntervalMs_ = 1000.0
                    / static_cast<double>(displayMode.dmDisplayFrequency);
                return animationFrameIntervalMs_;
            }

            DWM_TIMING_INFO timingInfo{};
            timingInfo.cbSize = sizeof(timingInfo);
            if (SUCCEEDED(DwmGetCompositionTimingInfo(hwnd_, &timingInfo))
                && timingInfo.rateRefresh.uiNumerator > 0
                && timingInfo.rateRefresh.uiDenominator > 0) {
                const double intervalMs = 1000.0
                    * static_cast<double>(timingInfo.rateRefresh.uiDenominator)
                    / static_cast<double>(timingInfo.rateRefresh.uiNumerator);
                if (intervalMs >= 2.0 && intervalMs <= 100.0) {
                    animationFrameIntervalMs_ = intervalMs;
                }
            }
        }
        return animationFrameIntervalMs_;
    }

    void discardPostedCallbacks() {
        std::queue<std::function<void()>> callbacks;
        std::queue<std::function<void(double)>> frames;
        {
            std::lock_guard<std::mutex> lock(postedCallbacksMutex_);
            callbacks.swap(postedCallbacks_);
        }
        {
            std::lock_guard<std::mutex> lock(animationCallbacksMutex_);
            frames.swap(animationCallbacks_);
        }
    }

    void runInteractivePaintFrame() {
        if (hwnd_ && interactivePaintTimerActive_) {
            KillTimer(hwnd_, kOneUiInteractivePaintTimer);
            interactivePaintTimerActive_ = false;
        }
        flushPendingPaint();
    }

    // Brings a sleeping content wake forward to the next display refresh.
    void expediteDeferredAnimationFrame() {
        if (!animationFramePending_ || deferredAnimationFrameMs_ <= 0.0) {
            return;
        }
        deferredAnimationFrameMs_ = 0.0;
        if (!armAnimationFrameTimer() && hwnd_) {
            animationTimerActive_ = SetTimer(hwnd_, kOneUiAnimationTimer, kOneUiAnimationFrameIntervalMs, nullptr) != 0;
        }
    }

    void scheduleContentAnimationFrame() {
        if (contentAnimationFramePending_) {
            expediteDeferredAnimationFrame();
            return;
        }
        contentAnimationFramePending_ = true;
        requestAnimationFrame([this](double nowMs) { runContentAnimationFrame(nowMs); });
    }

    // Ticks content again at `dueMs` rather than the next refresh, for widgets
    // that asked to sleep (internal::requestAnimationWakeAt). Any frame request
    // made meanwhile brings the frame forward.
    void scheduleContentAnimationWake(double dueMs) {
        if (contentAnimationFramePending_) {
            return;
        }
        contentAnimationFramePending_ = true;
        {
            std::lock_guard<std::mutex> lock(animationCallbacksMutex_);
            animationCallbacks_.push([this](double nowMs) { runContentAnimationFrame(nowMs); });
        }
        if (animationFramePending_) {
            return;
        }
        animationFramePending_ = true;
        deferredAnimationFrameMs_ = dueMs;
        if (armAnimationFrameTimer(dueMs)) {
            return;
        }
        const double delayMs = std::max(0.0, dueMs - currentTimeMs());
        const UINT timerMs = std::max<UINT>(kOneUiAnimationFrameIntervalMs, static_cast<UINT>(std::ceil(delayMs)));
        if (SetTimer(hwnd_, kOneUiAnimationTimer, timerMs, nullptr)) {
            animationTimerActive_ = true;
        } else {
            animationFramePending_ = false;
            deferredAnimationFrameMs_ = 0.0;
        }
    }

    void runContentAnimationFrame(double nowMs) {
        contentAnimationFramePending_ = false;
        if (!content_ || !content_->visible()) {
            return;
        }

        const double workspaceUpdateStarted = internal::workspaceFrameProbe ? currentTimeMs() : 0;
        internal::AnimationWakeScope wake;
        const bool stillAnimating = content_->tickAnimations(nowMs);
        if (workspaceUpdateStarted) internal::workspaceUpdateMs += currentTimeMs() - workspaceUpdateStarted;
        if (stillAnimating) {
            scheduleContentAnimationFrame();
        } else if (wake.dueMs() > 0.0) {
            scheduleContentAnimationWake(wake.dueMs());
        }
    }

    RECT invalidationRectFor(Rect rect) const {
        RECT clientRect{};
        if (!hwnd_ || !GetClientRect(hwnd_, &clientRect)) {
            return RECT{0, 0, 0, 0};
        }

        const float scale = normalizedDpiScale();
        const int margin = logicalToPhysicalCeil(48.0f);
        const int left = static_cast<int>(std::floor(rect.x * scale)) - margin;
        const int top = static_cast<int>(std::floor(rect.y * scale)) - margin;
        const int right = static_cast<int>(std::ceil((rect.x + rect.width) * scale)) + margin;
        const int bottom = static_cast<int>(std::ceil((rect.y + rect.height) * scale)) + margin;
        const int clientLeft = static_cast<int>(clientRect.left);
        const int clientTop = static_cast<int>(clientRect.top);
        const int clientRight = static_cast<int>(clientRect.right);
        const int clientBottom = static_cast<int>(clientRect.bottom);
        return RECT{
            std::clamp(left, clientLeft, clientRight),
            std::clamp(top, clientTop, clientBottom),
            std::clamp(right, clientLeft, clientRight),
            std::clamp(bottom, clientTop, clientBottom)};
    }

    void requestRedrawRect(Rect rect) {
        if (internal::activeFrameProfile) ++tracePaintInvalidations_;
        if (!hwnd_ || rect.width <= 0.0f || rect.height <= 0.0f) {
            requestRedraw();
            return;
        }

        const RECT dirty = invalidationRectFor(rect);

        if (dirty.right <= dirty.left || dirty.bottom <= dirty.top) {
            return;
        }

        queueInvalidation(&dirty);
    }

    void requestInteractiveRedraw() {
        if (!hwnd_) {
            return;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        flushInteractivePaint();
    }
