    void requestRedraw() override {
        if (internal::activeFrameProfile) ++tracePaintInvalidations_;
        if (hwnd_) {
            queueInvalidation(nullptr);
        }
    }

    // Widget invalidations arrive by the hundred when a pane remounts, and
    // each InvalidateRect is a win32k call that can wait on the session-wide
    // USER lock (20-50 ms observed while a new console host started). They
    // are merged here and submitted once per message turn; anything that
    // paints or reads the update region submits the pending rect first.
    void queueInvalidation(const RECT* rect) {
        if (rect) {
            if (!invalidateAll_) {
                if (invalidationPending_) {
                    UnionRect(&pendingInvalidation_, &pendingInvalidation_, rect);
                } else {
                    pendingInvalidation_ = *rect;
                }
            }
        } else {
            invalidateAll_ = true;
        }
        if (!invalidationPending_) {
            invalidationPending_ = true;
            if (!PostMessageW(hwnd_, kOneUiFlushInvalidations, 0, 0)) {
                submitInvalidations();
            }
        }
    }

    void submitInvalidations() {
        if (!invalidationPending_) {
            return;
        }
        invalidationPending_ = false;
        const bool all = invalidateAll_;
        invalidateAll_ = false;
        if (hwnd_) {
            InvalidateRect(hwnd_, all ? nullptr : &pendingInvalidation_, FALSE);
        }
    }

    void prepareLayoutSnapshot() override {
        if (!hwnd_ || !content_ || !content_->visible()) {
            return;
        }
        const Size logical = clientSize();
        const Size physical = clientPixelSize();
        const int width = std::max(1, static_cast<int>(std::ceil(physical.width)));
        const int height = std::max(1, static_cast<int>(std::ceil(physical.height)));
        auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
        if (!surface) {
            return;
        }
        SkCanvas* skCanvas = surface->getCanvas();
        skCanvas->scale(normalizedDpiScale(), normalizedDpiScale());
        auto canvasOwner = rendering::makeSkiaCanvas(
            *skCanvas,
            &defaultFontFamily_,
            Rect{0.0f, 0.0f, logical.width, logical.height});
        auto& canvas = *canvasOwner;
        content_->setFrame(Rect{0.0f, 0.0f, logical.width, logical.height});
        content_->paint(canvas);
        paintTooltip(canvas);
    }

#ifdef ONEUI_ENABLE_TEST_FRAME_CAPTURE
    bool captureFramePng(const std::wstring& path) override {
        if (!hwnd_ || !content_ || !content_->visible() || path.empty()) {
            return false;
        }
        const Size logical = clientSize();
        const Size physical = clientPixelSize();
        const int width = std::max(1, static_cast<int>(std::ceil(physical.width)));
        const int height = std::max(1, static_cast<int>(std::ceil(physical.height)));
        auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
        if (!surface) return false;

        SkCanvas* skCanvas = surface->getCanvas();
        skCanvas->clear(SK_ColorTRANSPARENT);
        skCanvas->scale(normalizedDpiScale(), normalizedDpiScale());
        auto canvasOwner = rendering::makeSkiaCanvas(
            *skCanvas,
            &defaultFontFamily_,
            Rect{0.0f, 0.0f, logical.width, logical.height});
        auto& canvas = *canvasOwner;
        content_->setFrame(Rect{0.0f, 0.0f, logical.width, logical.height});
        content_->paint(canvas);
        paintTooltip(canvas);
#if ONEUI_DEVELOPER_TOOLS
        if(developerTools_ && developerTools_->options.overlay)paintDeveloperOverlay(canvas,*developerTools_,logical.width,logical.height);
#endif
        SkPixmap pixels;
        if (!surface->peekPixels(&pixels)) return false;

        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        IWICImagingFactory* factory = nullptr;
        IWICBitmapEncoder* encoder = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapFrameEncode* frame = nullptr;
        IPropertyBag2* properties = nullptr;
        HRESULT result = CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));
        if (SUCCEEDED(result)) result = factory->CreateStream(&stream);
        if (SUCCEEDED(result)) result = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
        if (SUCCEEDED(result)) result = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
        if (SUCCEEDED(result)) result = encoder->Initialize(stream, WICBitmapEncoderNoCache);
        if (SUCCEEDED(result)) result = encoder->CreateNewFrame(&frame, &properties);
        if (SUCCEEDED(result)) result = frame->Initialize(properties);
        if (SUCCEEDED(result)) result = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
        if (SUCCEEDED(result)) result = frame->SetPixelFormat(&format);
        const auto stride = static_cast<UINT>(pixels.rowBytes());
        const auto byteCount = static_cast<UINT>(pixels.rowBytes() * static_cast<std::size_t>(height));
        if (SUCCEEDED(result)) {
            result = frame->WritePixels(
                static_cast<UINT>(height),
                stride,
                byteCount,
                static_cast<BYTE*>(pixels.writable_addr()));
        }
        if (SUCCEEDED(result)) result = frame->Commit();
        if (SUCCEEDED(result)) result = encoder->Commit();
        if (properties) properties->Release();
        if (frame) frame->Release();
        if (encoder) encoder->Release();
        if (stream) stream->Release();
        if (factory) factory->Release();
        if (SUCCEEDED(initialized)) CoUninitialize();
        return SUCCEEDED(result);
    }
