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
        const auto inputLayout = inputLayout_;
        internal::InputLayoutSnapshot::PaintScope inputPaint(*inputLayout);
        const auto interactionPaint = internal::beginInteractionPaint(reinterpret_cast<std::uintptr_t>(hwnd_));
        const double paintStartMs = currentTimeMs();
        const bool traceLayout = interactionPaint.paintSequence != 0 && internal::scrollTraceEnabled();
        using PaintTrace = internal::Win32PaintTrace;
        PaintTrace paintTrace(traceLayout);
#if ONEUI_DEVELOPER_TOOLS
        const auto diagnostics=developerTools_; // Safe if application disables tools during paint.
        internal::DiagnosticSession diagnosticSession(diagnostics.get());
        internal::FrameProfileSession profile(renderTraceEnabled_ || bool(diagnostics) || traceLayout);
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
        internal::FrameProfileSession profile(renderTraceEnabled_ || traceLayout);
        const auto finishDiagnostics=[] {};
#endif
        const auto resizeGeneration = interactiveResizeActive_ ? liveResizeGeneration_ : 0;
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
        paintTrace.mark(PaintTrace::GlSetup);
        PAINTSTRUCT paintStruct{};
        HDC paintDc = BeginPaint(hwnd_, &paintStruct);
        paintTrace.mark(PaintTrace::NativePaint);
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
        const bool threadedGl = glWorker_.ready() && !IsRectEmpty(&dirtyRect);
        if (threadedGl && workerRecordingBlocked()) {
            // Exposure paints can arrive without going through queueInvalidation.
            // Validate the native region and keep its damage until the worker takes the pending scene.
            mergeInvalidation(&dirtyRect);
            EndPaint(hwnd_, &paintStruct);
            internal::traceInteractionPaint(interactionPaint, "worker_record_deferred", "opengl-worker");
            internal::traceInteractionPaint(interactionPaint, "paint_end", "opengl-worker", 0);
            finishDiagnostics();
            return;
        }
        if (threadedGl && win32::experimentalGlWorkerPacesRecording() && invalidationPending_) {
            // The worker can take the pending scene between submitInvalidations and BeginPaint.
            if (invalidateAll_) dirtyRect = clientRect;
            else UnionRect(&dirtyRect, &dirtyRect, &pendingInvalidation_);
            invalidationPending_ = false;
            invalidateAll_ = false;
        }
        const float scale = normalizedDpiScale();
        const float logicalWidth = static_cast<float>(width) / scale;
        const float logicalHeight = static_cast<float>(height) / scale;

        const double surfaceStartMs = currentTimeMs();
        const auto surfaceUpdate = threadedGl ? PaintSurfaceUpdate{} : preparePaintSurfaces(width, height);
        const double surfaceMs = currentTimeMs() - surfaceStartMs;
        paintTrace.mark(PaintTrace::Surface);
        if (traceLayout && surfaceUpdate.allocatedBacking) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "paint_backing_store", interactionPaint.window, 0, 0, 0, 0, 0, 0,
                surfaceMs, static_cast<double>(interactionPaint.paintSequence),
                static_cast<double>(paintSurfaceWidth_) * paintSurfaceHeight_});
        }

        if (!threadedGl && !paintSurface_) {
            EndPaint(hwnd_, &paintStruct);
            internal::traceInteractionPaint(interactionPaint, "paint_end", "unknown", 0);
            return;
        }
        win32::GlRenderWorker::Frame workerFrame;
        if (threadedGl) {
            workerFrame = glWorker_.plan(width, height, dirtyRect, interactiveResizeActive_);
            dirtyRect = workerFrame.damage;
        } else if (surfaceUpdate.repaint) {
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
            internal::traceInteractionPaint(interactionPaint, "paint_end", "unknown", 0);
            return;
        }

        SkCanvas* skCanvas = threadedGl ? nullptr : paintSurface_->getCanvas();
        const bool fullPaint = dirtyX == 0 && dirtyY == 0 && dirtyWidth == width && dirtyHeight == height;
        const bool captureWorkerStartup = !useGPU && fullPaint && IsWindowVisible(hwnd_) &&
            ((glWorkerWaitingForSoftwareFrame_ && win32::experimentalGlWorkerPrewarm()) ||
             glWorker_.wantsStartupPicture());
        std::optional<SkPictureRecorder> recorder;
        if (threadedGl || captureWorkerStartup ||
            (!useGPU && fullPaint && IsWindowVisible(hwnd_) && deferredStartupGl_.shouldCapture())) {
            recorder.emplace();
            skCanvas = recorder->beginRecording(SkRect::MakeWH(static_cast<float>(width), static_cast<float>(height)));
        }
        if (threadedGl && !fullPaint) {
            skCanvas->clipRect(SkRect::MakeLTRB(static_cast<float>(dirtyX), static_cast<float>(dirtyY),
                static_cast<float>(dirtyRight), static_cast<float>(dirtyBottom)));
        }
        if (fullPaint) {
            const auto background = colors::Surface;
            skCanvas->clear(SkColorSetARGB(background.a, background.r, background.g, background.b));
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

        const auto recordResizePresentation = [&] {
            if (fullPaint && interactiveResizeActive_ && resizeGeneration == liveResizeGeneration_) {
                presentedResizeGeneration_ = resizeGeneration;
            }
        };
        paintTrace.mark(PaintTrace::Canvas);
        if (content_ && content_->visible()) {
            const double contentStartMs = currentTimeMs();
            g_primitivePaintTrace = PrimitivePaintTrace{};
            text::AtlasWarmupCapture atlasCapture(textAtlasWarmup_.capture(
                useGPU && nvidiaDriver_ && fullPaint && !interactiveResizeActive_));
            paintClientContent(canvas, logicalWidth, logicalHeight);
            recordContentPaint(currentTimeMs() - contentStartMs);
            recordPrimitivePaint(g_primitivePaintTrace);
            for (std::size_t i = 0; i < traceStageMs_.size(); ++i) traceStageMs_[i] += profile.data.ms[i];
        }
        paintTrace.mark(PaintTrace::Tree);

        if (!fullPaint) {
            canvas.restore();
        }
#if ONEUI_DEVELOPER_TOOLS
        if(diagnostics && diagnostics->options.overlay)paintDeveloperOverlay(canvas,*diagnostics,logicalWidth,logicalHeight);
#endif
        skCanvas->restore();
        sk_sp<SkPicture> startupPicture;
        if (recorder) {
            canvasOwner.reset();
            startupPicture = recorder->finishRecordingAsPicture();
            if (!threadedGl) paintSurface_->getCanvas()->drawPicture(startupPicture);
        }
        paintTrace.mark(PaintTrace::End);

        if (threadedGl) {
            internal::traceInteractionPaint(interactionPaint, "worker_scene_ready", "opengl-worker");
            EndPaint(hwnd_, &paintStruct);
            const bool accepted = glWorker_.enqueue(std::move(startupPicture), workerFrame, interactionPaint);
            if (accepted) {
                setRenderBackend(RenderBackend::OpenGL);
                rendererInfo_.reason.clear();
                rendererInfo_.vsync = glWorker_.vsync();
                ++rendererInfo_.paints;
                rendererInfo_.paintMs += currentTimeMs() - paintStartMs;
            } else {
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            flushDeferredImePosition();
            finishDiagnostics();
            internal::traceInteractionPaint(interactionPaint, "paint_end", "opengl-worker", accepted ? 1 : 0);
            paintTrace.write(interactionPaint.window, static_cast<double>(interactionPaint.paintSequence),
                RECT{dirtyX, dirtyY, dirtyRight, dirtyBottom}, profile.data);
            return;
        }

        internal::traceInteractionPaint(interactionPaint, "content_paint_end",
            gpuAvailable_ && grContext_ && windowSurface_ ? "opengl" : "software");
        if (traceLayout) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "paint_layout", interactionPaint.window, 0, 0, 0, 0, 0, 0,
                profile.data.ms[static_cast<size_t>(internal::FrameStage::Layout)],
                static_cast<double>(interactionPaint.paintSequence),
                profile.data.ms[static_cast<size_t>(internal::FrameStage::TextLayout)]});
        }

        if (gpuAvailable_ && grContext_ && windowSurface_) {
            internal::traceInteractionPaint(interactionPaint, "submit_begin", "opengl");
            const double submitStartMs = currentTimeMs();
            SkCanvas* presentCanvas = windowSurface_->getCanvas();
            presentCanvas->clear(SK_ColorBLACK);
            paintSurface_->draw(presentCanvas, 0.0f, 0.0f);
            const double copyEndMs = traceLayout ? currentTimeMs() : 0.0;
            const bool traceGl = traceLayout && internal::win32GlTraceEnabled();
            std::optional<internal::GlCallTrace> glTrace;
            std::optional<internal::GlCallTrace> glSubmitTrace;
            ULONG64 cyclesBeforeFlush = 0, cyclesAfterFlush = 0;
            ULONG64 cyclesBeforeSubmit = 0, cyclesAfterSubmit = 0;
            if (traceGl) {
                const internal::GlCallTrace::Scope scope(&glTrace.emplace());
                QueryThreadCycleTime(GetCurrentThread(), &cyclesBeforeFlush);
                grContext_->flush();
                QueryThreadCycleTime(GetCurrentThread(), &cyclesAfterFlush);
            } else {
                grContext_->flush();
            }
            const double flushEndMs = traceLayout ? currentTimeMs() : 0.0;
            if (traceGl) {
                const internal::GlCallTrace::Scope scope(&glSubmitTrace.emplace());
                QueryThreadCycleTime(GetCurrentThread(), &cyclesBeforeSubmit);
                grContext_->submit(GrSyncCpu::kNo);
                QueryThreadCycleTime(GetCurrentThread(), &cyclesAfterSubmit);
            } else {
                grContext_->submit(GrSyncCpu::kNo);
            }
            const double submitEndMs = traceLayout ? currentTimeMs() : 0.0;
            internal::traceInteractionPaint(interactionPaint, "submit_end", "opengl");
            const double presentStarted = currentTimeMs();
            internal::traceInteractionPaint(interactionPaint, "swap_begin", "opengl");
            const BOOL swapped = SwapBuffers(glDC_);
            internal::traceInteractionPaint(interactionPaint, "swap_return", "opengl", swapped ? 1 : 0);
            internal::workspacePresentMs = currentTimeMs() - presentStarted;
            if (traceLayout) {
                const double sequence = static_cast<double>(interactionPaint.paintSequence);
                const auto recordGpuPhase = [&](const char* phase, double elapsed, double extra = 0.0) {
                    internal::writeScrollTrace(internal::ScrollTraceEvent{
                        "win32", phase, interactionPaint.window, 0, 0, 0, 0, 0, 0, elapsed, sequence, extra});
                };
                recordGpuPhase("paint_gpu_copy", copyEndMs - submitStartMs);
                recordGpuPhase("paint_gpu_flush", flushEndMs - copyEndMs);
                if (traceGl) {
                    if (cyclesBeforeFlush && cyclesAfterFlush >= cyclesBeforeFlush)
                        recordGpuPhase("paint_gpu_flush_cycles", 0.0,
                            static_cast<double>(cyclesAfterFlush - cyclesBeforeFlush));
                    glTrace->write(interactionPaint.window, sequence);
                    if (cyclesBeforeSubmit && cyclesAfterSubmit >= cyclesBeforeSubmit)
                        recordGpuPhase("paint_gpu_submit_cycles", 0.0,
                            static_cast<double>(cyclesAfterSubmit - cyclesBeforeSubmit));
                    glSubmitTrace->write(interactionPaint.window, sequence, true);
                }
                recordGpuPhase("paint_gpu_submit", submitEndMs - flushEndMs);
                recordGpuPhase("paint_shader_cache", profile.data.ms[static_cast<size_t>(internal::FrameStage::ShaderCacheLoad)],
                    profile.data.ms[static_cast<size_t>(internal::FrameStage::ShaderCacheStore)]);
            }
            if (swapped && nvidiaDriver_) {
                const double censusStarted = internal::workspaceFrameProbe || traceLayout ? currentTimeMs() : 0;
                const bool startupCensusReady = deferredGlCensusReady_ || gpuWarmup_.censusReady();
                if (!startupCensusReady) triggerDriverThreadCensus();
                if (censusStarted) {
                    const double censusMs = currentTimeMs() - censusStarted;
                    if (internal::workspaceFrameProbe) internal::workspaceCensusMs = censusMs;
                    if (traceLayout) {
                        internal::writeScrollTrace(internal::ScrollTraceEvent{
                            "win32", "paint_driver_census", interactionPaint.window, 0, 0, 0, 0, 0, 0,
                            censusMs, static_cast<double>(interactionPaint.paintSequence), startupCensusReady ? 1.0 : 0.0});
                    }
                }
            }
            const double submitMs = currentTimeMs() - submitStartMs;
            traceSubmitMs_ += submitMs;
            rendererInfo_.submitMs += submitMs;
            if (swapped) {
                recordResizePresentation();
                textAtlasWarmup_.arm(hwnd_);
                EndPaint(hwnd_, &paintStruct);
                recordPaint(width, height, fullPaint, surfaceUpdate.allocatedBacking, surfaceMs, currentTimeMs() - paintStartMs);
                flushDeferredImePosition();
                finishDiagnostics();
                internal::traceInteractionPaint(interactionPaint, "paint_end", "opengl", 1);
                paintTrace.write(interactionPaint.window, static_cast<double>(interactionPaint.paintSequence),
                    RECT{dirtyX, dirtyY, dirtyRight, dirtyBottom}, profile.data);
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
            internal::traceInteractionPaint(interactionPaint, "paint_end", "opengl", 0);
            return;
        }

        SkPixmap pixmap;
        bool blitSucceeded = false;
        if (paintSurface_->peekPixels(&pixmap)) {
            internal::traceInteractionPaint(interactionPaint, "blit_begin", "software");
            const double blitStartMs = currentTimeMs();
            if (fullPaint) {
                BITMAPINFO bitmapInfo{};
                bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bitmapInfo.bmiHeader.biWidth = paintSurfaceWidth_;
                bitmapInfo.bmiHeader.biHeight = -paintSurfaceHeight_;
                bitmapInfo.bmiHeader.biPlanes = 1;
                bitmapInfo.bmiHeader.biBitCount = 32;
                bitmapInfo.bmiHeader.biCompression = BI_RGB;

                const int copied = StretchDIBits(
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
                blitSucceeded = copied != 0 && copied != GDI_ERROR;
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

                blitSucceeded = SetDIBitsToDevice(
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
                    DIB_RGB_COLORS) != 0;
            }
            internal::traceInteractionPaint(interactionPaint, "blit_return", "software", blitSucceeded ? 1 : 0);
            recordBlit(currentTimeMs() - blitStartMs);
        }

        EndPaint(hwnd_, &paintStruct);
        if (blitSucceeded) recordResizePresentation();
        if (blitSucceeded && glWorkerWaitingForSoftwareFrame_ &&
            IsWindowVisible(hwnd_) && !IsIconic(hwnd_)) {
            glWorkerWaitingForSoftwareFrame_ = false;
            // Start WGL after the first successful visible software presentation.
            const bool started = captureWorkerStartup
                ? glWorker_.start(hwnd_, {std::move(startupPicture), width, height})
                : glWorker_.start(hwnd_);
            rendererInfo_.reason = started ? "opengl-worker-starting" : "opengl-worker-start-failed";
            internal::traceInteractionPaint(interactionPaint, "worker_start_after_software",
                "software", started ? 1 : 0);
        }
        if (blitSucceeded && startupPicture) {
            if (captureWorkerStartup) {
                glWorker_.setStartupPicture({std::move(startupPicture), width, height});
            } else {
                deferredStartupGl_.submitPicture(std::move(startupPicture), width, height);
            }
        }
        scheduleDeferredGlAdoption();
        recordPaint(width, height, fullPaint, surfaceUpdate.allocatedBacking, surfaceMs, currentTimeMs() - paintStartMs);
        flushDeferredImePosition();
        finishDiagnostics();
        internal::traceInteractionPaint(interactionPaint, "paint_end", "software", blitSucceeded ? 1 : 0);
        paintTrace.write(interactionPaint.window, static_cast<double>(interactionPaint.paintSequence),
            RECT{dirtyX, dirtyY, dirtyRight, dirtyBottom}, profile.data);
    }
