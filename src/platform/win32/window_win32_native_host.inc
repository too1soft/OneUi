    float normalizedDpiScale() const {
        return (dpiScale_ > 0.0f ? dpiScale_ : 1.0f) * contentScale_;
    }

    int logicalToPhysicalCeil(float value) const {
        return std::max(1, static_cast<int>(std::ceil(value * normalizedDpiScale())));
    }

    Point logicalPointFromClientPixels(int x, int y) const {
        const float scale = normalizedDpiScale();
        return Point{static_cast<float>(x) / scale, static_cast<float>(y) / scale};
    }

    Point logicalPointFromLParam(LPARAM lParam) const {
        return logicalPointFromClientPixels(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
    }

    void ensureCreated() {
        setPlatformMotionEnabled(clientAreaAnimationsEnabled());
        if (hwnd_ || !acceptingPostedCallbacks_.load(std::memory_order_acquire)) {
            return;
        }

        ensureProcessDpiAwareness();
        dpiScale_ = dpiScaleForWindowHandle(nullptr);

        HINSTANCE instance = GetModuleHandleW(nullptr);
        const wchar_t* className = L"OneUIWindow";

        WNDCLASSW windowClass{};
        windowClass.style = CS_DBLCLKS;
        windowClass.lpfnWndProc = &Win32Window::windowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        windowClass.hbrBackground = nullptr;
        RegisterClassW(&windowClass);

        DWORD style = windowStyle();
        DWORD exStyle = windowExStyle();
        RECT rect{0, 0, logicalToPhysicalCeil(static_cast<float>(options_.width)), logicalToPhysicalCeil(static_cast<float>(options_.height))};
        // 无边框窗口的 WM_NCCALCSIZE 会把客户区铺满整个窗口，所以窗口尺寸就等于期望的
        // 客户区尺寸，不能再用 AdjustWindowRectEx 按 WS_THICKFRAME 外扩(否则会大出一圈边框)。
        if (!options_.borderless) {
            AdjustWindowRectEx(&rect, style, FALSE, exStyle);
        }

        hwnd_ = CreateWindowExW(
            exStyle,
            className,
            options_.title.c_str(),
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            instance,
            this);

        if (hwnd_) {
            dpiScale_ = dpiScaleForWindowHandle(hwnd_);
#ifdef ONEUI_ENABLE_TEST_FRAME_CAPTURE
            // CreateWindow may query size limits before WM_NCCREATE installs
            // our instance. Reapply the requested test canvas with our handler active.
            if (std::getenv("ONEUI_TEST_DPI_SCALE")) {
                RECT captureRect{0, 0, logicalToPhysicalCeil(static_cast<float>(options_.width)),
                                      logicalToPhysicalCeil(static_cast<float>(options_.height))};
                if (!options_.borderless) AdjustWindowRectEx(&captureRect, style, FALSE, exStyle);
                SetWindowPos(hwnd_, nullptr, 0, 0, captureRect.right - captureRect.left,
                             captureRect.bottom - captureRect.top,
                             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
#endif
            DragAcceptFiles(hwnd_, fileDropHandler_ ? TRUE : FALSE);
            applyBorderlessShadow();
            applyRoundedCorners();
            captureNormalPlacement();
            if (options_.visible) {
                initGPU();
            }
        }

        if (hwnd_ && options_.fullscreen) {
            applyWindowState();
        }
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            window = reinterpret_cast<Win32Window*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
            window->hwnd_ = hwnd;
            win32::registerNativeWindow();
        }

        if (!window) {
            return DefWindowProcW(hwnd, message, wParam, lParam);
        }

        return window->handleMessage(message, wParam, lParam);
    }
