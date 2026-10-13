    static int frameMetricForDpi(int index, UINT dpi) {
        using Fn = int(WINAPI*)(int, UINT);
        static Fn fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi")));
        return fn ? fn(index, dpi) : GetSystemMetrics(index);
    }

    void applyBorderlessShadow() {
        if (!hwnd_ || !options_.borderless || options_.fullscreen) {
            return;
        }
        // Keep the opaque application client area out of the DWM glass frame.
        // Rounded-corner/DWM and companion-window shadows are handled separately.
        const MARGINS margins{0, 0, 0, 0};
        DwmExtendFrameIntoClientArea(hwnd_, &margins);
    }

    // 整体圆角：Win11 优先用 DWM 圆角首选项（抗锯齿 + 保留投影）；Win10 无该属性，
    // 退回 SetWindowRgn 裁一个圆角矩形区域。最大化/全屏时取消圆角（方正铺满）。
    void setCornerRadius(float radiusLogical) override {
        cornerRadiusLogical_ = radiusLogical > 0.0f ? radiusLogical : 0.0f;
        applyRoundedCorners();
    }

    void applyRoundedCorners() {
        if (!hwnd_) {
            return;
        }
        // Win11：DWMWA_WINDOW_CORNER_PREFERENCE = 33，DWMWCP_ROUND = 2 / DWMWCP_DONOTROUND = 1。
        const DWORD attr = 33;
        DWORD pref = (cornerRadiusLogical_ > 0.0f) ? 2 : 1;
        const bool dwmRounded = SUCCEEDED(DwmSetWindowAttribute(hwnd_, attr, &pref, sizeof(pref)));

        const bool square = cornerRadiusLogical_ <= 0.0f || borderlessMaximized_ || options_.fullscreen;
        if (dwmRounded) {
            // Win11 由 DWM 负责圆角（抗锯齿 + 自带柔和投影），清掉可能残留的区域与伴随投影。
            SetWindowRgn(hwnd_, nullptr, TRUE);
            shadowActive_ = false;
            updateShadowWindow();
            return;
        }
        // Win10 回退：用圆角矩形区域裁剪窗口。
        if (square) {
            SetWindowRgn(hwnd_, nullptr, TRUE);
            shadowActive_ = false;
            updateShadowWindow();
            return;
        }
        RECT rc{};
        if (!GetWindowRect(hwnd_, &rc)) {
            return;
        }
        const int w = rc.right - rc.left;
        const int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) {
            return;
        }
        const int d = logicalToPhysicalCeil(cornerRadiusLogical_ * 2.0f);
        HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, d, d);
        SetWindowRgn(hwnd_, rgn, TRUE); // 系统接管 rgn 生命周期，不需手动 DeleteObject
        // 区域裁剪丢掉了 DWM 投影，启用伴随分层窗口补一层柔光投影。
        shadowActive_ = true;
        updateShadowWindow();
    }

    // ——— Win10 圆角柔光投影（分层伴随窗口）———
    // GL SwapBuffers present 是不透明的，主窗本身无法做逐像素透明；因此用一个独立的
    // WS_EX_LAYERED 窗口叠在主窗正下方，用有向距离场画出主窗圆角轮廓外的抗锯齿柔光。
    static constexpr float kShadowMarginLogical = 18.0f; // 投影外扩/羽化半径（逻辑像素）
    static constexpr int kShadowMaxAlpha = 66;           // 贴边最深处透明度（0-255，克制些更高级）
    static constexpr float kShadowDropLogical = 3.0f;    // 轮廓下移量，模拟顶部来光的下坠投影

    void ensureShadowWindow() {
        if (shadowHwnd_ || !hwnd_) {
            return;
        }
        static const wchar_t* kShadowClass = L"OneUIShadowWindow";
        static bool registered = false;
        HINSTANCE instance = GetModuleHandleW(nullptr);
        if (!registered) {
            WNDCLASSW wc{};
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = instance;
            wc.lpszClassName = kShadowClass;
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            RegisterClassW(&wc);
            registered = true;
        }
        // 不设 owner：被 owner 的窗口恒在 owner 之上，会盖住内容；改为独立弹窗 + TOOLWINDOW
        // （不进任务栏/Alt-Tab），z 序由我们手动压到主窗正下方，并随主窗一起显隐/销毁。
        shadowHwnd_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            kShadowClass, L"", WS_POPUP,
            0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
        shadowBuiltW_ = shadowBuiltH_ = 0;
    }

    void destroyShadowWindow() {
        if (shadowHwnd_) {
            DestroyWindow(shadowHwnd_);
            shadowHwnd_ = nullptr;
        }
        shadowActive_ = false;
        shadowBuiltW_ = shadowBuiltH_ = 0;
    }

    // 根据主窗当前状态刷新伴随投影：Win10 圆角且正常显示时展示，最大化/最小化/隐藏/
    // 非圆角时隐藏。尺寸未变的纯移动只重定位，避免拖动时反复重绘位图。
    void updateShadowWindow() {
        if (!shadowActive_) {
            if (shadowHwnd_ && IsWindowVisible(shadowHwnd_)) {
                ShowWindow(shadowHwnd_, SW_HIDE);
            }
            return;
        }
        if (!hwnd_ || !IsWindowVisible(hwnd_) || IsIconic(hwnd_) ||
            borderlessMaximized_ || options_.fullscreen || cornerRadiusLogical_ <= 0.0f) {
            if (shadowHwnd_) {
                ShowWindow(shadowHwnd_, SW_HIDE);
            }
            return;
        }
        ensureShadowWindow();
        if (!shadowHwnd_) {
            return;
        }
        RECT rc{};
        if (!GetWindowRect(hwnd_, &rc)) {
            return;
        }
        const int mw = rc.right - rc.left;
        const int mh = rc.bottom - rc.top;
        if (mw <= 0 || mh <= 0) {
            ShowWindow(shadowHwnd_, SW_HIDE);
            return;
        }
        const int margin = logicalToPhysicalCeil(kShadowMarginLogical);
        const int W = mw + margin * 2;
        const int H = mh + margin * 2;
        const POINT ptDst{rc.left - margin, rc.top - margin};
        const float scale = normalizedDpiScale();
        const float radius = static_cast<float>(logicalToPhysicalCeil(cornerRadiusLogical_));

        if (mw == shadowBuiltW_ && mh == shadowBuiltH_ &&
            radius == shadowBuiltRadius_ && scale == shadowBuiltScale_) {
            // 尺寸没变，只需把投影窗挪到新位置并保持在主窗正下方。
            SetWindowPos(shadowHwnd_, hwnd_, ptDst.x, ptDst.y, W, H,
                         SWP_NOACTIVATE | SWP_NOREDRAW | SWP_SHOWWINDOW);
            return;
        }

        // 构建逐像素预乘 BGRA 位图：主窗圆角轮廓外用有向距离场做柔光衰减。
        const win32::RoundedWindowShadow shadow(mw, mh, radius, static_cast<float>(margin),
                                                kShadowDropLogical * scale, kShadowMaxAlpha);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = W;
        bmi.bmiHeader.biHeight = -H; // 自上而下
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC screenDC = GetDC(nullptr);
        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib || !bits) {
            if (dib) DeleteObject(dib);
            ReleaseDC(nullptr, screenDC);
            return;
        }
        auto* px = static_cast<uint32_t*>(bits);
        for (int y = 0; y < H; ++y) {
            const float fy = static_cast<float>(y - margin) + 0.5f;
            for (int x = 0; x < W; ++x) {
                const float fx = static_cast<float>(x - margin) + 0.5f;
                // 预乘黑色：RGB=0，仅 alpha 生效。
                px[static_cast<size_t>(y) * W + x] = static_cast<uint32_t>(shadow.alphaAt(fx, fy)) << 24;
            }
        }

        HDC memDC = CreateCompatibleDC(screenDC);
        HGDIOBJ oldBmp = SelectObject(memDC, dib);
        SIZE sz{W, H};
        POINT ptSrc{0, 0};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        POINT dst = ptDst;
        UpdateLayeredWindow(shadowHwnd_, screenDC, &dst, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);
        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);
        DeleteObject(dib);
        ReleaseDC(nullptr, screenDC);

        shadowBuiltW_ = mw;
        shadowBuiltH_ = mh;
        shadowBuiltRadius_ = radius;
        shadowBuiltScale_ = scale;
        // 压到主窗正下方并显示。
        SetWindowPos(shadowHwnd_, hwnd_, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
