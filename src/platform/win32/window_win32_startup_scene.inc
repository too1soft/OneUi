    struct StartupScene {
        sk_sp<SkPicture> picture;
        SkISize size{};
        float scale = 0;
        bool redrawAfterReplay = false;
    };

    StartupScene startupScene_;
    bool startupSceneAttempted_ = false;
    bool recordingStartupScene_ = false;
    bool startupSceneReentered_ = false;
    bool startupSceneNeedsRedraw_ = false;

    bool shouldRecordStartupScene() const {
        if (startupSceneAttempted_ || rendererInfo_.paints != 1 || interactiveResizeActive_ ||
            !win32::experimentalAngleWorkerEnabled()) return false;
        static const bool enabled = [] {
            wchar_t value[2]{};
            return GetEnvironmentVariableW(L"ONEUI_WIN32_STAGE_STARTUP_SCENE", value, 2) == 1 && value[0] == L'1';
        }();
        return enabled;
    }

    void invalidateStartupScene() {
        startupScene_.picture.reset();
        if (recordingStartupScene_) startupSceneNeedsRedraw_ = true;
    }

    StartupScene takeStartupScene(SkISize size, float scale, bool fullSoftwareOrWorkerPaint) {
        if (!startupScene_.picture) return {};
        auto cached = std::move(startupScene_);
        if (!fullSoftwareOrWorkerPaint || invalidationPending_ || interactiveResizeActive_ ||
            cached.size != size || cached.scale != scale) return {};
        return cached;
    }

    bool deferStartupScene(const sk_sp<SkPicture>& picture, SkISize size, float scale) {
        if (!picture || startupSceneReentered_) return false;
        // Paint can request a follow-up frame while laying out children. That
        // frame must still run after presenting this snapshot. Any invalidation
        // from a later message discards the snapshot before it can replay.
        startupScene_ = {picture, size, scale, startupSceneNeedsRedraw_ || invalidationPending_};
        return true;
    }
