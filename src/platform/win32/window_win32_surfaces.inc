    struct PaintSurfaceUpdate {
        bool repaint;
        bool allocatedBacking;
    };

    static int alignedPaintSurfaceSize(int requested) {
        const int padded = std::max(1, requested + kPaintSurfaceGrowthPadding);
        return ((padded + kPaintSurfaceAlignment - 1) / kPaintSurfaceAlignment) * kPaintSurfaceAlignment;
    }

    static bool reuseResizeBacking() {
        static const bool enabled = [] {
            const auto* value = std::getenv("ONEUI_WIN32_REUSE_RESIZE_TARGET");
            return !value || std::string_view(value) != "0";
        }();
        return enabled;
    }

    static int alignedGpuPaintExtent(int requested, int limit) {
        if (requested > limit) return requested;
        const auto blocks = (static_cast<std::int64_t>(requested) + kPaintSurfaceAlignment - 1)
            / kPaintSurfaceAlignment;
        return static_cast<int>(std::min<std::int64_t>(limit, blocks * kPaintSurfaceAlignment));
    }

    sk_sp<SkSurface> makeRetainedGpuSurface(int width, int height) {
        const auto info = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        return SkSurfaces::RenderTarget(grContext_.get(), skgpu::Budgeted::kYes, info, 0,
            kTopLeft_GrSurfaceOrigin, nullptr);
    }

    PaintSurfaceUpdate preparePaintSurfaces(int width, int height) {
        const bool useGpu = gpuAvailable_ && grContext_;
        if (useGpu && paintSurface_ && windowSurface_ && windowSurface_->width() == width &&
            windowSurface_->height() == height) return {};

        int bufferWidth = width;
        int bufferHeight = height;
        if (useGpu && angleContext_) {
            const bool changingDrawable = windowSurface_ != nullptr;
            if (!angleContext_->refreshBufferSize(bufferWidth, bufferHeight, changingDrawable)) {
                shutdownGPU();
                rendererInfo_.reason = "angle-surface-query-failed";
            } else if (changingDrawable) {
                // Resizing ANGLE's EGL surface can rebuild its underlying target.
                grContext_->resetContext();
            }
        }

        if (gpuAvailable_ && grContext_) {
            const bool liveReuse = interactiveResizeActive_ && reuseResizeBacking();
            const bool fits = paintSurface_ && windowSurface_ &&
                paintSurfaceWidth_ >= width && paintSurfaceHeight_ >= height;
            // Keep at most twice the visible area after a shrink. An exact
            // drawable is still required for the default framebuffer.
            const auto area = static_cast<std::uint64_t>(width) * height;
            const auto retainedArea = static_cast<std::uint64_t>(paintSurfaceWidth_) * paintSurfaceHeight_;
            const bool sameAnglePaintSize = angleContext_ && paintSurface_ &&
                paintSurfaceWidth_ == width && paintSurfaceHeight_ == height;
            const bool keepBacking = sameAnglePaintSize || (liveReuse && fits && retainedArea <= 2 * area);
            auto retained = keepBacking ? paintSurface_ : sk_sp<SkSurface>{};
            if (!keepBacking) {
                const int limit = liveReuse && !fits ? grContext_->maxRenderTargetSize() : 0;
                const int backingWidth = alignedGpuPaintExtent(width, limit);
                const int backingHeight = alignedGpuPaintExtent(height, limit);
                retained = makeRetainedGpuSurface(backingWidth, backingHeight);
                if (!retained && (backingWidth != width || backingHeight != height))
                    retained = makeRetainedGpuSurface(width, height);
            }

            GrGLFramebufferInfo fbInfo;
            fbInfo.fFBOID = 0;
            fbInfo.fFormat = GL_RGBA8;
            const int stencilBits = angleContext_ ? angleContext_->stencilBits() : 0;
            const auto colorType = angleContext_ ? kRGBA_8888_SkColorType : kBGRA_8888_SkColorType;
            const auto target = GrBackendRenderTargets::MakeGL(bufferWidth, bufferHeight, 0, stencilBits, fbInfo);
            // Skia must use the opposite origin when ANGLE skips its
            // vertical flip and draws directly into the D3D11 back buffer.
            const auto origin = angleContext_ && angleContext_->invertedSurface() ?
                kTopLeft_GrSurfaceOrigin : kBottomLeft_GrSurfaceOrigin;
            auto drawable = SkSurfaces::WrapBackendRenderTarget(grContext_.get(), target, origin,
                colorType, nullptr, nullptr);
            if (retained && drawable) {
                paintSurfaceWidth_ = retained->width();
                paintSurfaceHeight_ = retained->height();
                paintSurface_ = std::move(retained);
                windowSurface_ = std::move(drawable);
                setRenderBackend(RenderBackend::OpenGL);
                rendererInfo_.reason.clear();
                rendererInfo_.vsync = swapIntervalEnabled_;
                return {true, !keepBacking};
            }
            rendererInfo_.reason = !retained ? "retained-surface-failed" : "window-surface-failed";
            traceGpu(rendererInfo_.reason.c_str());
            if (angleContext_) {
                retained.reset();
                drawable.reset();
                shutdownGPU();
            }
            paintSurface_.reset();
            windowSurface_.reset();
            gpuAvailable_ = false;
            swapIntervalEnabled_ = false;
        }

        if (!gpuAvailable_ && paintSurface_ &&
            paintSurfaceWidth_ >= width && paintSurfaceHeight_ >= height) return {};

        const int surfaceWidth = useGpu ? width : alignedPaintSurfaceSize(width);
        const int surfaceHeight = useGpu ? height : alignedPaintSurfaceSize(height);
        const auto imageInfo = SkImageInfo::Make(surfaceWidth, surfaceHeight,
            kBGRA_8888_SkColorType, kPremul_SkAlphaType);
        // Reallocate before the blit reads pixels at the new client size.
        paintSurface_ = SkSurfaces::Raster(imageInfo);
        setRenderBackend(paintSurface_ ? RenderBackend::Software : RenderBackend::Unknown);
        rendererInfo_.vsync = false;
        if (!paintSurface_) rendererInfo_.reason = "raster-surface-failed";
        paintSurfaceWidth_ = paintSurface_ ? surfaceWidth : 0;
        paintSurfaceHeight_ = paintSurface_ ? surfaceHeight : 0;
        return {true, true};
    }
