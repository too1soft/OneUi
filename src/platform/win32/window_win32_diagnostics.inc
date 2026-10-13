    void recordResizeMessage(WPARAM sizeType) {
        if (!renderTraceEnabled_) {
            return;
        }
        ++traceResizeMessages_;
        if (interactiveResizeActive_) {
            ++traceInteractiveResizeMessages_;
        }
        if (sizeType == SIZE_MAXIMIZED) {
            ++traceMaximizeMessages_;
        } else if (sizeType == SIZE_RESTORED) {
            ++traceRestoreMessages_;
        }
    }

    void scheduleClientSizeChanged() {
        if (!clientSizeChangedHandler_ || clientSizeChangedFramePending_ || !hwnd_) {
            return;
        }
        clientSizeChangedFramePending_ = true;
        requestAnimationFrame([this](double) {
            clientSizeChangedFramePending_ = false;
            if (clientSizeChangedHandler_) {
                clientSizeChangedHandler_(clientSize());
            }
        });
    }

    void recordContentPaint(double elapsedMs) {
        rendererInfo_.contentMs += elapsedMs;
        if (renderTraceEnabled_) {
            traceContentPaintMs_ += elapsedMs;
        }
    }

    void recordPrimitivePaint(const PrimitivePaintTrace& trace) {
        if (!renderTraceEnabled_) {
            return;
        }
        traceTextCalls_ += trace.textCalls;
        traceTextMeasureCalls_ += trace.textMeasureCalls;
        traceShadowCalls_ += trace.shadowCalls;
        traceGradientCalls_ += trace.gradientCalls;
        traceTextMs_ += trace.textMs;
        traceTextMeasureMs_ += trace.textMeasureMs;
        traceShadowMs_ += trace.shadowMs;
        traceGradientMs_ += trace.gradientMs;
    }

    void recordBlit(double elapsedMs) {
        rendererInfo_.blitMs += elapsedMs;
        if (renderTraceEnabled_) {
            traceBlitMs_ += elapsedMs;
        }
    }

    void recordPaint(int width, int height, bool fullPaint, bool allocatedSurface, double surfaceMs, double elapsedMs) {
        internal::recordWorkspaceFrame(elapsedMs);
        ++rendererInfo_.paints;
        rendererInfo_.paintMs += elapsedMs;
        const double nowMs = currentTimeMs();
        double frameIntervalMs = 0.0;
        if (internal::scrollTraceEnabled()) {
            const double traceNowMs = internal::scrollTraceNowMs();
            frameIntervalMs = scrollTraceLastPaintMs_ > 0.0
                ? traceNowMs - scrollTraceLastPaintMs_
                : 0.0;
            scrollTraceLastPaintMs_ = traceNowMs;
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "win32", "paint_presented", reinterpret_cast<std::uintptr_t>(hwnd_),
                0.0, 0.0, 0.0, 0.0, frameIntervalMs, 0.0, elapsedMs,
                fullPaint ? 1.0 : 0.0,
                static_cast<double>(width) * static_cast<double>(height)});
        }
        if (!renderTraceEnabled_) {
            return;
        }
        if (traceLastPaintMs_ > 0.0) {
            const double renderFrameIntervalMs = nowMs - traceLastPaintMs_;
            if (renderFrameIntervalMs > 0.0 && renderFrameIntervalMs < 250.0) {
                traceFrameIntervalMs_ += renderFrameIntervalMs;
                ++traceFrameIntervals_;
                if (traceMinFrameIntervalMs_ == 0.0 || renderFrameIntervalMs < traceMinFrameIntervalMs_) {
                    traceMinFrameIntervalMs_ = renderFrameIntervalMs;
                }
                traceMaxFrameIntervalMs_ = std::max(traceMaxFrameIntervalMs_, renderFrameIntervalMs);
            }
        }
        traceLastPaintMs_ = nowMs;
        ++tracePaints_;
        if (fullPaint) {
            ++traceFullPaints_;
        } else {
            ++tracePartialPaints_;
        }
        if (allocatedSurface) {
            ++traceSurfaceAllocations_;
            traceSurfaceAllocMs_ += surfaceMs;
        }
        tracePaintMs_ += elapsedMs;
        traceLastWidth_ = width;
        traceLastHeight_ = height;
        printRenderTrace(false);
    }

    void printRenderTrace(bool force) {
        if (!renderTraceEnabled_) {
            return;
        }
        const double nowMs = currentTimeMs();
        if (traceLastPrintMs_ == 0.0) {
            traceLastPrintMs_ = nowMs;
            if (!force) {
                return;
            }
        }
        if (!force && nowMs - traceLastPrintMs_ < 1000.0) {
            return;
        }
        if (traceResizeMessages_ == 0 && tracePaints_ == 0 && traceSurfaceAllocations_ == 0) {
            return;
        }

        const double paintAvg = tracePaints_ > 0 ? tracePaintMs_ / static_cast<double>(tracePaints_) : 0.0;
        const double contentAvg = tracePaints_ > 0 ? traceContentPaintMs_ / static_cast<double>(tracePaints_) : 0.0;
        const double blitAvg = tracePaints_ > 0 ? traceBlitMs_ / static_cast<double>(tracePaints_) : 0.0;
        const double frameIntervalAvg = traceFrameIntervals_ > 0
            ? traceFrameIntervalMs_ / static_cast<double>(traceFrameIntervals_)
            : 0.0;
        const double presentedFps = frameIntervalAvg > 0.0 ? 1000.0 / frameIntervalAvg : 0.0;
        char line[2048]{};
        std::snprintf(
            line,
            sizeof(line),
            "[oneui-render] size=%dx%d wm_size=%llu interactive_size=%llu max=%llu restore=%llu paints=%llu full=%llu partial=%llu frame=%.2fms/%.1ffps min=%.2fms max=%.2fms surface_alloc=%llu avg_paint=%.2fms avg_content=%.2fms avg_blit=%.2fms surface_alloc_ms=%.2f text=%llu/%.2fms measure=%llu/%.2fms shadow=%llu/%.2fms gradient=%llu/%.2fms backend=%s avg_layout=%.3fms avg_text_layout=%.3fms avg_fill=%.3fms avg_path=%.3fms avg_submit=%.3fms paint_invalidations=%llu\n",
            traceLastWidth_,
            traceLastHeight_,
            static_cast<unsigned long long>(traceResizeMessages_),
            static_cast<unsigned long long>(traceInteractiveResizeMessages_),
            static_cast<unsigned long long>(traceMaximizeMessages_),
            static_cast<unsigned long long>(traceRestoreMessages_),
            static_cast<unsigned long long>(tracePaints_),
            static_cast<unsigned long long>(traceFullPaints_),
            static_cast<unsigned long long>(tracePartialPaints_),
            frameIntervalAvg,
            presentedFps,
            traceMinFrameIntervalMs_,
            traceMaxFrameIntervalMs_,
            static_cast<unsigned long long>(traceSurfaceAllocations_),
            paintAvg,
            contentAvg,
            blitAvg,
            traceSurfaceAllocMs_,
            static_cast<unsigned long long>(traceTextCalls_),
            traceTextMs_,
            static_cast<unsigned long long>(traceTextMeasureCalls_),
            traceTextMeasureMs_,
            static_cast<unsigned long long>(traceShadowCalls_),
            traceShadowMs_,
            static_cast<unsigned long long>(traceGradientCalls_),
            traceGradientMs_,
            gpuAvailable_ ? "opengl" : "raster",
            traceStageMs_[0] / std::max<std::uint64_t>(1, tracePaints_),
            traceStageMs_[1] / std::max<std::uint64_t>(1, tracePaints_),
            traceStageMs_[2] / std::max<std::uint64_t>(1, tracePaints_),
            traceStageMs_[3] / std::max<std::uint64_t>(1, tracePaints_),
            traceSubmitMs_ / std::max<std::uint64_t>(1, tracePaints_),
            static_cast<unsigned long long>(tracePaintInvalidations_));
        if (!renderTraceFilePath_.empty()) {
            if (FILE* file = _wfopen(renderTraceFilePath_.c_str(), L"ab")) {
                std::fputs(line, file);
                std::fclose(file);
            }
        } else {
            std::fputs(line, stderr);
            std::fflush(stderr);
        }

        traceLastPrintMs_ = nowMs;
        traceResizeMessages_ = 0;
        traceInteractiveResizeMessages_ = 0;
        traceMaximizeMessages_ = 0;
        traceRestoreMessages_ = 0;
        tracePaints_ = 0;
        traceFullPaints_ = 0;
        tracePartialPaints_ = 0;
        traceFrameIntervals_ = 0;
        traceFrameIntervalMs_ = 0.0;
        traceMinFrameIntervalMs_ = 0.0;
        traceMaxFrameIntervalMs_ = 0.0;
        traceSurfaceAllocations_ = 0;
        tracePaintMs_ = 0.0;
        traceContentPaintMs_ = 0.0;
        traceBlitMs_ = 0.0;
        traceSurfaceAllocMs_ = 0.0;
        traceTextCalls_ = 0;
        traceTextMeasureCalls_ = 0;
        traceShadowCalls_ = 0;
        traceGradientCalls_ = 0;
        traceTextMs_ = 0.0;
        traceTextMeasureMs_ = 0.0;
        traceShadowMs_ = 0.0;
        traceGradientMs_ = 0.0;
        traceStageMs_.fill(0);
        traceSubmitMs_ = 0;
        tracePaintInvalidations_ = 0;
    }
