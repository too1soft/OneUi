#if ONEUI_DEVELOPER_TOOLS
    RECT developerOverlayPixels(const RECT& client) const {
        const float scale=normalizedDpiScale();
        return {std::max<LONG>(0,client.right-LONG(408*scale)),std::max<LONG>(0,client.bottom-LONG(194*scale)),client.right,client.bottom};
    }
    void paintDeveloperOverlay(Canvas& canvas,const internal::DeveloperRecorder& tools,float width,float height) {
        const auto& summary=tools.summary();const auto& f=summary.last;
        const Rect panel{std::max(4.f,width-400),std::max(4.f,height-186),std::min(392.f,width-8),178};
        canvas.save();canvas.clipRect(panel);canvas.fillRect(panel,Color{20,29,25,250},10);
        auto text=[&](const std::wstring& value,float y,Color color=Color{220,230,224}){canvas.drawText(value,{panel.x+14,panel.y+y,panel.width-28,22},color,12,TextAlign::Left);};
        text(L"OneUI DevTools   F12 显隐 · Ctrl+F12 导出",10,Color{147,222,179});
        std::wostringstream timing;timing << std::fixed << std::setprecision(2) << L"绘制 " << f.paintMs << L"ms  内容 " << f.contentMs << L"ms  提交 " << f.submitMs+f.blitMs << L"ms";text(timing.str(),36);
        text(L"慢绘制 "+std::to_wstring(summary.slowPaints)+L"  动画延迟 "+std::to_wstring(summary.animationDelays)+L"  倒退 "+std::to_wstring(summary.motionReversals),60);
        auto wide=[](const std::string& v){return unicode::fromUtf8(v);};
        text(L"耗时控件："+wide(f.hottest.component)+L"（自身绘制）",84);
        if(const auto* issue=tools.latestIssue()) {
            const auto separator=issue->source.file.find_last_of("/\\");
            const auto file=wide(issue->source.file.substr(separator==std::string::npos?0:separator+1));
            text((issue->flags&MotionReversal?std::wstring(L"进度倒退："):issue->flags&SlowPaint?std::wstring(L"慢绘制："):std::wstring(L"动画延迟："))+wide(issue->source.component)+L" "+file+L":"+std::to_wstring(issue->source.line),108,Color{255,196,127});
        } else text(L"尚未检测到异常 · 按需绘制，无采样定时器",108);
        text(developerExportStatus_>0?L"已导出工作目录 oneui-devtools.json":developerExportStatus_<0?L"导出失败：检查工作目录写入权限":L"CPU 侧计时；面板和采样本身有开销",140,Color{151,170,158});
        canvas.restore();
    }
