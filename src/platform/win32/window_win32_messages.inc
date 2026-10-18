    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        if(message==WM_SHOWWINDOW||message==WM_SIZE||message==WM_ACTIVATE||message==WM_WINDOWPOSCHANGED||message==WM_STYLECHANGED||message==WM_DPICHANGED)queueStateChange();
        if (taskbarCreatedMessage_ != 0 && message == taskbarCreatedMessage_) {
            internalTrayVisible_ = false;
            if (closeToTray_ || trayEnabled_) {
                if (!showInternalTrayIcon()) restoreFromTray();
            }
            return 0;
        }
        switch (message) {
        case WM_GETOBJECT:
            if (static_cast<DWORD>(lParam)==static_cast<DWORD>(OBJID_CLIENT) && content_
                && SUCCEEDED(accessibilityComResult_) && callbackWindowAlive_->load()) {
                if(!accessibilityProvider_){
                    auto context=std::make_shared<win32_accessibility::Context>();
                    context->window=hwnd_;context->alive=callbackWindowAlive_;
                    context->root=[this]{return content_;};
                    context->focus=[this](Widget* widget){return requestFocus(widget,true);};
                    context->scale=[this]{return dpiScale();};
                    accessibilityProvider_=new win32_accessibility::Provider(context);
                    context->provider=accessibilityProvider_;
                }
                return LresultFromObject(IID_IAccessible,wParam,accessibilityProvider_);
            }
            return DefWindowProcW(hwnd_,message,wParam,lParam);
        case WM_DROPFILES:
        {
            const HDROP drop = reinterpret_cast<HDROP>(wParam);
            if (!drop) {
                return 0;
            }
            struct DropGuard {
                HDROP value;
                ~DropGuard() { DragFinish(value); }
            } guard{drop};

            if (!fileDropHandler_) {
                return 0;
            }
            try {
                POINT dropPoint{};
                // DragQueryPoint still fills pt when fNC is true. Borderless
                // windows can report that form near resize margins, so keep
                // the coordinate instead of redirecting the drop to (0, 0).
                DragQueryPoint(drop, &dropPoint);
                constexpr UINT kMaxDroppedPaths = 512;
                const UINT count = std::min(
                    DragQueryFileW(drop, 0xFFFFFFFFu, nullptr, 0),
                    kMaxDroppedPaths);
                std::vector<std::wstring> paths;
                paths.reserve(count);
                for (UINT index = 0; index < count; ++index) {
                    const UINT required = DragQueryFileW(drop, index, nullptr, 0);
                    if (required == 0) {
                        continue;
                    }
                    std::wstring path(static_cast<std::size_t>(required) + 1, L'\0');
                    const UINT copied = DragQueryFileW(
                        drop,
                        index,
                        path.data(),
                        static_cast<UINT>(path.size()));
                    if (copied == 0) {
                        continue;
                    }
                    path.resize(copied);
                    paths.push_back(std::move(path));
                }
                if (!paths.empty()) {
                    const auto alive = callbackWindowAlive_;
                    const auto handler = fileDropHandler_;
                    handler(
                        std::move(paths),
                        logicalPointFromClientPixels(dropPoint.x, dropPoint.y));
                    if (!alive->load(std::memory_order_acquire)) {
                        return 0;
                    }
                }
            } catch (...) {
                // A malformed shell payload or application callback must not
                // escape the Win32 window procedure.
            }
            return 0;
        }
        case WM_SETTINGCHANGE:
            setPlatformMotionEnabled(clientAreaAnimationsEnabled());
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_CLOSE:
            if(dispatchCloseRequest())return 0;
            if (onCloseRequested_) { auto callback = onCloseRequested_; callback(false); }
            else close();
            return 0;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wParam) {
                closeToTray_ = false;
                if (onCloseRequested_) { auto callback = onCloseRequested_; callback(true); }
                else close();
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            submitInvalidations();
            paint();
            return 0;
        case kOneUiFlushInvalidations:
            submitInvalidations();
            return 0;
        case oneui::kTrayCallbackMessage:
            switch (LOWORD(lParam)) {
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case NIN_SELECT:
            case NIN_KEYSELECT:
                restoreFromTray();
                break;
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU:
                showTrayMenu();
                break;
            }
            return 0;
        case WM_COMMAND:
            if (HIWORD(wParam) == 0) { // 菜单命令
                switch (LOWORD(wParam)) {
                case oneui::kTrayCommandShow:
                    restoreFromTray();
                    return 0;
                case oneui::kTrayCommandExit:
                    if (onCloseRequested_) { auto callback = onCloseRequested_; callback(true); return 0; }
                    closeToTray_ = false; // 强制真正退出，绕过“关闭到托盘”
                    hideInternalTrayIcon();
                    if (hwnd_) {
                        DestroyWindow(hwnd_);
                    }
                    return 0;
                }
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case kOneUiRunPostedCallbacks:
            runPostedCallbacks();
            return 0;
        case kOneUiFinishWindowStatePaint:
            flushWindowStatePaint();
            return 0;
        case kOneUiDeferredFullPaint:
            flushDeferredFullPaint();
            return 0;
        case WM_TIMER:
            if (wParam == kOneUiAnimationTimer) {
                runAnimationFrameCallbacks();
                return 0;
            }
            if (wParam == kOneUiInteractivePaintTimer) {
                runInteractivePaintFrame();
                return 0;
            }
            if (wParam == kOneUiTooltipTimer) {
                KillTimer(hwnd_, kOneUiTooltipTimer);
                if (!hoveredTooltip_.empty()) {
                    tooltipVisible_ = true;
                    requestRedraw();
                }
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_MOUSEMOVE:
        {
            const Point point = logicalPointFromLParam(lParam);
            dispatchMouseMove(point);
            updateCursor(point);
            return 0;
        }
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                POINT point{};
                if (GetCursorPos(&point) && ScreenToClient(hwnd_, &point)) {
                    updateCursor(logicalPointFromClientPixels(point.x, point.y), true);
                } else {
                    SetCursor(cursorForKind(CursorKind::Default));
                }
                return TRUE;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_MOUSELEAVE:
            mouseLeaveTracking_ = false;
            lastCursorKind_ = CursorKind::Default;
            hasLastCursorPoint_ = false;
            SetCursor(cursorForKind(CursorKind::Default));
            clearTooltip();
            if (content_) {
                if (content_->clearInteractionState()) {
                    requestInteractiveRedraw();
                }
            }
            return 0;
        case WM_ENTERSIZEMOVE:
            interactiveResizeActive_ = true;
            return 0;
        case WM_EXITSIZEMOVE:
            interactiveResizeActive_ = false;
            requestDeferredFullPaint();
            printRenderTrace(true);
            return 0;
        case WM_MOUSEWHEEL:
            dispatchMouseWheel(wParam, lParam);
            return 0;
        case WM_LBUTTONDOWN:
            // Depending on DWM/window activation state, Windows can deliver a
            // real press over a maximized WS_POPUP caption lane as a client
            // message even though WM_NCHITTEST classifies the same point as
            // HTCAPTION. Convert that fallback path before widget dispatch so
            // title-bar accessory content cannot swallow restore-and-drag.
            if (options_.borderless && borderlessMaximized_ && !options_.fullscreen) {
                POINT screenPoint{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (ClientToScreen(hwnd_, &screenPoint)) {
                    const LPARAM screenPosition = MAKELPARAM(screenPoint.x, screenPoint.y);
                    if (hitTestBorderlessWindow(screenPosition) == HTCAPTION
                        && restoreBorderlessForCaptionDrag(screenPoint.x, screenPoint.y)) {
                        return DefWindowProcW(
                            hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, screenPosition);
                    }
                }
            }
            SetFocus(hwnd_);
            SetCapture(hwnd_);
            dispatchMouseDown(lParam, MouseButton::Left);
            return 0;
        case WM_LBUTTONDBLCLK:
            SetFocus(hwnd_);
            SetCapture(hwnd_);
            dispatchMouseDown(lParam, MouseButton::Left, 2);
            return 0;
        case WM_LBUTTONUP:
            ReleaseCapture();
            dispatchMouseUp(lParam, MouseButton::Left);
            return 0;
        case WM_RBUTTONDOWN:
            dispatchMouseDown(lParam, MouseButton::Right);
            return 0;
        case WM_RBUTTONUP:
            dispatchMouseUp(lParam, MouseButton::Right);
            return 0;
        case WM_MBUTTONDOWN:
            dispatchMouseDown(lParam, MouseButton::Middle);
            return 0;
        case WM_MBUTTONUP:
            dispatchMouseUp(lParam, MouseButton::Middle);
            return 0;
        case WM_WINDOWPOSCHANGED:
        {
            // 位置/尺寸/Z 序变化（含失焦→再激活）后，重定位伴随投影并把它重新压到主窗正
            // 下方——修复“失焦后投影消失、要移动一下才回来”。必须先走默认处理，好派生出
            // WM_MOVE/WM_SIZE（区域圆角重算依赖 WM_SIZE）。
            const LRESULT r = DefWindowProcW(hwnd_, message, wParam, lParam);
            captureNormalPlacement();
            updateShadowWindow();
            return r;
        }
        case WM_DISPLAYCHANGE:
            animationFrameMonitor_ = nullptr;
            animationFrameIntervalMs_ = 1000.0 / 60.0;
            return 0;
        case WM_SHOWWINDOW:
#if ONEUI_DEVELOPER_TOOLS
            if(developerTools_)developerTools_->resetCadence();
#endif
            updateShadowWindow(); // 显隐（含托盘还原/隐藏）时同步投影窗显隐
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_GETMINMAXINFO:
        {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            if (info) {
                LONG nonClientWidth = 0;
                LONG nonClientHeight = 0;
                RECT windowRect{};
                RECT clientRect{};
                if (GetWindowRect(hwnd_, &windowRect) && GetClientRect(hwnd_, &clientRect)) {
                    nonClientWidth = std::max<LONG>(
                        0,
                        (windowRect.right - windowRect.left) - (clientRect.right - clientRect.left));
                    nonClientHeight = std::max<LONG>(
                        0,
                        (windowRect.bottom - windowRect.top) - (clientRect.bottom - clientRect.top));
                }
#ifdef ONEUI_ENABLE_TEST_FRAME_CAPTURE
                // A capture fixture may require a logical viewport larger than the
                // developer's physical desktop at forced DPI. Do not silently clamp
                // that test canvas to the monitor's default maximum tracking size.
                if (std::getenv("ONEUI_TEST_DPI_SCALE")) {
                    info->ptMaxTrackSize.x = std::max<LONG>(info->ptMaxTrackSize.x,
                        static_cast<LONG>(std::ceil(options_.width * dpiScale_)) + nonClientWidth + 32);
                    info->ptMaxTrackSize.y = std::max<LONG>(info->ptMaxTrackSize.y,
                        static_cast<LONG>(std::ceil(options_.height * dpiScale_)) + nonClientHeight + 32);
                }
#endif
                if(maximumClientSize_.width>0)info->ptMaxTrackSize.x=std::max(info->ptMinTrackSize.x,LONG(std::ceil(maximumClientSize_.width*dpiScale_))+nonClientWidth);
                if(maximumClientSize_.height>0)info->ptMaxTrackSize.y=std::max(info->ptMinTrackSize.y,LONG(std::ceil(maximumClientSize_.height*dpiScale_))+nonClientHeight);
                if (minimumClientSize_.width > 0.0f) {
                    info->ptMinTrackSize.x = std::max<LONG>(
                        info->ptMinTrackSize.x,
                        static_cast<LONG>(std::ceil(minimumClientSize_.width * dpiScale_)) + nonClientWidth);
                }
                if (minimumClientSize_.height > 0.0f) {
                    info->ptMinTrackSize.y = std::max<LONG>(
                        info->ptMinTrackSize.y,
                        static_cast<LONG>(std::ceil(minimumClientSize_.height * dpiScale_)) + nonClientHeight);
                }
            }
            return 0;
        }
        case WM_SIZE:
#if ONEUI_DEVELOPER_TOOLS
            if(developerTools_)developerTools_->resetCadence();
#endif
            recordResizeMessage(wParam);
            if (wParam == SIZE_MAXIMIZED) {
                lastKnownMaximized_ = true;
            } else if (wParam == SIZE_RESTORED) {
                lastKnownMaximized_ = borderlessMaximized_;
            }
            if (wParam != SIZE_MINIMIZED) syncTitleBarWindowState();
            if (wParam == SIZE_MINIMIZED && shadowHwnd_) {
                ShowWindow(shadowHwnd_, SW_HIDE); // 最小化立刻收起投影
            }
            if (wParam != SIZE_MINIMIZED) {
                applyRoundedCorners(); // 尺寸变化后重算圆角区域，避免拉伸/露白
                scheduleClientSizeChanged();
            }
            if (applyingWindowState_) {
                windowStatePaintPending_ = true;
                return 0;
            }
            if (wParam != SIZE_MINIMIZED) {
                if (interactiveResizeActive_) {
                    InvalidateRect(hwnd_, nullptr, FALSE);
                    flushInteractivePaint();
                } else {
                    requestDeferredFullPaint();
                }
            }
            return 0;
        case kOneUiDpiChanged:
            handleDpiChanged(wParam, lParam);
            return 0;
        case WM_NCCALCSIZE:
            // 无边框窗口带着 WS_THICKFRAME(为拿 DWM 投影)，这里把非客户区尺寸清零，
            // 使客户区铺满整个窗口——投影保留、可见边框消失。常规最大化走自定义逻辑
            // (applyBorderlessMaximize，摆到工作区，IsZoomed 恒 false)，此时直接铺满。
            if (wParam == TRUE && options_.borderless && !options_.fullscreen) {
                if (IsZoomed(hwnd_)) {
                    // 兜底：若经由 Aero Snap 贴顶等路径进入真·系统最大化，系统会把窗口
                    // 摆成工作区外溢一圈边框。此时必须按边框厚度内缩，否则内容被屏幕边缘
                    // 裁掉、标题栏按钮跑到屏幕外。
                    auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                    const UINT dpi = static_cast<UINT>(std::lround(normalizedDpiScale() * kDefaultDpi));
                    const int fx = frameMetricForDpi(SM_CXSIZEFRAME, dpi) + frameMetricForDpi(SM_CXPADDEDBORDER, dpi);
                    const int fy = frameMetricForDpi(SM_CYSIZEFRAME, dpi) + frameMetricForDpi(SM_CXPADDEDBORDER, dpi);
                    p->rgrc[0].left += fx;
                    p->rgrc[0].right -= fx;
                    p->rgrc[0].top += fy;
                    p->rgrc[0].bottom -= fy;
                }
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_NCHITTEST:
            if (const auto hit = hitTestBorderlessWindow(lParam); hit != HTNOWHERE) {
                return hit;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_NCLBUTTONDOWN:
            // WS_POPUP windows do not get the shell's usual "drag a maximized
            // caption move loop from WM_NCLBUTTONDOWN alone. Restore beneath
            // the pointer when needed, then explicitly enter SC_MOVE while the
            // physical button is still down. This keeps accessory controls
            // interactive and makes every HTCAPTION lane actually draggable.
            if (options_.borderless && wParam == HTCAPTION && !options_.fullscreen) {
                // A persisted normal placement can exactly fill the monitor
                // work area even though it is not marked maximized.  The shell
                // move loop clamps that rectangle back to the work area and
                // makes the caption appear non-draggable.  Treat that boundary
                // state like maximized chrome and restore it beneath the pointer
                // before entering the native move loop.
                restoreBorderlessForCaptionDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                ReleaseCapture();
                return DefWindowProcW(
                    hwnd_, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, lParam);
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_NCLBUTTONDBLCLK:
            // A borderless window has no native caption style for DefWindowProc
            // to maximize. Preserve the standard title-bar contract explicitly
            // for drag regions while leaving interactive accessory content alone.
            if (options_.borderless && wParam == HTCAPTION && !options_.fullscreen) {
                toggleMaximize();
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_ACTIVATE:
            dispatchActivationChanged(LOWORD(wParam) != WA_INACTIVE);
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_SETFOCUS:
            dispatchFocusChanged(true);
            return 0;
        case WM_KILLFOCUS:
            pendingHighSurrogate_ = 0;
            resetTrackedKeyState();
            dispatchFocusChanged(false);
            return 0;
        case WM_KEYDOWN:
            dispatchKeyDown(wParam, lParam);
            return 0;
        case WM_KEYUP:
            dispatchKeyUp(wParam, lParam);
            return 0;
        case WM_SYSKEYDOWN:
            if (dispatchKeyDown(wParam, lParam)) {
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_SYSKEYUP:
            if (dispatchKeyUp(wParam, lParam)) {
                return 0;
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_CHAR:
            dispatchTextInput(wParam);
            return 0;
        case WM_UNICHAR:
            if (wParam == UNICODE_NOCHAR) {
                return TRUE;
            }
            dispatchUnicodeScalar(static_cast<std::uint32_t>(wParam));
            return 0;
        case WM_IME_STARTCOMPOSITION:
            beginImeComposition();
            updateImePosition(true);
            return content_ && content_->textInputState().drawsPreedit ? 0 : DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_IME_COMPOSITION:
            return dispatchImeComposition(wParam, lParam);
        case WM_IME_ENDCOMPOSITION:
            endImeComposition();
            // The system owns the preedit window for controls such as the
            // terminal. It must receive END as well as START, or the final
            // composition text can remain floating after Enter.
            return content_ && content_->textInputState().drawsPreedit ? 0 : DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_IME_SETCONTEXT:
            if (content_ && content_->textInputState().drawsPreedit) lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
            updateImePosition(true);
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case WM_DESTROY:
            return 0;
        case WM_NCDESTROY:
        {
            callbackWindowAlive_->store(false, std::memory_order_release);
            captureNormalPlacement();
            hideInternalTrayIcon();
            acceptingPostedCallbacks_.store(false, std::memory_order_release);
            discardPostedCallbacks();
            if (animationFrameTimer_) {
                CancelWaitableTimer(animationFrameTimer_);
            }
            shutdownGPU();
            destroyShadowWindow(); // 主窗销毁时一并销毁独立的伴随投影窗
            HWND destroyedHwnd = hwnd_;
            SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
            hwnd_ = nullptr;
            win32::unregisterNativeWindow();
            publishClosed();
            return DefWindowProcW(destroyedHwnd, message, wParam, lParam);
        }
        default:
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        }
    }
