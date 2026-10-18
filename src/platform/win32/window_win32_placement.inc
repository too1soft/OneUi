    void setTitle(std::wstring title) override {
        options_.title = std::move(title);
        if (hwnd_) {
            SetWindowTextW(hwnd_, options_.title.c_str());
        }
    }

    void setFullscreen(bool enabled) override {
        if (options_.fullscreen == enabled) {
            return;
        }

        ensureCreated();
        options_.fullscreen = enabled;
        applyWindowState();
    }

    bool isFullscreen() const override {
        return options_.fullscreen;
    }

    void setBorderless(bool enabled) override {
        if (options_.borderless == enabled) {
            return;
        }

        ensureCreated();
        options_.borderless = enabled;
        if (!options_.fullscreen) {
            applyWindowState();
        }
        applyBorderlessShadow();
    }

    void setTitleBarDragMetrics(float titleBarHeight, float reservedButtonWidth) override {
        if (titleBarHeight > 0.0f) {
            titleBarHeightLogical_ = titleBarHeight;
        }
        if (reservedButtonWidth >= 0.0f) {
            titleButtonReservedWidthLogical_ = reservedButtonWidth;
        }
    }

    void setTitleBarInteractiveInsets(float leadingWidth, float trailingWidth) override {
        if (leadingWidth < 0.0f || trailingWidth < 0.0f) {
            titleBarInteractiveLeadingWidthLogical_ = -1.0f;
            titleBarInteractiveTrailingWidthLogical_ = -1.0f;
            return;
        }
        titleBarInteractiveLeadingWidthLogical_ = leadingWidth;
        titleBarInteractiveTrailingWidthLogical_ = trailingWidth;
    }

    void toggleMaximize() override {
        ensureCreated();
        if (!hwnd_ || options_.fullscreen) {
            return;
        }

        if (!options_.borderless) {
            ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
            return;
        }

        applyBorderlessMaximize(!borderlessMaximized_);
    }

    bool getWindowPlacement(WindowPlacement& placement) const override {
        if (!hasNormalPlacement_) {
            return false;
        }
        placement.x = lastNormalRect_.left;
        placement.y = lastNormalRect_.top;
        placement.width = lastNormalRect_.right - lastNormalRect_.left;
        placement.height = lastNormalRect_.bottom - lastNormalRect_.top;
        placement.maximized = placementStatePending_ ? pendingMaximized_ : lastKnownMaximized_;
        return placement.width > 0 && placement.height > 0;
    }

    bool setWindowPlacement(const WindowPlacement& placement) override {
        if (placement.width <= 0 || placement.height <= 0) {
            return false;
        }
        ensureCreated();
        if (!hwnd_) {
            return false;
        }

        const auto toLong = [](std::int64_t value) {
            return static_cast<LONG>(std::clamp<std::int64_t>(value, LONG_MIN, LONG_MAX));
        };
        RECT requested{
            toLong(placement.x),
            toLong(placement.y),
            toLong(static_cast<std::int64_t>(placement.x) + placement.width),
            toLong(static_cast<std::int64_t>(placement.y) + placement.height)};
        const HMONITOR monitor = MonitorFromRect(&requested, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo)) {
            return false;
        }

        const RECT& work = monitorInfo.rcWork;
        const int workWidth = static_cast<int>(std::max(1L, work.right - work.left));
        const int workHeight = static_cast<int>(std::max(1L, work.bottom - work.top));
        const int width = std::clamp(placement.width, 1, workWidth);
        const int height = std::clamp(placement.height, 1, workHeight);
        const int x = std::clamp(
            placement.x,
            static_cast<int>(work.left),
            static_cast<int>(work.right) - width);
        const int y = std::clamp(
            placement.y,
            static_cast<int>(work.top),
            static_cast<int>(work.bottom) - height);

        const bool visible = IsWindowVisible(hwnd_) != FALSE;
        if (borderlessMaximized_) {
            applyBorderlessMaximize(false);
        } else if (visible && IsZoomed(hwnd_)) {
            ShowWindow(hwnd_, SW_RESTORE);
        }

        if (!SetWindowPos(
            hwnd_,
            options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
            x,
            y,
            width,
            height,
            SWP_NOOWNERZORDER | SWP_NOACTIVATE)) {
            return false;
        }
        lastNormalRect_ = RECT{x, y, x + width, y + height};
        hasNormalPlacement_ = true;
        placementStatePending_ = !visible;
        pendingMaximized_ = placement.maximized;
        lastKnownMaximized_ = placement.maximized;

        if (visible && placement.maximized) {
            if (options_.borderless) {
                applyBorderlessMaximize(true);
            } else {
                ShowWindow(hwnd_, SW_MAXIMIZE);
            }
        }
        return true;
    }

private:
    void showWithPlacementState(bool restoreIconic) {
        if (!placementStatePending_) {
            ShowWindow(hwnd_, restoreIconic && IsIconic(hwnd_) ? SW_RESTORE : SW_SHOW);
            return;
        }

        if (pendingMaximized_) {
            if (options_.borderless) {
                applyBorderlessMaximize(true);
                ShowWindow(hwnd_, SW_SHOW);
            } else {
                ShowWindow(hwnd_, SW_MAXIMIZE);
            }
        } else {
            if (borderlessMaximized_) {
                applyBorderlessMaximize(false);
            }
            ShowWindow(hwnd_, !options_.borderless && IsZoomed(hwnd_) ? SW_RESTORE : SW_SHOW);
        }
        placementStatePending_ = false;
        pendingMaximized_ = false;
    }

    void captureNormalPlacement() {
        if (!hwnd_ || options_.fullscreen || borderlessMaximized_ || IsIconic(hwnd_) || IsZoomed(hwnd_)) {
            return;
        }
        RECT rect{};
        if (!GetWindowRect(hwnd_, &rect) || rect.right <= rect.left || rect.bottom <= rect.top) {
            return;
        }
        lastNormalRect_ = rect;
        hasNormalPlacement_ = true;
    }

    // GetSystemMetricsForDpi 在 Win10 运行时存在，但 mingw-w64 头文件未声明，动态解析；
    // Win7 等旧系统没有该接口，退回 GetSystemMetrics（系统 DPI 值）。
