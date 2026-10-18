    WindowBackend backend() const override { return WindowBackend::Win32; }
    unsigned int capabilities() const override {
        return WindowCapabilityClipboard | WindowCapabilityIme | WindowCapabilityPlacement |
            WindowCapabilityActivation | WindowCapabilityTopmost | WindowCapabilityTray |
            WindowCapabilityNativeDialogs | WindowCapabilityFileDrop | WindowCapabilityMenuBar | WindowCapabilityTrayTooltip;
    }
    explicit Win32Window(WindowOptions options)
        : options_(std::move(options))
        , dpiScale_(dpiScaleForWindowHandle(nullptr))
        , renderTraceEnabled_(runtimeConfig_.renderTrace)
        , renderTraceFilePath_(runtimeConfig_.renderTraceFile)
        , taskbarCreatedMessage_(RegisterWindowMessageW(L"TaskbarCreated")) {
#if ONEUI_DEVELOPER_TOOLS
        if(runtimeConfig_.developerTools) {
            // ONEUI_DEVTOOLS_OVERLAY=0 records without drawing the panel, so
            // frame measurements exclude the overlay's own text and fallback.
            DeveloperOptions options;
            const wchar_t* overlay=_wgetenv(L"ONEUI_DEVTOOLS_OVERLAY");
            options.overlay=!(overlay && overlay[0]==L'0');
            developerTools_=std::make_shared<internal::DeveloperRecorder>(options);
        }
#endif
        accessibilityComResult_ = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        animationFrameTimer_ = CreateWaitableTimerExW(
            nullptr,
            nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_ALL_ACCESS);
        if (!animationFrameTimer_) {
            animationFrameTimer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
        }
        if (animationFrameTimer_) {
            win32::registerFrameSource({animationFrameTimer_, callbackWindowAlive_,
                [this] { runAnimationFrameCallbacks(); }});
        }
    }

    ~Win32Window() override {
        callbackWindowAlive_->store(false, std::memory_order_release);
        if(accessibilityProvider_){accessibilityProvider_->Release();accessibilityProvider_=nullptr;}
        acceptingPostedCallbacks_.store(false, std::memory_order_release);
        discardPostedCallbacks();
        if (content_) {
            content_->detachFromOwner(this);
        }
        hideInternalTrayIcon();
        if (trayCustomIcon_) DestroyIcon(trayCustomIcon_);
        if (hwnd_) {
            DestroyWindow(hwnd_);
        }
        if (animationFrameTimer_) {
            CancelWaitableTimer(animationFrameTimer_);
            CloseHandle(animationFrameTimer_);
            animationFrameTimer_ = nullptr;
        }
        if (SUCCEEDED(accessibilityComResult_)) CoUninitialize();
    }

    void syncTitleBarWindowState() {
        const auto sync = [&](const auto& self, const std::shared_ptr<Widget>& widget) -> void {
            if (!widget) return;
            if (const auto title = std::dynamic_pointer_cast<WindowTitleBar>(widget))
                title->setMaximized(lastKnownMaximized_);
            if (const auto view = std::dynamic_pointer_cast<View>(widget))
                for (const auto& child : view->children()) self(self, child);
        };
        sync(sync, content_);
    }

    RendererInfo rendererInfo() const override { return rendererInfo_; }
    bool setDeveloperTools(DeveloperOptions options) override {
#if ONEUI_DEVELOPER_TOOLS
        if(options.enabled)developerTools_=std::make_shared<internal::DeveloperRecorder>(options);
        else developerTools_.reset();
        requestRedraw();return true;
#else
        (void)options;return false;
#endif
    }
    DeveloperSnapshot developerSnapshot() const override {
#if ONEUI_DEVELOPER_TOOLS
        DeveloperSnapshot result=developerTools_?developerTools_->snapshot():DeveloperSnapshot{};
        result.supported=true;
#else
        DeveloperSnapshot result;
#endif
        result.backend=rendererInfo_.backend==RenderBackend::OpenGL?"opengl":rendererInfo_.backend==RenderBackend::Software?"software":"unknown";
        return result;
    }
    bool exportDeveloperReport(const std::wstring& path) const override {
#if ONEUI_DEVELOPER_TOOLS
        try {
            std::ofstream file(std::filesystem::path(path),std::ios::binary);
            if(!file)return false;
            file << developerReportJson(developerSnapshot());file.flush();return bool(file);
        } catch(...) { return false; }
#else
        (void)path;return false;
#endif
    }

    RendererMemoryInfo rendererMemoryInfo() const override {
        RendererMemoryInfo info;
        info.cpuCachesAvailable = true;
        info.cpuFontCacheBytes = SkGraphics::GetFontCacheUsed();
        info.cpuResourceCacheBytes = SkGraphics::GetResourceCacheTotalBytesUsed();
        if (grContext_ && gpuAvailable_) {
            int count = 0;
            size_t bytes = 0;
            grContext_->getResourceCacheUsage(&count, &bytes);
            info.gpuCacheAvailable = true;
            info.gpuCacheBytes = bytes;
            info.gpuResourceCount = static_cast<std::uint64_t>(std::max(0, count));
            info.gpuPurgeableBytes = grContext_->getResourceCachePurgeableBytes();
            info.gpuCacheLimitBytes = grContext_->getResourceCacheLimit();
        }
        if (paintSurface_) {
            info.retainedSurfaceBytes = static_cast<std::uint64_t>(paintSurfaceWidth_)
                * static_cast<std::uint64_t>(paintSurfaceHeight_) * 4;
        }
        return info;
    }

    void setContent(std::shared_ptr<Widget> widget) override {
        applicationContent_ = widget;
        if (menuHost_) { menuHost_->setApplicationContent(std::move(widget)); widget = menuHost_; }
        if (content_) {
            content_->detachFromOwner(this);
        }
        content_ = std::move(widget);
        setCommandRoot(content_);
        syncTitleBarWindowState();
        if (content_) {
            content_->setTextEnvironment(defaultFontFamily_, dpiScale());
            content_->attachToOwner(
                this,
                [this] { requestRedraw(); },
                [this](Rect rect) { requestRedrawRect(rect); },
                [this] { scheduleContentAnimationFrame(); });
            // Children may start style transitions before the root has a
            // window scheduler. Resume those otherwise lost frame requests.
            scheduleContentAnimationFrame();
        }
        requestRedraw();
    }

    void setContentScale(float scale) override {
        if (!std::isfinite(scale)) return;
        scale = std::clamp(scale, 1.0f, 2.25f);
        if (std::abs(scale - contentScale_) < 0.001f) return;
        // Create at the physical monitor DPI; changing text preferences must
        // not resize the user's window or multiply the native minimum bounds.
        ensureCreated();
        contentScale_ = scale;
        if (content_) content_->setTextEnvironment(defaultFontFamily_, dpiScale());
        scheduleClientSizeChanged();
        requestRedraw();
    }

    bool requestFocus(Widget* widget, bool focusVisible) override {
        if (!content_ || !widget) {
            return false;
        }
        bool focused = false;
        if (content_.get() == widget) {
            focused = widget->isFocusable();
            if (focused) {
                widget->onFocusChanged(true);
                widget->setFocusVisible(focusVisible);
            }
        } else if (auto* root = dynamic_cast<View*>(content_.get())) {
            focused = root->requestFocus(widget, focusVisible);
        }
        if (!focused) {
            return false;
        }
        ensureCreated();
        if (hwnd_) {
            if (GetFocus() != hwnd_) {
                SetFocus(hwnd_);
            }
            // The focused widget's caret is only known after the next layout,
            // and the IME round trip must not delay the frame that shows the
            // focus change. Paint positions the IME once it has laid out.
            imePositionAfterPaint_ = true;
        }
        requestRedraw();
        return true;
    }

    bool prepareContentTransfer(Widget* subtree) override {
        if (!subtree || closed()) return false;
        if (subtree->focused()) cancelImeComposition();
        if (hwnd_ && GetCapture() == hwnd_) ReleaseCapture();
        return Window::prepareContentTransfer(subtree);
    }

    void setRawKeyHandler(RawKeyHandler handler) override {
        rawKeyHandler_ = std::move(handler);
    }

    void setClientSizeChangedHandler(ClientSizeChangedHandler handler) override {
        clientSizeChangedHandler_ = std::move(handler);
        scheduleClientSizeChanged();
    }

    void setActivationChangedHandler(ActivationChangedHandler handler) override {
        activationChangedHandler_ = std::move(handler);
    }

    void setFileDropHandler(FileDropHandler handler) override {
        fileDropHandler_ = std::move(handler);
        if (hwnd_) {
            DragAcceptFiles(hwnd_, fileDropHandler_ ? TRUE : FALSE);
        }
    }

    WindowState state() const override {
        auto result=Window::state();if(!hwnd_)return result;
        result.knownFields=127;
        result.visible=IsWindowVisible(hwnd_)!=FALSE;
        result.active=GetForegroundWindow()==hwnd_;
        result.minimized=IsIconic(hwnd_)!=FALSE;
        result.maximized=borderlessMaximized_||IsZoomed(hwnd_)!=FALSE;
        result.fullscreen=options_.fullscreen;
        result.topmost=(GetWindowLongPtrW(hwnd_,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
        result.resizable=options_.resizable;return result;
    }
    bool hide() override { if(!hwnd_)return false;ShowWindow(hwnd_,SW_HIDE);return true; }
    bool restore() override { ensureCreated();if(!hwnd_)return false;if(options_.fullscreen)setFullscreen(false);if(borderlessMaximized_)applyBorderlessMaximize(false);ShowWindow(hwnd_,SW_RESTORE);return true; }
    bool maximize() override { ensureCreated();if(!hwnd_)return false;if(options_.borderless)applyBorderlessMaximize(true);else ShowWindow(hwnd_,SW_MAXIMIZE);return true; }
    bool setTopmost(bool enabled) override { ensureCreated();if(!hwnd_)return false;auto ok=SetWindowPos(hwnd_,enabled?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);if(ok)options_.topmost=enabled;return ok!=FALSE; }
    bool setResizable(bool enabled) override { if(closed())return false;options_.resizable=enabled;if(hwnd_)applyWindowState();queueStateChange();return true; }
    bool setClientSize(Size size) override {
        if(!std::isfinite(size.width)||!std::isfinite(size.height)||size.width<=0||size.height<=0||size.width>32767||size.height>32767)return false;
        ensureCreated();if(!hwnd_)return false;RECT frame{},client{};GetWindowRect(hwnd_,&frame);GetClientRect(hwnd_,&client);
        return SetWindowPos(hwnd_,nullptr,0,0,LONG(std::ceil(size.width*dpiScale()))+frame.right-frame.left-client.right,LONG(std::ceil(size.height*dpiScale()))+frame.bottom-frame.top-client.bottom,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE;
    }
    bool setMaximumClientSize(Size size) override { if(!std::isfinite(size.width)||!std::isfinite(size.height))return false;maximumClientSize_={std::max(0.f,size.width),std::max(0.f,size.height)};return true; }
    bool setOwner(Window* owner) override {
        if(owner==this||(owner&&(owner->backend()!=backend()||owner->closed())))return false;
        ensureCreated();if(!hwnd_)return false;if(owner)owner->initialize();
        SetLastError(0);auto previous=SetWindowLongPtrW(hwnd_,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(owner?owner->nativeHandle():nullptr));return previous!=0||GetLastError()==0;
    }
    bool setInputEnabled(bool enabled) override { if(!hwnd_)return false;EnableWindow(hwnd_,enabled?TRUE:FALSE);return IsWindowEnabled(hwnd_)==BOOL(enabled); }

    void setMinimumClientSize(Size size) override {
        minimumClientSize_.width = std::max(0.0f, size.width);
        minimumClientSize_.height = std::max(0.0f, size.height);
    }

    void setDefaultFontFamily(std::wstring family) override {
        defaultFontFamily_ = std::move(family);
        if (content_) content_->setTextEnvironment(defaultFontFamily_, dpiScale());
        requestRedraw();
    }

    void initialize() override {
        ensureCreated();
    }

    void show() override {
        ensureCreated();
        initGPU();
        showWithPlacementState(false);
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
    }

    bool centerOnActiveMonitor() override {
        ensureCreated();
        if (!hwnd_) {
            return false;
        }

        HMONITOR monitor = nullptr;
        if (const HWND foreground = GetForegroundWindow()) {
            monitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONULL);
        }
        if (!monitor) {
            POINT cursor{};
            if (GetCursorPos(&cursor)) {
                monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONULL);
            }
        }
        if (!monitor) {
            POINT origin{};
            monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
        }

        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        RECT windowRect{};
        if (!monitor || !GetMonitorInfoW(monitor, &monitorInfo) || !GetWindowRect(hwnd_, &windowRect)) {
            return false;
        }
        const RECT& work = monitorInfo.rcWork;
        const int workWidth = std::max(1L, work.right - work.left);
        const int workHeight = std::max(1L, work.bottom - work.top);
        const int width = std::min(
            static_cast<int>(std::max(1L, windowRect.right - windowRect.left)),
            workWidth);
        const int height = std::min(
            static_cast<int>(std::max(1L, windowRect.bottom - windowRect.top)),
            workHeight);
        const int x = static_cast<int>(work.left) + (workWidth - width) / 2;
        const int y = static_cast<int>(work.top) + (workHeight - height) / 2;
        if (!SetWindowPos(
                hwnd_,
                options_.topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                x,
                y,
                width,
                height,
                SWP_NOACTIVATE | SWP_NOOWNERZORDER)) {
            return false;
        }
        lastNormalRect_ = RECT{x, y, x + width, y + height};
        hasNormalPlacement_ = true;
        return true;
    }

    bool clientAreaAnimationsEnabled() const override {
        BOOL enabled = TRUE;
        return !SystemParametersInfoW(
                   SPI_GETCLIENTAREAANIMATION,
                   0,
                   &enabled,
                   0)
            || enabled != FALSE;
    }

    void showWithFade(unsigned int durationMs) override {
        ensureCreated();
        if (!hwnd_) {
            return;
        }
        initGPU();
        if (durationMs == 0 || !clientAreaAnimationsEnabled() || IsWindowVisible(hwnd_)) {
            show();
            return;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        if (!AnimateWindow(hwnd_, durationMs, AW_ACTIVATE | AW_BLEND)) {
            showWithPlacementState(false);
        } else {
            placementStatePending_ = false;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
    }

    void activate() override {
        ensureCreated();
        if (!hwnd_) {
            return;
        }
        initGPU();
        showWithPlacementState(true);
        SetForegroundWindow(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
    }

    int run() override {
        ensureCreated();
        if (!hwnd_) {
            return acceptingPostedCallbacks_.load(std::memory_order_acquire) ? -1 : 0;
        }
        return win32::runWindowLoop();
    }

    void close() override {
        if (!hwnd_) { publishClosed(); return; }
        // 开启“关闭到托盘”时，点击关闭仅隐藏窗口，程序继续在托盘后台运行。
        if (closeToTray_) {
            if (showInternalTrayIcon()) ShowWindow(hwnd_, SW_HIDE);
            else restoreFromTray(); // Never leave a window without a recovery path.
            return;
        }
        DestroyWindow(hwnd_);
    }

    void requestClose() override {
        if (hwnd_) {
            PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        }
    }
