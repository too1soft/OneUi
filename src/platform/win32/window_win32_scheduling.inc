    static constexpr auto postedCallbackBudget_ = std::chrono::milliseconds(4);

    bool postedPaintDue(bool timeBudgetExhausted) {
        if (inputPaintPending_) return false;
        if (interactiveResizeActive_ && lastLiveResizePaintMs_ != 0.0) {
            // A callback flood can paint once the refresh and native timer interval elapse.
            const double interval = std::max<double>(USER_TIMER_MINIMUM, animationFrameIntervalMs());
            return currentTimeMs() - lastLiveResizePaintMs_ >= interval;
        }
        if (gpuAvailable_ || glWorker_.ready() ||
            lastContentPaintCompletedMs_ == 0.0) return true;
        const bool expensivePaint = lastContentPaintDurationMs_ >= postedCallbackBudget_.count();
        if (!expensivePaint && !timeBudgetExhausted) return true;
        const double interval = std::max<double>(USER_TIMER_MINIMUM, animationFrameIntervalMs());
        const double age = currentTimeMs() - lastContentPaintCompletedMs_;
        // A short callback can force a second software frame while input is
        // already queued. Keep the update region for WM_PAINT so Win32 can
        // dispatch the waiting messages first. Bound the yield during bursts.
        if (expensivePaint && age < interval &&
            HIWORD(GetQueueStatus(QS_INPUT | QS_POSTMESSAGE)) != 0) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "posted_software_paint_queue_yield", reinterpret_cast<std::uintptr_t>(hwnd_)});
            return false;
        }
        if (!timeBudgetExhausted) return true;
        static const bool yieldSoftwarePaint = [] {
            wchar_t value[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_YIELD_POSTED_SOFTWARE_PAINT", value, 2) == 1 &&
                value[0] == L'1';
        }();
        if (!yieldSoftwarePaint) return true;
        // Let the next native message hold its turn after an expensive callback.
        // A busy posted queue may force a paint once the previous frame is old.
        if (age >= 2.0 * interval) return true;
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "win32", "posted_software_paint_yield", reinterpret_cast<std::uintptr_t>(hwnd_)});
        return false;
    }

    void runPostedCallbacks() {
        const auto alive = callbackWindowAlive_;
        std::queue<std::function<void()>> callbacks;
        {
            std::lock_guard<std::mutex> lock(postedCallbacksMutex_);
            postedCallbackMessagePending_ = false;
            if (!deferredPostedCallbackBatches_.empty()) {
                callbacks.swap(deferredPostedCallbackBatches_.front());
                deferredPostedCallbackBatches_.pop_front();
            } else callbacks.swap(postedCallbacks_);
        }
        // Reveal keyboard edits before potentially expensive model work.
        // Pointer commands can replace the invalidated content entirely and
        // should run before drawing its hover or release state.
        if (!callbacks.empty() && std::exchange(inputPaintBeforePostedWork_, false)) {
            flushPendingPaint();
            if (!alive->load(std::memory_order_acquire)) return;
        }
        const auto started = std::chrono::steady_clock::now();
        unsigned int dispatched = 0;
        bool timeBudgetExhausted = false;
        // A posted message is part of the native loop's budget, not permission
        // to drain an arbitrary background batch. Keep this batch local while
        // invoking callbacks: a nested native loop may run newly posted work,
        // but must not reenter older callbacks held by its caller's batch.
        while (!callbacks.empty() && dispatched < 32) {
            if (!alive->load(std::memory_order_acquire)) return;
            auto callback = std::move(callbacks.front());
            callbacks.pop();
            callback();
            ++dispatched;
            timeBudgetExhausted = std::chrono::steady_clock::now() - started >= postedCallbackBudget_;
            if (timeBudgetExhausted) break;
        }
        if (dispatched) win32::requestInputTurnAfterPostedWork();
        if (!alive->load(std::memory_order_acquire)) return;
        if (dispatched && internal::scrollTraceEnabled()) {
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            if (elapsed >= 2.0) {
                internal::writeScrollTrace(internal::ScrollTraceEvent{
                    "win32", "posted_work", reinterpret_cast<std::uintptr_t>(hwnd_), 0, 0, 0, 0, 0, 0,
                    elapsed, static_cast<double>(dispatched), 0});
            }
        }
        // WM_PAINT is lower priority than posted messages. A continuously
        // refilled queue must still reveal static content without an rAF.
        if (postedPaintDue(timeBudgetExhausted)) flushPendingPaint();
        if (!alive->load(std::memory_order_acquire)) return;
        bool posted = true;
        {
            std::lock_guard<std::mutex> lock(postedCallbacksMutex_);
            // Move the unprocessed batch in O(1), ahead of newer batches. Do
            // not copy/requeue every remaining callback on each 4 ms yield.
            if (!callbacks.empty()) deferredPostedCallbackBatches_.push_front(std::move(callbacks));
            if ((!deferredPostedCallbackBatches_.empty() || !postedCallbacks_.empty()) && !postedCallbackMessagePending_) {
                postedCallbackMessagePending_ = true;
                posted = PostMessageW(hwnd_, kOneUiRunPostedCallbacks, 0, 0) != FALSE;
            }
        }
        if (!posted) {
            acceptingPostedCallbacks_.store(false, std::memory_order_release);
            discardPostedCallbacks();
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
        std::deque<std::queue<std::function<void()>>> deferred;
        std::queue<std::function<void(double)>> frames;
        {
            std::lock_guard<std::mutex> lock(postedCallbacksMutex_);
            callbacks.swap(postedCallbacks_);
            deferred.swap(deferredPostedCallbackBatches_);
            postedCallbackMessagePending_ = false;
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

    enum class InputPaint { Coalesced, BeforePostedWork };

    void requestEditRedraw(const internal::EditPresentationScope& edit) {
        if (edit.hasPostedWork()) {
            // A ready frame can run when the message batch yields, before
            // the posted model update. Release painting at the end of the
            // work queued by this edit, keeping the newest edit in effect.
            const auto version = ++coalescedEditPaintVersion_;
            coalescedEditPaintPending_ = post([this, version] {
                if (version == coalescedEditPaintVersion_) coalescedEditPaintPending_ = false;
            });
        }
        requestInteractiveRedraw(edit.coalesced() ? InputPaint::Coalesced : InputPaint::BeforePostedWork);
    }

    void requestInteractiveRedraw(InputPaint policy = InputPaint::Coalesced) {
        if (!hwnd_) {
            return;
        }
        // Widgets normally supplied a precise invalidation already. Preserve
        // it instead of expanding every handled click to the entire client.
        if (!invalidationPending_) queueInvalidation(nullptr);
        if (policy == InputPaint::BeforePostedWork) inputPaintBeforePostedWork_ = true;
        scheduleInputPaint();
    }

    void scheduleInputPaint() {
        if (!hwnd_ || inputPaintPending_) return;
        inputPaintPending_ = true;
        requestAnimationFrame([this](double) {
            inputPaintPending_ = false;
            inputPaintBeforePostedWork_ = false;
        });
        if (!animationFramePending_) {
            // Timer creation failure must not leave invalidation held forever.
            inputPaintPending_ = false;
            inputPaintBeforePostedWork_ = false;
            submitInvalidations();
        }
    }
