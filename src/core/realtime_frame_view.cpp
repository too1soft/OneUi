#include "oneui/controls/realtime_frame_view.h"

#include "oneui/color.h"

#include <algorithm>
#include <cstring>

namespace oneui {
namespace {

int bytesPerPixel(PixelFormat format) {
    switch (format) {
    case PixelFormat::Bgra8888:
    case PixelFormat::Rgba8888:
        return 4;
    case PixelFormat::Nv12:
        return 0;
    }
    return 0;
}

bool toCanvasPixelFormat(PixelFormat source, CanvasPixelFormat& target) {
    switch (source) {
    case PixelFormat::Bgra8888:
        target = CanvasPixelFormat::Bgra8888;
        return true;
    case PixelFormat::Rgba8888:
        target = CanvasPixelFormat::Rgba8888;
        return true;
    case PixelFormat::Nv12:
        return false;
    }
    return false;
}

} // namespace

RealtimeFrameView::RealtimeFrameView() {
    setPreferredSize(Size{320.0f, 200.0f});
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(L"Realtime frame view");
}

void RealtimeFrameView::submitFrame(const VideoFrame& frame) {
    VideoFrameSnapshot snapshot;
    snapshot.width = std::max(0, frame.width);
    snapshot.height = std::max(0, frame.height);
    snapshot.format = frame.format;
    snapshot.frameId = frame.frameId;
    snapshot.timestampUs = frame.timestampUs;

    const int bpp = bytesPerPixel(frame.format);
    if (frame.data && snapshot.width > 0 && snapshot.height > 0 && bpp > 0) {
        const int rowBytes = snapshot.width * bpp;
        const int sourceStride = frame.stride > 0 ? frame.stride : rowBytes;
        if (sourceStride >= rowBytes) {
            snapshot.stride = rowBytes;
            snapshot.pixels.resize(static_cast<std::size_t>(rowBytes) * static_cast<std::size_t>(snapshot.height));

            const auto* source = static_cast<const std::uint8_t*>(frame.data);
            for (int row = 0; row < snapshot.height; ++row) {
                std::memcpy(
                    snapshot.pixels.data() + static_cast<std::size_t>(rowBytes) * static_cast<std::size_t>(row),
                    source + static_cast<std::size_t>(sourceStride) * static_cast<std::size_t>(row),
                    static_cast<std::size_t>(rowBytes));
            }
        }
    } else {
        snapshot.stride = std::max(0, frame.stride);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (snapshot.width <= 0 || snapshot.height <= 0) {
            latestFrame_.reset();
        } else {
            latestFrame_ = std::move(snapshot);
        }
    }

    invalidate();
}

void RealtimeFrameView::setScaleMode(ScaleMode mode) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (scaleMode_ == mode) {
            return;
        }
        scaleMode_ = mode;
    }
    invalidate();
}

ScaleMode RealtimeFrameView::scaleMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return scaleMode_;
}

void RealtimeFrameView::setBackgroundColor(Color color) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        backgroundColor_ = color;
    }
    invalidate();
}

Rect RealtimeFrameView::contentRect() const {
    const Rect bounds = frame();

    ScaleMode mode = ScaleMode::Fit;
    Size videoSize;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        mode = scaleMode_;
        videoSize = videoSizeLocked();
    }

    if (bounds.width <= 0.0f || bounds.height <= 0.0f || videoSize.width <= 0.0f || videoSize.height <= 0.0f) {
        return bounds;
    }

    if (mode == ScaleMode::Stretch) {
        return bounds;
    }

    float scale = 1.0f;
    if (mode != ScaleMode::ActualSize) {
        const float scaleX = bounds.width / videoSize.width;
        const float scaleY = bounds.height / videoSize.height;
        scale = mode == ScaleMode::Fill ? std::max(scaleX, scaleY) : std::min(scaleX, scaleY);
    }

    const float width = videoSize.width * scale;
    const float height = videoSize.height * scale;
    return Rect{bounds.x + (bounds.width - width) / 2.0f, bounds.y + (bounds.height - height) / 2.0f, width, height};
}

std::optional<VideoFrameSnapshot> RealtimeFrameView::latestFrame() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latestFrame_;
}

void RealtimeFrameView::paint(Canvas& canvas) {
    if (backgroundColor_.a > 0) {
        canvas.fillRect(frame(), backgroundColor_, 0.0f);
    }
    const Rect content = contentRect();

    std::optional<VideoFrameSnapshot> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot = latestFrame_;
    }

    CanvasPixelFormat canvasFormat = CanvasPixelFormat::Bgra8888;
    if (snapshot && !snapshot->pixels.empty() && toCanvasPixelFormat(snapshot->format, canvasFormat)) {
        canvas.drawPixels(content, snapshot->pixels.data(), snapshot->width, snapshot->height, snapshot->stride, canvasFormat);
        return;
    }

    if (backgroundColor_.a > 0) {
        canvas.fillRect(content, Color{15, 23, 42}, 0.0f);
    }
}

AccessibilityInfo RealtimeFrameView::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::Custom;
    }
    if (info.name.empty()) {
        info.name = L"Realtime frame view";
    }
    return info;
}

Size RealtimeFrameView::videoSizeLocked() const {
    if (!latestFrame_) {
        return Size{};
    }
    return Size{static_cast<float>(latestFrame_->width), static_cast<float>(latestFrame_->height)};
}

} // namespace oneui
