    bool blitSoftwareSurface(HDC dc, SkSurface& surface, RECT dirty, bool fullPaint,
                             const internal::InteractionSnapshot& trace) {
        SkPixmap pixmap;
        if (!surface.peekPixels(&pixmap)) return false;
        internal::traceInteractionPaint(trace, "blit_begin", "software");
        const double started = currentTimeMs();
        const int width = dirty.right - dirty.left;
        const int height = dirty.bottom - dirty.top;
        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;
        bool succeeded;
        if (fullPaint) {
            bitmapInfo.bmiHeader.biWidth = pixmap.width();
            bitmapInfo.bmiHeader.biHeight = -pixmap.height();
            const int copied = StretchDIBits(dc, 0, 0, width, height, 0, 0, width, height,
                pixmap.addr(), &bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
            succeeded = copied != 0 && copied != GDI_ERROR;
        } else {
            blitScratch_.resize(static_cast<std::size_t>(width) * height);
            const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
            auto* dst = reinterpret_cast<unsigned char*>(blitScratch_.data());
            for (int row = 0; row < height; ++row) {
                std::memcpy(dst + static_cast<std::size_t>(row) * rowBytes,
                            pixmap.addr(dirty.left, dirty.top + row), rowBytes);
            }
            bitmapInfo.bmiHeader.biWidth = width;
            bitmapInfo.bmiHeader.biHeight = -height;
            succeeded = SetDIBitsToDevice(dc, dirty.left, dirty.top, width, height, 0, 0, 0, height,
                blitScratch_.data(), &bitmapInfo, DIB_RGB_COLORS) != 0;
        }
        internal::traceInteractionPaint(trace, "blit_return", "software", succeeded ? 1 : 0);
        recordBlit(currentTimeMs() - started);
        return succeeded;
    }

    bool presentStartupRaster(PAINTSTRUCT& paint, win32::StartupRaster::Frame frame,
                              RECT client, double paintStarted) {
        const bool presented = blitSoftwareSurface(paint.hdc, *frame.surface, client, true, frame.trace);
        EndPaint(hwnd_, &paint);
        internal::traceInteractionPaint(frame.trace, "startup_raster_presented", "software", presented ? 1 : 0);
        if (!presented) {
            startupRasterInputSeen_ = true;
            queueInvalidation(nullptr);
            return false;
        }
        paintSurfaceWidth_ = frame.surface->width();
        paintSurfaceHeight_ = frame.surface->height();
        paintSurface_ = std::move(frame.surface);
        glWorker_.setStartupPicture({std::move(frame.picture), frame.width, frame.height, frame.trace.paintSequence});
        recordPaint(frame.width, frame.height, true, false, 0.0, currentTimeMs() - paintStarted);
        return true;
    }

    bool blitResizeOverlay(RECT client, const internal::InteractionSnapshot& trace, win32::VirtualRowDamage* damage) {
        const auto geometry = win32::resizeOverlayGeometry();
        const bool reserve = geometry == win32::ResizeOverlayGeometry::ReuseCapacity;
        const int width = reserve ? win32::alignedGpuPaintExtent(client.right, INT_MAX) : client.right;
        const int height = reserve ? win32::alignedGpuPaintExtent(client.bottom, INT_MAX) : client.bottom;
        internal::traceInteractionPaint(trace, "resize_overlay_prepare_begin", "software");
        const auto layer = resizeOverlay_.prepare(hwnd_, width, height, geometry);
        internal::traceInteractionPaint(trace, "resize_overlay_prepare_end", "software", layer.window ? 1 : 0);
        if (!layer.window) return false;
        if (layer.positioned) internal::traceInteractionPaint(trace, "resize_overlay_positioned", "software", 1);
        const auto dc = GetDC(layer.window);
        if (!dc) return false;
        const bool blitted = blitSoftwareSurface(dc, *paintSurface_, client, true, trace);
        if (blitted && damage) damage->inspectOverlayClip(dc);
        ReleaseDC(layer.window, dc);
        internal::traceInteractionPaint(trace, "resize_overlay_blit_return", "software", blitted ? 1 : 0);
        return blitted;
    }

    void finishResizeOverlay() {
        if (!glWorker_.takeSoftwareResizeResume()) return;
        resizeOverlay_.close();
        internal::traceInteractionPaint({reinterpret_cast<std::uintptr_t>(hwnd_)},
                                        "resize_overlay_closed", "opengl-worker", 1);
    }

    void abortResizeOverlay() {
        resizeOverlayFailed_ = true;
        resizeOverlay_.close();
        glWorker_.finishSoftwareResize();
        requestDeferredFullPaint();
    }
