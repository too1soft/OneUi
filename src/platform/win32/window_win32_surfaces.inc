    struct PaintSurfaceUpdate {
        bool repaint;
        bool allocatedBacking;
    };

    static int alignedPaintSurfaceSize(int requested) {
        const int padded = std::max(1, requested + kPaintSurfaceGrowthPadding);
        constexpr int alignment = win32::paintSurfaceAlignment;
        return ((padded + alignment - 1) / alignment) * alignment;
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
            const bool sameAnglePaintSize = angleContext_ && paintSurface_ &&
                paintSurfaceWidth_ == width && paintSurfaceHeight_ == height;
            auto backing = sameAnglePaintSize ? win32::RetainedGpuBacking{paintSurface_, true}
                : win32::resizeGpuBacking(*grContext_, windowSurface_ ? paintSurface_.get() : nullptr,
                                          width, height, interactiveResizeActive_);
            auto& retained = backing.surface;

            GrGLFramebufferInfo fbInfo;
            fbInfo.fFBOID = 0;
            fbInfo.fFormat = GL_RGBA8;
            const int stencilBits = angleContext_ ? angleContext_->stencilBits() : 0;
            const auto colorType = angleContext_ ? kRGBA_8888_SkColorType : kBGRA_8888_SkColorType;
            const auto target = GrBackendRenderTargets::MakeGL(bufferWidth, bufferHeight, 0, stencilBits, fbInfo);
            auto drawable = SkSurfaces::WrapBackendRenderTarget(grContext_.get(), target, kBottomLeft_GrSurfaceOrigin,
                colorType, nullptr, nullptr);
            if (retained && drawable) {
                paintSurfaceWidth_ = retained->width();
                paintSurfaceHeight_ = retained->height();
                paintSurface_ = std::move(retained);
                windowSurface_ = std::move(drawable);
                setRenderBackend(RenderBackend::OpenGL);
                rendererInfo_.reason.clear();
                rendererInfo_.vsync = swapIntervalEnabled_;
                return {true, !backing.reused};
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
