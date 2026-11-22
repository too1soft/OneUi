    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_SHOWWINDOW || message == WM_SIZE || message == WM_ACTIVATE ||
            message == WM_STYLECHANGED || message == WM_DPICHANGED) {
            queueStateChange();
        }
        if (taskbarCreatedMessage_ != 0 && message == taskbarCreatedMessage_) {
            internalTrayVisible_ = false;
            if (closeToTray_ || trayEnabled_) {
                if (!showInternalTrayIcon()) restoreFromTray();
            }
            return 0;
        }
        switch (message) {
        case WM_GETOBJECT: {
            const auto objectId = static_cast<LONG>(lParam);
            if ((objectId == OBJID_CLIENT || objectId == UiaRootObjectId) && content_
                && SUCCEEDED(accessibilityComResult_) && callbackWindowAlive_->load()) {
                if (!accessibilityProvider_) {
                    auto context = std::make_shared<win32_accessibility::Context>();
                    context->window = hwnd_;
                    context->alive = callbackWindowAlive_;
                    context->root = [this] { return content_; };
                    context->focus = [this](Widget* widget) { return requestFocus(widget, true); };
                    context->scale = [this] { return dpiScale(); };
                    context->frameRevision = [] { return internal::accessibilityFrameRevision; };
                    if (internal::scrollTraceEnabled()) {
                        const auto window = reinterpret_cast<std::uintptr_t>(hwnd_);
                        context->recordSnapshot = [window](double elapsed, std::size_t visited, std::size_t children) {
                            internal::ScrollTraceEvent event{"win32-a11y", "snapshot_rebuild", window};
                            event.durationMs = elapsed;
                            event.detailA = static_cast<double>(visited);
                            event.detailB = static_cast<double>(children);
                            internal::writeScrollTrace(event);
                        };
                    }
                    accessibilityProvider_ = new win32_accessibility::Provider(context);
                    context->provider = accessibilityProvider_;
                }
                if (objectId == OBJID_CLIENT) {
                    const win32::DispatchTraceScope trace("msaa_marshal",
                        reinterpret_cast<std::uintptr_t>(hwnd_), WM_GETOBJECT);
                    return LresultFromObject(IID_IAccessible, wParam, accessibilityProvider_);
                }
                if (const auto result = accessibilityAutomation_.reply(hwnd_, wParam, lParam, accessibilityProvider_))
                    return result;
            }
            // Do not create an OS proxy while our provider is shutting down.
            if (!callbackWindowAlive_->load()) return 0;
            return DefWindowProcW(hwnd_,message,wParam,lParam);
        }
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
        case WM_PRINTCLIENT:
            printClient(reinterpret_cast<HDC>(wParam), lParam);
            return 0;
        case WM_PAINT:
            if (clientSizeChangedAtPaint_) {
                const auto alive = callbackWindowAlive_;
                deliverClientSizeChanged();
                if (!alive->load(std::memory_order_acquire)) return 0;
            }
            submitInvalidations();
            paint();
            return 0;
        case kOneUiFlushInvalidations:
            // Input mutations and visual animation are sampled together. Do
            // not expose an intermediate state before their scheduled frame.
            if (!inputPaintPending_ && !coalescedEditPaintPending_) submitInvalidations();
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
                        callbackWindowAlive_->store(false, std::memory_order_release);
                        DestroyWindow(hwnd_);
                    }
                    return 0;
                }
            }
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        case kOneUiRunPostedCallbacks:
            runPostedCallbacks();
            return 0;
        case win32::deferredGlReadyMessage:
            if (deferredStartupGl_.pending()) {
                deferredGlReady_ = true;
                scheduleDeferredGlAdoption();
            }
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
            const auto alive = callbackWindowAlive_;
            const Point point = logicalPointFromLParam(lParam);
            dispatchMouseMove(point);
            if (alive->load(std::memory_order_acquire)) updateCursor(point);
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
            tooltipWaitForMove_ = false;
            if (content_) {
                if (content_->clearInteractionState()) {
                    requestInteractiveRedraw();
                }
            }
            return 0;
        case WM_ENTERSIZEMOVE:
            interactiveResizeActive_ = true;
            interactiveSizeChanged_ = false;
            lastLiveResizePaintMs_ = 0.0;
            return 0;
        case WM_EXITSIZEMOVE:
            interactiveResizeActive_ = false;
            if (std::exchange(interactiveSizeChanged_, false)) requestDeferredFullPaint();
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
                    const auto alive = callbackWindowAlive_;
                    const auto hit = hitTestBorderlessWindow(screenPosition);
                    if (!alive->load(std::memory_order_acquire)) return 0;
                    if (hit == HTCAPTION)
                        return beginBorderlessCaptionDrag(screenPosition);
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
            const auto* position = reinterpret_cast<const WINDOWPOS*>(lParam);
            // Position is not part of WindowState. WM_SIZE, WM_ACTIVATE and
            // WM_DPICHANGED still cover client size, activation and scale.
            // Position messages also carry visibility and z-order changes.
            if (position && ((position->flags & (SWP_NOSIZE | SWP_NOZORDER)) !=
                    (SWP_NOSIZE | SWP_NOZORDER) || (position->flags & (SWP_SHOWWINDOW | SWP_HIDEWINDOW)))) {
                queueStateChange();
            }
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
            if (interactiveResizeActive_) interactiveSizeChanged_ = true;
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
                requestDeferredFullPaint();
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
        {
            const auto alive = callbackWindowAlive_;
            const auto hit = hitTestBorderlessWindow(lParam);
            if (!alive->load(std::memory_order_acquire)) return HTNOWHERE;
            if (hit != HTNOWHERE) return hit;
            return DefWindowProcW(hwnd_, message, wParam, lParam);
        }
        case WM_NCLBUTTONDOWN:
            if (options_.borderless && wParam == HTCAPTION && !options_.fullscreen) {
                // A maximized or work-area-sized WS_POPUP needs explicit
                // restoration. Ordinary captions use the system move handler.
                return beginBorderlessCaptionDrag(lParam);
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
            return dispatchImeEndComposition(wParam, lParam);
        case WM_IME_SETCONTEXT:
            return dispatchImeSetContext(wParam, lParam);
        case WM_DESTROY:
            callbackWindowAlive_->store(false, std::memory_order_release);
            deferredStartupGl_.cancel();
            retireAccessibilityProvider();
            return 0;
        case WM_NCDESTROY:
        {
            callbackWindowAlive_->store(false, std::memory_order_release);
            if (accessibilityProvider_) retireAccessibilityProvider();
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
            internal::forgetInteractionWindow(reinterpret_cast<std::uintptr_t>(destroyedHwnd));
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
