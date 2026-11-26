    bool startupTablePreparationEnabled() const {
        static const bool enabled = [] {
            wchar_t value[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_STARTUP_TABLE_YIELD", value, 2) == 1 && value[0] == L'1';
        }();
        return enabled && rendererInfo_.paints == 1 && !interactiveResizeActive_ &&
            !gpuAvailable_ && !glWorker_.ready() && win32::experimentalAngleWorkerEnabled();
    }

    bool enqueueWorkerPaint(sk_sp<SkPicture> picture, const win32::GlRenderWorker::Frame& frame,
                            const internal::InteractionSnapshot& trace) {
        internal::traceInteractionPaint(trace, "worker_scene_ready", "opengl-worker");
        if (!glWorker_.enqueue(std::move(picture), frame, trace)) {
            InvalidateRect(hwnd_, nullptr, FALSE);
            return false;
        }
        setRenderBackend(RenderBackend::OpenGL);
        rendererInfo_.reason.clear();
        rendererInfo_.vsync = glWorker_.vsync();
        return true;
    }

    // All client render destinations use the live widget tree, logical viewport,
    // DPI/content scale and tooltip state. Printing must not depend on whether
    // the compositor has presented the window, or on a test-only capture build.
    bool paintClientContent(Canvas& canvas, float width, float height) {
        if (!content_ || !content_->visible()) {
            return true;
        }
        if (clientContentPainting_) {
            return false;
        }
        struct PaintGuard {
            bool& active;
            explicit PaintGuard(bool& value) : active(value) { active = true; }
            ~PaintGuard() { active = false; }
        } guard(clientContentPainting_);
        traceLiveResizeDamage("resize_content_layout_begin");
        {
            ONEUI_VIEW_TIME(Layout);
            content_->setFrame(Rect{0.0f, 0.0f, width, height});
        }
        traceLiveResizeDamage("resize_content_layout_end");
        {
            internal::DiagnosticPaintSpan diagnostic(*content_);
            content_->paint(canvas);
        }
        paintTooltip(canvas);
        return true;
    }

    sk_sp<SkSurface> renderClientSurface(SkColor background, bool includeOverlay) {
        if (!hwnd_ || clientContentPainting_) {
            return nullptr;
        }
        const Size logical = clientSize();
        const Size physical = clientPixelSize();
        const int width = static_cast<int>(std::ceil(physical.width));
        const int height = static_cast<int>(std::ceil(physical.height));
        if (width <= 0 || height <= 0) {
            return nullptr;
        }
        auto surface = SkSurfaces::Raster(SkImageInfo::Make(
            width, height, kBGRA_8888_SkColorType, kPremul_SkAlphaType));
        if (!surface) {
            return nullptr;
        }
        auto* skCanvas = surface->getCanvas();
        skCanvas->clear(background);
        skCanvas->scale(normalizedDpiScale(), normalizedDpiScale());
        auto canvas = rendering::makeSkiaCanvas(*skCanvas, &defaultFontFamily_,
            Rect{0.0f, 0.0f, logical.width, logical.height});
        if (!paintClientContent(*canvas, logical.width, logical.height)) {
            return nullptr;
        }
#if ONEUI_DEVELOPER_TOOLS
        if (includeOverlay && developerTools_ && developerTools_->options.overlay) {
            paintDeveloperOverlay(*canvas, *developerTools_, logical.width, logical.height);
        }
#else
        (void)includeOverlay;
#endif
        return surface;
    }

    void printClient(HDC destination, LPARAM flags) {
        if (!destination || !hwnd_ ||
            ((flags & PRF_CHECKVISIBLE) && !IsWindowVisible(hwnd_))) {
            return;
        }
        const auto background = colors::Surface;
        auto surface = renderClientSurface(
            SkColorSetARGB(background.a, background.r, background.g, background.b), true);
        SkPixmap pixels;
        if (!surface || !surface->peekPixels(&pixels)) {
            return;
        }
        BITMAPINFO bitmap{};
        bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = surface->width();
        bitmap.bmiHeader.biHeight = -surface->height();
        bitmap.bmiHeader.biPlanes = 1;
        bitmap.bmiHeader.biBitCount = 32;
        bitmap.bmiHeader.biCompression = BI_RGB;
        // The caller owns the DC, its viewport origin, mapping and clip. GDI
        // applies those while copying the entire top-down BGRA client image.
        StretchDIBits(destination, 0, 0, surface->width(), surface->height(),
            0, 0, surface->width(), surface->height(), pixels.addr(), &bitmap,
            DIB_RGB_COLORS, SRCCOPY);
    }
