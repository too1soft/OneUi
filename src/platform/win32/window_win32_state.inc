    DWORD windowStyle() const {
        if (options_.fullscreen) {
            return WS_POPUP;
        }
        if (options_.borderless) {
            // 纯 WS_POPUP 没有 DWM 投影，浅色桌面上窗口边缘看不清。加 WS_THICKFRAME
            // 让系统按标准窗口绘制投影；可见的非客户区边框随后由 WM_NCCALCSIZE 抹掉，
            // 视觉仍是无边框。最大化走自定义逻辑(applyBorderlessMaximize)，故不加
            // WS_MAXIMIZEBOX，避免系统 SW_MAXIMIZE 与自定义状态冲突。
            return WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX;
        }

        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        if (options_.resizable) {
            style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        }
        return style;
    }

    DWORD windowExStyle() const {
        DWORD exStyle = WS_EX_APPWINDOW;
        if (options_.topmost) {
            exStyle |= WS_EX_TOPMOST;
        }
        return exStyle;
    }

    LRESULT hitTestBorderlessWindow(LPARAM lParam) const {
        if (!hwnd_ || !options_.borderless || options_.fullscreen) {
            return HTNOWHERE;
        }

        RECT windowRect{};
        if (!GetWindowRect(hwnd_, &windowRect)) {
            return HTNOWHERE;
        }

        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        const int localX = x - windowRect.left;
        const int localY = y - windowRect.top;
        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;
        const int resizeBorder = logicalToPhysicalCeil(6.0f);
        const int titleBarHeight = logicalToPhysicalCeil(titleBarHeightLogical_);
        const int titleButtonReservedWidth = logicalToPhysicalCeil(titleButtonReservedWidthLogical_);

        if (options_.resizable) {
            const bool left = localX >= 0 && localX < resizeBorder;
            const bool right = localX <= width && localX >= width - resizeBorder;
            const bool top = localY >= 0 && localY < resizeBorder;
            const bool bottom = localY <= height && localY >= height - resizeBorder;

            if (top && left) {
                return HTTOPLEFT;
            }
            if (top && right) {
                return HTTOPRIGHT;
            }
            if (bottom && left) {
                return HTBOTTOMLEFT;
            }
            if (bottom && right) {
                return HTBOTTOMRIGHT;
            }
            if (left) {
                return HTLEFT;
            }
            if (right) {
                return HTRIGHT;
            }
            if (top) {
                return HTTOP;
            }
            if (bottom) {
                return HTBOTTOM;
            }
        }

        const bool inTitleBar = localY >= 0 && localY < titleBarHeight;
        // Caption controls may live on either edge. Use the rendered widget's
        // hit geometry rather than assuming every client uses Windows chrome.
        if (inTitleBar && content_) {
            const Point point{localX / dpiScale(), localY / dpiScale()};
            const auto hitsControl = [&](const auto& self, const std::shared_ptr<Widget>& widget) -> bool {
                if (!widget || !widget->visible()) return false;
                if (const auto title = std::dynamic_pointer_cast<WindowTitleBar>(widget))
                    return title->hitsWindowButton(point);
                if (const auto view = std::dynamic_pointer_cast<View>(widget))
                    for (const auto& child : view->children()) if (self(self, child)) return true;
                return false;
            };
            if (hitsControl(hitsControl, content_)) return HTNOWHERE;
        }
        const bool hasInteractiveTitleBarRegion =
            titleBarInteractiveLeadingWidthLogical_ >= 0.0f
            && titleBarInteractiveTrailingWidthLogical_ >= 0.0f;
        if (inTitleBar && hasInteractiveTitleBarRegion) {
            const int interactiveLeft = logicalToPhysicalCeil(titleBarInteractiveLeadingWidthLogical_);
            const int interactiveRight =
                width - logicalToPhysicalCeil(titleBarInteractiveTrailingWidthLogical_);
            if (localX >= interactiveLeft && localX < interactiveRight) {
                return HTNOWHERE;
            }
        }
        const bool overWindowButtons = localX >= width - titleButtonReservedWidth;
        if (inTitleBar && !overWindowButtons) {
            return HTCAPTION;
        }

        return HTNOWHERE;
    }

    bool restoreBorderlessForCaptionDrag(int screenX, int screenY) {
        if (!hwnd_) {
            return false;
        }

        RECT maximizedRect{};
        if (!GetWindowRect(hwnd_, &maximizedRect)) {
            return false;
        }
        RECT restoreRect = savedBorderlessRect_;
        if (restoreRect.right <= restoreRect.left || restoreRect.bottom <= restoreRect.top) {
            if (!hasNormalPlacement_ || lastNormalRect_.right <= lastNormalRect_.left
                || lastNormalRect_.bottom <= lastNormalRect_.top) {
                return false;
            }
            restoreRect = lastNormalRect_;
        }

        POINT pointer{screenX, screenY};
        const HMONITOR monitor = MonitorFromPoint(pointer, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo)) {
            return false;
        }

        const RECT& work = monitorInfo.rcWork;
        const int workWidth = std::max(1L, work.right - work.left);
        const int workHeight = std::max(1L, work.bottom - work.top);
        constexpr int kWorkAreaMatchTolerance = 2;
        const bool fillsWorkArea =
            std::abs(maximizedRect.left - work.left) <= kWorkAreaMatchTolerance
            && std::abs(maximizedRect.top - work.top) <= kWorkAreaMatchTolerance
            && std::abs(maximizedRect.right - work.right) <= kWorkAreaMatchTolerance
            && std::abs(maximizedRect.bottom - work.bottom) <= kWorkAreaMatchTolerance;
        const bool wasBorderlessMaximized = borderlessMaximized_;
        if (!wasBorderlessMaximized && !fillsWorkArea) {
            return false;
        }

        int restoreWidth = std::clamp(
            static_cast<int>(restoreRect.right - restoreRect.left),
            1,
            workWidth);
        int restoreHeight = std::clamp(
            static_cast<int>(restoreRect.bottom - restoreRect.top),
            1,
            workHeight);
        // A normal placement persisted at the exact work-area dimensions is
        // not a useful restore target.  Match the familiar restore-from-
        // maximized gesture while retaining enough room for dense workspaces.
        if (restoreWidth >= workWidth - kWorkAreaMatchTolerance
            && restoreHeight >= workHeight - kWorkAreaMatchTolerance) {
            restoreWidth = std::max(1, static_cast<int>(std::lround(workWidth * 0.78)));
            restoreHeight = std::max(1, static_cast<int>(std::lround(workHeight * 0.80)));
        }
        const int maximizedWidth = std::max(
            1,
            static_cast<int>(maximizedRect.right - maximizedRect.left));
        const float horizontalRatio = std::clamp(
            static_cast<float>(screenX - static_cast<int>(maximizedRect.left))
                / static_cast<float>(maximizedWidth),
            0.0f,
            1.0f);
        const int grabOffsetY = std::clamp(
            screenY - static_cast<int>(maximizedRect.top),
            0,
            std::max(0, logicalToPhysicalCeil(titleBarHeightLogical_) - 1));
        const int targetLeft = std::clamp(
            screenX - static_cast<int>(std::lround(horizontalRatio * restoreWidth)),
            static_cast<int>(work.left),
            static_cast<int>(work.right) - restoreWidth);
        const int targetTop = std::clamp(
            screenY - grabOffsetY,
            static_cast<int>(work.top),
            static_cast<int>(work.bottom) - restoreHeight);

        beginWindowStateChange();
        borderlessMaximized_ = false;
        if (!SetWindowPos(
                hwnd_,
                options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                targetLeft,
                targetTop,
                restoreWidth,
                restoreHeight,
                SWP_NOOWNERZORDER | SWP_NOACTIVATE)) {
            borderlessMaximized_ = wasBorderlessMaximized;
            applyingWindowState_ = false;
            windowStatePaintPending_ = false;
            return false;
        }
        lastNormalRect_ = RECT{
            targetLeft,
            targetTop,
            targetLeft + restoreWidth,
            targetTop + restoreHeight};
        hasNormalPlacement_ = true;
        finishWindowStateChange();
        return true;
    }

    void applyWindowState() {
        if (!hwnd_) {
            return;
        }

        beginWindowStateChange();
        if (options_.borderless) {
            if (options_.fullscreen) {
                if (!fullscreenApplied_) {
                    GetWindowRect(hwnd_, &savedFullscreenRect_);
                    fullscreenApplied_ = true;
                }

                HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
                MONITORINFO monitorInfo{};
                monitorInfo.cbSize = sizeof(monitorInfo);
                GetMonitorInfoW(monitor, &monitorInfo);
                const RECT& bounds = monitorInfo.rcMonitor;
                SetWindowPos(
                    hwnd_,
                    options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                    bounds.left,
                    bounds.top,
                    bounds.right - bounds.left,
                    bounds.bottom - bounds.top,
                    SWP_NOOWNERZORDER | SWP_NOACTIVATE);
                finishWindowStateChange();
                return;
            }

            if (fullscreenApplied_) {
                fullscreenApplied_ = false;
                SetWindowPos(
                    hwnd_,
                    options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                    savedFullscreenRect_.left,
                    savedFullscreenRect_.top,
                    savedFullscreenRect_.right - savedFullscreenRect_.left,
                    savedFullscreenRect_.bottom - savedFullscreenRect_.top,
                    SWP_NOOWNERZORDER | SWP_NOACTIVATE);
                finishWindowStateChange();
                return;
            }

            finishWindowStateChange();
            return;
        }

        if (options_.fullscreen) {
            if (!fullscreenApplied_) {
                savedStyle_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_STYLE));
                savedExStyle_ = static_cast<DWORD>(GetWindowLongPtrW(hwnd_, GWL_EXSTYLE));
                savedPlacement_.length = sizeof(savedPlacement_);
                GetWindowPlacement(hwnd_, &savedPlacement_);
                fullscreenApplied_ = true;
            }

            HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(monitorInfo);
            GetMonitorInfoW(monitor, &monitorInfo);

            SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP);
            SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, windowExStyle());
            SetWindowPos(
                hwnd_,
                options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOOWNERZORDER);
            finishWindowStateChange();
            return;
        }

        const DWORD style = fullscreenApplied_ ? savedStyle_ : windowStyle();
        const DWORD exStyle = fullscreenApplied_ ? savedExStyle_ : windowExStyle();
        SetWindowLongPtrW(hwnd_, GWL_STYLE, style);
        SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle);

        if (fullscreenApplied_) {
            SetWindowPlacement(hwnd_, &savedPlacement_);
            fullscreenApplied_ = false;
        }

        SetWindowPos(
            hwnd_,
            options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        finishWindowStateChange();
    }

    void beginWindowStateChange() {
        applyingWindowState_ = true;
        windowStatePaintPending_ = true;
    }

    void applyBorderlessMaximize(bool enabled) {
        if (!hwnd_ || borderlessMaximized_ == enabled) {
            return;
        }

        beginWindowStateChange();
        if (enabled) {
            GetWindowRect(hwnd_, &savedBorderlessRect_);
            HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(monitorInfo);
            GetMonitorInfoW(monitor, &monitorInfo);
            const RECT& work = monitorInfo.rcWork;
            borderlessMaximized_ = true;
            SetWindowPos(
                hwnd_,
                options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                work.left,
                work.top,
                work.right - work.left,
                work.bottom - work.top,
                SWP_NOOWNERZORDER | SWP_NOACTIVATE);
            finishWindowStateChange();
            return;
        }

        borderlessMaximized_ = false;
        SetWindowPos(
            hwnd_,
            options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
            savedBorderlessRect_.left,
            savedBorderlessRect_.top,
            savedBorderlessRect_.right - savedBorderlessRect_.left,
            savedBorderlessRect_.bottom - savedBorderlessRect_.top,
            SWP_NOOWNERZORDER | SWP_NOACTIVATE);
        finishWindowStateChange();
    }

    void finishWindowStateChange() {
        if (!hwnd_) {
            applyingWindowState_ = false;
            windowStatePaintPending_ = false;
            return;
        }
        PostMessageW(hwnd_, kOneUiFinishWindowStatePaint, 0, 0);
    }

    void flushWindowStatePaint() {
        if (!hwnd_) {
            applyingWindowState_ = false;
            windowStatePaintPending_ = false;
            return;
        }

        applyingWindowState_ = false;
        if (!windowStatePaintPending_) {
            return;
        }

        windowStatePaintPending_ = false;
        InvalidateRect(hwnd_, nullptr, FALSE);
        flushInteractivePaint();
        printRenderTrace(true);
    }

    void requestDeferredFullPaint() {
        if (!hwnd_) {
            return;
        }

        InvalidateRect(hwnd_, nullptr, FALSE);
        scheduleInteractivePaint();
    }

    void flushDeferredFullPaint() {
        if (!hwnd_) {
            deferredFullPaintPending_ = false;
            return;
        }

        deferredFullPaintPending_ = false;
        flushPendingPaint();
    }

    void handleDpiChanged(WPARAM wParam, LPARAM lParam) {
        const UINT dpiX = LOWORD(wParam);
        const float nextScale = scaleFromDpi(dpiX != 0 ? dpiX : static_cast<UINT>(std::round(dpiScale_ * kDefaultDpi)));
        if (nextScale <= 0.0f) {
            return;
        }

        const bool changed = std::abs(nextScale - dpiScale_) > 0.001f;
        dpiScale_ = nextScale;
        if (content_) content_->setTextEnvironment(defaultFontFamily_, dpiScale());
        if (changed) {
            paintSurface_.reset();
            paintSurfaceWidth_ = 0;
            paintSurfaceHeight_ = 0;
        }

        if (lParam && !options_.fullscreen && !borderlessMaximized_) {
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(
                hwnd_,
                nullptr,
                suggested->left,
                suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }

        requestDeferredFullPaint();
    }