#endif
    void paint() {
        ONEUI_VIEW_TIME(Paint);
        const double paintStartMs = currentTimeMs();
#if ONEUI_DEVELOPER_TOOLS
        const auto diagnostics=developerTools_; // Safe if application disables tools during paint.
        internal::DiagnosticSession diagnosticSession(diagnostics.get());
        internal::FrameProfileSession profile(renderTraceEnabled_ || bool(diagnostics));
        const double beforeContent=rendererInfo_.contentMs, beforeSubmit=rendererInfo_.submitMs, beforeBlit=rendererInfo_.blitMs;
        if(diagnostics)diagnostics->beginFrame(paintStartMs);
        const auto finishDiagnostics=[&] {
            if(!diagnostics)return;
            DeveloperFrame frame;
            frame.contentMs=rendererInfo_.contentMs-beforeContent;
            frame.submitMs=rendererInfo_.submitMs-beforeSubmit;
            frame.blitMs=rendererInfo_.blitMs-beforeBlit;
            frame.layoutMs=profile.data.ms[0];frame.textLayoutMs=profile.data.ms[1];
            frame.fillMs=profile.data.ms[2];frame.pathMs=profile.data.ms[3];
            diagnostics->finishFrame(currentTimeMs(),contentAnimationFramePending_,frame);
        };
#else
        internal::FrameProfileSession profile(renderTraceEnabled_);
        const auto finishDiagnostics=[] {};
#endif
        RECT clientRect{};
        GetClientRect(hwnd_, &clientRect);

        const bool useGPU = gpuAvailable_ && glContext_ && glDC_;
        if (useGPU) {
            // The context stays current between frames; rebinding it every
            // paint makes the driver synchronize for nothing.
            if (wglGetCurrentContext() != glContext_ || wglGetCurrentDC() != glDC_) {
                wglMakeCurrent(glDC_, glContext_);
            }
            glViewport(0, 0,
                static_cast<GLsizei>(clientRect.right - clientRect.left),
                static_cast<GLsizei>(clientRect.bottom - clientRect.top));
        }
        PAINTSTRUCT paintStruct{};
        HDC paintDc = BeginPaint(hwnd_, &paintStruct);
        HDC dc = useGPU ? glDC_ : paintDc;
        RECT dirtyRect = paintStruct.rcPaint;
#if ONEUI_DEVELOPER_TOOLS
        if(diagnostics && diagnostics->options.overlay) {
            const RECT overlay=developerOverlayPixels(clientRect);
            RECT combined{};UnionRect(&combined,&dirtyRect,&overlay);dirtyRect=combined;
        }
#endif
        const int width = std::max<LONG>(1, clientRect.right - clientRect.left);
        const int height = std::max<LONG>(1, clientRect.bottom - clientRect.top);
        const float scale = normalizedDpiScale();
        const float logicalWidth = static_cast<float>(width) / scale;
        const float logicalHeight = static_cast<float>(height) / scale;

        const double surfaceStartMs = currentTimeMs();
        const bool allocatedSurface = ensurePaintSurfaceCapacity(width, height);
        const double surfaceMs = currentTimeMs() - surfaceStartMs;

        if (!paintSurface_) {
            EndPaint(hwnd_, &paintStruct);
            return;
        }
        if (allocatedSurface) {
            dirtyRect = clientRect;
        }
        const int dirtyX = std::clamp<LONG>(dirtyRect.left, 0, width);
        const int dirtyY = std::clamp<LONG>(dirtyRect.top, 0, height);
        const int dirtyRight = std::clamp<LONG>(dirtyRect.right, dirtyX, width);
        const int dirtyBottom = std::clamp<LONG>(dirtyRect.bottom, dirtyY, height);
        const int dirtyWidth = dirtyRight - dirtyX;
        const int dirtyHeight = dirtyBottom - dirtyY;
        if (dirtyWidth <= 0 || dirtyHeight <= 0) {
            EndPaint(hwnd_, &paintStruct);
            return;
        }

        SkCanvas* skCanvas = paintSurface_->getCanvas();
        auto rawCanvasOwner = rendering::makeSkiaCanvas(
            *skCanvas,
            nullptr,
            Rect{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)});
        auto& rawCanvas = *rawCanvasOwner;
        const bool fullPaint = dirtyX == 0 && dirtyY == 0 && dirtyWidth == width && dirtyHeight == height;
        if (fullPaint) {
            rawCanvas.clear(colors::Surface);
        }

        skCanvas->save();
        skCanvas->scale(scale, scale);
        auto canvasOwner = rendering::makeSkiaCanvas(
            *skCanvas,
            &defaultFontFamily_,
            Rect{0.0f, 0.0f, logicalWidth, logicalHeight});
        auto& canvas = *canvasOwner;
        if (!fullPaint) {
            const Rect dirtyCanvasRect{
                static_cast<float>(dirtyX) / scale,
                static_cast<float>(dirtyY) / scale,
                static_cast<float>(dirtyWidth) / scale,
                static_cast<float>(dirtyHeight) / scale};
            canvas.save();
            canvas.clipRect(dirtyCanvasRect);
            canvas.fillRect(dirtyCanvasRect, colors::Surface, 0.0f);
        }

        if (content_ && content_->visible()) {
            const double contentStartMs = currentTimeMs();
            g_primitivePaintTrace = PrimitivePaintTrace{};
            { ONEUI_VIEW_TIME(Layout); content_->setFrame(Rect{0.0f, 0.0f, logicalWidth, logicalHeight}); }
            { internal::DiagnosticPaintSpan diagnostic(*content_); content_->paint(canvas); }
            paintTooltip(canvas);
            recordContentPaint(currentTimeMs() - contentStartMs);
            recordPrimitivePaint(g_primitivePaintTrace);
            for (std::size_t i = 0; i < traceStageMs_.size(); ++i) traceStageMs_[i] += profile.data.ms[i];
        }

        if (!fullPaint) {
            canvas.restore();
        }
