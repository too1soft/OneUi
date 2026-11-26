    struct ClientPaintArea {
        int width, height;
        int dirtyX, dirtyY, dirtyWidth, dirtyHeight;
        float scale;
        bool fullPaint;
    };

    template<class Overlay>
    sk_sp<SkPicture> drawPaintContent(const ClientPaintArea& area, bool record, bool threadedGl,
                                    bool useGPU, internal::Win32PaintTrace& paintTrace,
                                    const internal::FrameProfile& profile, Overlay paintOverlay) {
        using PaintTrace = internal::Win32PaintTrace;
        SkCanvas* skCanvas = threadedGl ? nullptr : paintSurface_->getCanvas();
        std::optional<SkPictureRecorder> recorder;
        if (record) {
            recorder.emplace();
            skCanvas = recorder->beginRecording(SkRect::MakeWH(
                static_cast<float>(area.width), static_cast<float>(area.height)));
        }
        if (threadedGl && !area.fullPaint) {
            skCanvas->clipRect(SkRect::MakeLTRB(static_cast<float>(area.dirtyX), static_cast<float>(area.dirtyY),
                static_cast<float>(area.dirtyX + area.dirtyWidth), static_cast<float>(area.dirtyY + area.dirtyHeight)));
        }
        if (area.fullPaint) {
            const auto background = colors::Surface;
            skCanvas->clear(SkColorSetARGB(background.a, background.r, background.g, background.b));
        }

        skCanvas->save();
        skCanvas->scale(area.scale, area.scale);
        const float logicalWidth = static_cast<float>(area.width) / area.scale;
        const float logicalHeight = static_cast<float>(area.height) / area.scale;
        auto canvasOwner = rendering::makeSkiaCanvas(
            *skCanvas, &defaultFontFamily_, Rect{0.0f, 0.0f, logicalWidth, logicalHeight});
        auto& canvas = *canvasOwner;
        if (!area.fullPaint) {
            const Rect dirtyCanvasRect{
                static_cast<float>(area.dirtyX) / area.scale,
                static_cast<float>(area.dirtyY) / area.scale,
                static_cast<float>(area.dirtyWidth) / area.scale,
                static_cast<float>(area.dirtyHeight) / area.scale};
            canvas.save();
            canvas.clipRect(dirtyCanvasRect);
            canvas.fillRect(dirtyCanvasRect, colors::Surface, 0.0f);
        }

        paintTrace.mark(PaintTrace::Canvas);
        if (content_ && content_->visible()) {
            const double contentStartMs = currentTimeMs();
            g_primitivePaintTrace = PrimitivePaintTrace{};
            text::AtlasWarmupCapture atlasCapture(textAtlasWarmup_.capture(
                useGPU && nvidiaDriver_ && area.fullPaint && !interactiveResizeActive_));
            paintClientContent(canvas, logicalWidth, logicalHeight);
            recordContentPaint(currentTimeMs() - contentStartMs);
            recordPrimitivePaint(g_primitivePaintTrace);
            for (std::size_t i = 0; i < traceStageMs_.size(); ++i) traceStageMs_[i] += profile.ms[i];
        }
        paintTrace.mark(PaintTrace::Tree);

        if (!area.fullPaint) canvas.restore();
        paintOverlay(canvas);
        skCanvas->restore();
        if (!recorder) return {};
        canvasOwner.reset();
        return recorder->finishRecordingAsPicture();
    }