#endif

    bool post(std::function<void()> callback) override {
        if (!callback || !acceptingPostedCallbacks_.load(std::memory_order_acquire)) {
            return false;
        }

        ensureCreated();
        if (!hwnd_ || !acceptingPostedCallbacks_.load(std::memory_order_acquire)) {
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(postedCallbacksMutex_);
            if (!acceptingPostedCallbacks_.load(std::memory_order_relaxed)) {
                return false;
            }
            postedCallbacks_.push(std::move(callback));
        }
        if (!PostMessageW(hwnd_, kOneUiRunPostedCallbacks, 0, 0)) {
            acceptingPostedCallbacks_.store(false, std::memory_order_release);
            discardPostedCallbacks();
            return false;
        }
        return true;
    }

    void requestAnimationFrame(std::function<void(double)> callback) override {
        if (!callback) {
            return;
        }

        ensureCreated();
        {
            std::lock_guard<std::mutex> lock(animationCallbacksMutex_);
            animationCallbacks_.push(std::move(callback));
        }
        if (animationFramePending_) {
            return;
        }

        animationFramePending_ = true;
        if (armAnimationFrameTimer()) {
            return;
        }
        if (SetTimer(hwnd_, kOneUiAnimationTimer, kOneUiAnimationFrameIntervalMs, nullptr)) {
            animationTimerActive_ = true;
        } else {
            animationFramePending_ = false;
        }
    }

    NativeWindowHandle nativeHandle() const override {
        return hwnd_;
    }

    Size clientSize() const override {
        if (!hwnd_) {
            return Size{static_cast<float>(options_.width), static_cast<float>(options_.height)};
        }

        RECT rect{};
        if (!GetClientRect(hwnd_, &rect)) {
            return {};
        }
        const float scale = normalizedDpiScale();
        return Size{
            static_cast<float>(rect.right - rect.left) / scale,
            static_cast<float>(rect.bottom - rect.top) / scale};
    }

    Size clientPixelSize() const override {
        if (!hwnd_) {
            return Size{
                static_cast<float>(logicalToPhysicalCeil(static_cast<float>(options_.width))),
                static_cast<float>(logicalToPhysicalCeil(static_cast<float>(options_.height)))};
        }

        RECT rect{};
        if (!GetClientRect(hwnd_, &rect)) {
            return {};
        }
        return Size{static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top)};
    }

    float dpiScale() const override {
        return normalizedDpiScale();
    }
