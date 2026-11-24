    void traceGpu(const char* stage) {
        if (!renderTraceEnabled_) return;
        const auto* renderer = wglGetCurrentContext() ? glGetString(GL_RENDERER) : nullptr;
        const auto* version = wglGetCurrentContext() ? glGetString(GL_VERSION) : nullptr;
        FILE* file = renderTraceFilePath_.empty() ? stderr : _wfopen(renderTraceFilePath_.c_str(), L"ab");
        if (!file) return;
        std::fprintf(file, "[oneui-gpu] stage=%s renderer=%s version=%s\n", stage,
            renderer ? reinterpret_cast<const char*>(renderer) : "none",
            version ? reinterpret_cast<const char*>(version) : "none");
        if (file != stderr) std::fclose(file); else std::fflush(file);
    }

    void setRenderBackend(RenderBackend backend) {
        if (rendererInfo_.backend != backend) ++rendererInfo_.backendChanges;
        rendererInfo_.backend = backend;
    }

    void initGPU() {
        if (gpuInitializationAttempted_ || !hwnd_ || deferredStartupGl_.pending()) {
            return;
        }
        gpuInitializationAttempted_ = true;
        if (!runtimeConfig_.gpuEnabled) {
            rendererInfo_.reason = "disabled-by-environment";
            return;
        }

        POINT origin{};
        if (win32::experimentalGlWorkerEnabled() && win32::nvidiaWindowDisplayOrigin(hwnd_, origin)) {
            if (win32::experimentalGlWorkerAfterFirstFrame()) {
                glWorkerWaitingForSoftwareFrame_ = true;
                rendererInfo_.reason = "opengl-worker-waiting-for-software-frame";
                return;
            }
            if (glWorker_.start(hwnd_)) {
                rendererInfo_.reason = "opengl-worker-starting";
                return;
            }
        }

        glDC_ = GetDC(hwnd_);
        if (!glDC_) { rendererInfo_.reason = "window-dc-failed"; return; }

        const auto pfd = win32::openGlWindowPixelFormat();

        int pixelFormat = ChoosePixelFormat(glDC_, &pfd);
        if (!pixelFormat || !SetPixelFormat(glDC_, pixelFormat, &pfd)) {
            rendererInfo_.reason = "pixel-format-failed";
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
            return;
        }

        // Fault injection is compiled only into the performance-lab test renderer.
#if defined(ONEUI_LAB_RENDER_TESTS)
        wchar_t failInit[2]{};
        const bool injectedFailure = GetEnvironmentVariableW(L"ONEUI_LAB_FAIL_GPU_INIT",failInit,2)>0 && failInit[0]==L'1';
        HGLRC tempContext = injectedFailure ? nullptr : wglCreateContext(glDC_);
#else
        HGLRC tempContext = wglCreateContext(glDC_);
#endif
        if (!tempContext) {
            rendererInfo_.reason = "opengl-context-failed";
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
            return;
        }

        if (!wglMakeCurrent(glDC_, tempContext)) {
            rendererInfo_.reason = "opengl-make-current-failed";
            wglDeleteContext(tempContext);
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
            return;
        }

        if (const auto* device = glGetString(GL_RENDERER))
            rendererInfo_.device = reinterpret_cast<const char*>(device);
        if (const auto* vendor = glGetString(GL_VENDOR))
            nvidiaDriver_ = std::strstr(reinterpret_cast<const char*>(vendor), "NVIDIA") != nullptr;
        traceGpu("context-current");
        GrContextOptions contextOptions;
        contextOptions.fPersistentCache = rendering::shaderCache();
        grContext_ = internal::win32GlTraceEnabled()
            ? GrDirectContexts::MakeGL(internal::makeWin32GlTraceInterface(), contextOptions)
            : GrDirectContexts::MakeGL(contextOptions);
        if (!grContext_) {
            rendererInfo_.reason = "ganesh-context-failed";
            traceGpu("ganesh-context-failed");
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempContext);
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
            return;
        }

        publishCurrentGpu(tempContext);
    }

    void publishCurrentGpu(HGLRC context) {
        const auto swapInterval = reinterpret_cast<WglSwapIntervalExtProc>(
            wglGetProcAddress("wglSwapIntervalEXT"));
        swapIntervalEnabled_ = swapInterval && swapInterval(1) == TRUE;

        glContext_ = context;
        gpuAvailable_ = true;
        traceGpu("ganesh-ready");
        std::fprintf(
            stderr,
            "OneUI GPU rendering enabled (OpenGL+Skia Ganesh, vsync=%s)\n",
            swapIntervalEnabled_ ? "on" : "dwm-fallback");
    }

    bool canAdoptDeferredGl() const {
        return deferredGlReady_ && hwnd_ && IsWindowVisible(hwnd_) && !IsIconic(hwnd_) &&
            !interactiveResizeActive_ && callbackWindowAlive_->load(std::memory_order_acquire);
    }

    void scheduleDeferredGlAdoption() {
        if (deferredGlScheduled_ || !canAdoptDeferredGl()) return;
        deferredGlScheduled_ = true;
        requestAnimationFrame([this](double) {
            deferredGlScheduled_ = false;
            if (!canAdoptDeferredGl()) return;
            deferredGlReady_ = false;
            auto prepared = deferredStartupGl_.take();
            gpuInitializationAttempted_ = true;
            const bool tracing = internal::scrollTraceEnabled();
            const double started = tracing ? internal::scrollTraceNowMs() : 0.0;
            const bool adopted = prepared && adoptDeferredGl(*prepared);
            if (tracing) {
                internal::writeScrollTrace(internal::ScrollTraceEvent{
                    "win32", "deferred_gl_ui_adopt", reinterpret_cast<std::uintptr_t>(hwnd_),
                    0, 0, 0, 0, 0, 0, internal::scrollTraceNowMs() - started,
                    adopted ? 1.0 : 0.0, 0.0});
            }
            if (adopted) requestRedraw();
            else rendererInfo_.reason = "deferred-opengl-unavailable";
        });
    }

    bool adoptDeferredGl(win32::PreparedStartupGl& prepared) {
        if (MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONULL) != prepared.monitor) return false;
        HDC dc = GetDC(hwnd_);
        if (!dc) return false;
        PIXELFORMATDESCRIPTOR actual{};
        if (!DescribePixelFormat(dc, prepared.format, sizeof(actual), &actual) ||
            std::memcmp(&actual, &prepared.descriptor, sizeof(actual)) != 0 ||
            !SetPixelFormat(dc, prepared.format, &prepared.descriptor)) {
            ReleaseDC(hwnd_, dc);
            return false;
        }
        const auto previousContext = wglGetCurrentContext();
        const auto previousDC = wglGetCurrentDC();
        if (!wglMakeCurrent(dc, prepared.context)) {
            if (previousContext) wglMakeCurrent(previousDC, previousContext);
            ReleaseDC(hwnd_, dc);
            return false;
        }
        glDC_ = dc;
        if (const auto* device = glGetString(GL_RENDERER))
            rendererInfo_.device = reinterpret_cast<const char*>(device);
        nvidiaDriver_ = true;
        grContext_ = std::move(prepared.ganesh);
        publishCurrentGpu(std::exchange(prepared.context, nullptr));
        return true;
    }

    void shutdownGPU() {
        glWorkerWaitingForSoftwareFrame_ = false;
        glWorker_.stop();
        // Each top-level window owns an independent GL context. Destruction
        // can arrive while a different window was the last one to paint.
        const HGLRC previousContext = wglGetCurrentContext();
        const HDC previousDC = wglGetCurrentDC();
        const bool bound = glContext_ && wglMakeCurrent(glDC_, glContext_);
        if (grContext_ && glContext_ && !bound) grContext_->abandonContext();
        windowSurface_.reset();
        paintSurface_.reset();
        grContext_.reset();
        if (glContext_) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(glContext_);
            if (previousContext && previousContext != glContext_)
                wglMakeCurrent(previousDC, previousContext);
            glContext_ = nullptr;
        }
        if (glDC_) {
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
        }
        gpuAvailable_ = false;
        swapIntervalEnabled_ = false;
    }
