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
        if (gpuInitializationAttempted_ || !hwnd_) {
            return;
        }
        gpuInitializationAttempted_ = true;
        if (!runtimeConfig_.gpuEnabled) {
            rendererInfo_.reason = "disabled-by-environment";
            return;
        }

        glDC_ = GetDC(hwnd_);
        if (!glDC_) { rendererInfo_.reason = "window-dc-failed"; return; }

        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 0;
        pfd.cStencilBits = 0;
        pfd.iLayerType = PFD_MAIN_PLANE;

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
        grContext_ = GrDirectContexts::MakeGL(contextOptions);
        if (!grContext_) {
            rendererInfo_.reason = "ganesh-context-failed";
            traceGpu("ganesh-context-failed");
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempContext);
            ReleaseDC(hwnd_, glDC_);
            glDC_ = nullptr;
            return;
        }

        const auto swapInterval = reinterpret_cast<WglSwapIntervalExtProc>(
            wglGetProcAddress("wglSwapIntervalEXT"));
        swapIntervalEnabled_ = swapInterval && swapInterval(1) == TRUE;

        glContext_ = tempContext;
        gpuAvailable_ = true;
        traceGpu("ganesh-ready");
        std::fprintf(
            stderr,
            "OneUI GPU rendering enabled (OpenGL+Skia Ganesh, vsync=%s)\n",
            swapIntervalEnabled_ ? "on" : "dwm-fallback");
    }

    void shutdownGPU() {
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