#if ONEUI_DEVELOPER_TOOLS
        if(diagnostics && diagnostics->options.overlay)paintDeveloperOverlay(canvas,*diagnostics,logicalWidth,logicalHeight);
#endif
        skCanvas->restore();

        if (gpuAvailable_ && grContext_ && windowSurface_) {
            const double submitStartMs = currentTimeMs();
            SkCanvas* presentCanvas = windowSurface_->getCanvas();
            presentCanvas->clear(SK_ColorBLACK);
            paintSurface_->draw(presentCanvas, 0.0f, 0.0f);
            grContext_->flushAndSubmit();
            const double presentStarted = currentTimeMs();
            const BOOL swapped = SwapBuffers(glDC_);
            internal::workspacePresentMs = currentTimeMs() - presentStarted;
            if (swapped && nvidiaDriver_) {
                const double censusStarted = internal::workspaceFrameProbe ? currentTimeMs() : 0;
                absorbDriverThreadCensus();
                if (censusStarted) internal::workspaceCensusMs = currentTimeMs() - censusStarted;
            }
            const double submitMs = currentTimeMs() - submitStartMs;
            traceSubmitMs_ += submitMs;
            rendererInfo_.submitMs += submitMs;
            if (swapped) {
                EndPaint(hwnd_, &paintStruct);
                recordPaint(width, height, fullPaint, allocatedSurface, surfaceMs, currentTimeMs() - paintStartMs);
                flushDeferredImePosition();
                finishDiagnostics();
                return;
            }
            rendererInfo_.reason = "swap-buffers-failed";
            setRenderBackend(RenderBackend::Unknown);
            gpuAvailable_ = false;
            swapIntervalEnabled_ = false;
            windowSurface_.reset();
            paintSurface_.reset();
            paintSurfaceWidth_ = 0;
            paintSurfaceHeight_ = 0;
            InvalidateRect(hwnd_, nullptr, FALSE);
            EndPaint(hwnd_, &paintStruct);
            return;
        }

        SkPixmap pixmap;
        if (paintSurface_->peekPixels(&pixmap)) {
            const double blitStartMs = currentTimeMs();
            if (fullPaint) {
                BITMAPINFO bitmapInfo{};
                bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bitmapInfo.bmiHeader.biWidth = paintSurfaceWidth_;
                bitmapInfo.bmiHeader.biHeight = -paintSurfaceHeight_;
                bitmapInfo.bmiHeader.biPlanes = 1;
                bitmapInfo.bmiHeader.biBitCount = 32;
                bitmapInfo.bmiHeader.biCompression = BI_RGB;

                StretchDIBits(
                    dc,
                    0,
                    0,
                    width,
                    height,
                    0,
                    0,
                    width,
                    height,
                    pixmap.addr(),
                    &bitmapInfo,
                    DIB_RGB_COLORS,
                    SRCCOPY);
            } else {
                const std::size_t pixelCount = static_cast<std::size_t>(dirtyWidth) * static_cast<std::size_t>(dirtyHeight);
                blitScratch_.resize(pixelCount);
                const std::size_t rowBytes = static_cast<std::size_t>(dirtyWidth) * 4;
                for (int row = 0; row < dirtyHeight; ++row) {
                    const void* src = pixmap.addr(dirtyX, dirtyY + row);
                    std::memcpy(
                        reinterpret_cast<unsigned char*>(blitScratch_.data()) + static_cast<std::size_t>(row) * rowBytes,
                        src,
                        rowBytes);
                }

                BITMAPINFO bitmapInfo{};
                bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bitmapInfo.bmiHeader.biWidth = dirtyWidth;
                bitmapInfo.bmiHeader.biHeight = -dirtyHeight;
                bitmapInfo.bmiHeader.biPlanes = 1;
                bitmapInfo.bmiHeader.biBitCount = 32;
                bitmapInfo.bmiHeader.biCompression = BI_RGB;

                SetDIBitsToDevice(
                    dc,
                    dirtyX,
                    dirtyY,
                    dirtyWidth,
                    dirtyHeight,
                    0,
                    0,
                    0,
                    dirtyHeight,
                    blitScratch_.data(),
                    &bitmapInfo,
                    DIB_RGB_COLORS);
            }
            recordBlit(currentTimeMs() - blitStartMs);
        }

        EndPaint(hwnd_, &paintStruct);
        recordPaint(width, height, fullPaint, allocatedSurface, surfaceMs, currentTimeMs() - paintStartMs);
        flushDeferredImePosition();
        finishDiagnostics();
    }

    static int alignedPaintSurfaceSize(int requested) {
        const int padded = std::max(1, requested + kPaintSurfaceGrowthPadding);
        return ((padded + kPaintSurfaceAlignment - 1) / kPaintSurfaceAlignment) * kPaintSurfaceAlignment;
    }

    bool ensurePaintSurfaceCapacity(int width, int height) {
        if (paintSurface_) {
            if (gpuAvailable_ && windowSurface_ &&
                paintSurfaceWidth_ == width && paintSurfaceHeight_ == height) {
                return false;
            }
            if (!gpuAvailable_ && paintSurfaceWidth_ >= width && paintSurfaceHeight_ >= height) {
                return false;
            }
        }

        int surfaceWidth = alignedPaintSurfaceSize(width);
        int surfaceHeight = alignedPaintSurfaceSize(height);
        SkImageInfo imageInfo = SkImageInfo::Make(surfaceWidth, surfaceHeight, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        if (gpuAvailable_ && grContext_) {
            surfaceWidth = width;
            surfaceHeight = height;
            imageInfo = SkImageInfo::Make(surfaceWidth, surfaceHeight, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
            auto retainedSurface = SkSurfaces::RenderTarget(
                grContext_.get(),
                skgpu::Budgeted::kYes,
                imageInfo,
                0,
                kTopLeft_GrSurfaceOrigin,
                nullptr);
            GrGLFramebufferInfo fbInfo;
            fbInfo.fFBOID = 0;
            fbInfo.fFormat = GL_RGBA8;
            auto backendRT = GrBackendRenderTargets::MakeGL(surfaceWidth, surfaceHeight, 0, 0, fbInfo);
            auto windowSurface = SkSurfaces::WrapBackendRenderTarget(
                grContext_.get(), backendRT, kBottomLeft_GrSurfaceOrigin,
                kBGRA_8888_SkColorType, nullptr, nullptr);
            if (retainedSurface && windowSurface) {
                paintSurface_ = std::move(retainedSurface);
                windowSurface_ = std::move(windowSurface);
                paintSurfaceWidth_ = surfaceWidth;
                paintSurfaceHeight_ = surfaceHeight;
                setRenderBackend(RenderBackend::OpenGL);
                rendererInfo_.reason.clear();
                rendererInfo_.vsync = swapIntervalEnabled_;
                return true;
            }
            rendererInfo_.reason = !retainedSurface ? "retained-surface-failed" : "window-surface-failed";
            traceGpu(rendererInfo_.reason.c_str());
            paintSurface_.reset();
            windowSurface_.reset();
            gpuAvailable_ = false;
            swapIntervalEnabled_ = false;
        }
        // 走到这里说明无表面或容量不足，必须重建光栅表面。
        // 旧逻辑仅在表面为空时新建：容量不足时沿用小表面、记账尺寸却改成大的，
        // 后续 blit 按大尺寸读小缓冲越界崩溃（窗口最大化时交互重绘先于延迟全绘触发，必现）。
        paintSurface_ = SkSurfaces::Raster(imageInfo);
        setRenderBackend(paintSurface_ ? RenderBackend::Software : RenderBackend::Unknown);
        rendererInfo_.vsync = false;
        if (!paintSurface_) rendererInfo_.reason = "raster-surface-failed";
        paintSurfaceWidth_ = paintSurface_ ? surfaceWidth : 0;
        paintSurfaceHeight_ = paintSurface_ ? surfaceHeight : 0;
        return true;
    }
