#include "oneui/controls/realtime_frame_view.h"

#include "oneui/color.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

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

bool frameLayout(
    const VideoFrame& frame,
    int& rowBytes,
    int& sourceStride,
    std::size_t& requiredBytes) {
    const int bpp = bytesPerPixel(frame.format);
    if (!frame.data || frame.width <= 0 || frame.height <= 0 || bpp <= 0) {
        return false;
    }

    const auto rowBytesWide = static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(bpp);
    if (rowBytesWide > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    rowBytes = static_cast<int>(rowBytesWide);
    sourceStride = frame.stride > 0 ? frame.stride : rowBytes;
    if (sourceStride < rowBytes) {
        return false;
    }

    const auto rowsBeforeLast = static_cast<std::size_t>(frame.height - 1);
    const auto strideWide = static_cast<std::size_t>(sourceStride);
    if (rowsBeforeLast > (std::numeric_limits<std::size_t>::max() - rowBytesWide) / strideWide) {
        return false;
    }
    requiredBytes = rowsBeforeLast * strideWide + rowBytesWide;
    return true;
}

bool patchLayout(
    const VideoFramePatch& patch,
    int bytesPerPixel,
    int frameWidth,
    int frameHeight,
    int& rowBytes,
    int& sourceStride,
    std::size_t& requiredBytes) {
    if (!patch.data || patch.width <= 0 || patch.height <= 0 || patch.x < 0 || patch.y < 0 ||
        patch.x > frameWidth - patch.width || patch.y > frameHeight - patch.height) {
        return false;
    }
    const auto rowBytesWide = static_cast<std::size_t>(patch.width) * static_cast<std::size_t>(bytesPerPixel);
    if (rowBytesWide > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    rowBytes = static_cast<int>(rowBytesWide);
    sourceStride = patch.stride > 0 ? patch.stride : rowBytes;
    if (sourceStride < rowBytes) {
        return false;
    }
    const auto rowsBeforeLast = static_cast<std::size_t>(patch.height - 1);
    const auto strideWide = static_cast<std::size_t>(sourceStride);
    if (rowsBeforeLast > (std::numeric_limits<std::size_t>::max() - rowBytesWide) / strideWide) {
        return false;
    }
    requiredBytes = rowsBeforeLast * strideWide + rowBytesWide;
    return patch.pixelBytes >= requiredBytes;
}

Rect intersectRects(Rect first, Rect second) {
    const float left = std::max(first.x, second.x);
    const float top = std::max(first.y, second.y);
    const float right = std::min(first.x + first.width, second.x + second.width);
    const float bottom = std::min(first.y + first.height, second.y + second.height);
    if (right <= left || bottom <= top) {
        return {};
    }
    return Rect{left, top, right - left, bottom - top};
}

Rect scaledContentRect(Rect bounds, Size videoSize, ScaleMode mode) {
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

} // namespace

RealtimeFrameView::RealtimeFrameView() {
    setPreferredSize(Size{320.0f, 200.0f});
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(L"Realtime frame view");
}

void RealtimeFrameView::submitFrame(const VideoFrame& frame) {
    int rowBytes = 0;
    int sourceStride = 0;
    std::size_t requiredBytes = 0;
    if (!frameLayout(frame, rowBytes, sourceStride, requiredBytes)) {
        std::shared_ptr<StoredVideoFrame> previous;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            previous = std::move(latestFrame_);
        }
        previous.reset();
        invalidate();
        return;
    }

    const auto compactBytes = static_cast<std::size_t>(rowBytes) * static_cast<std::size_t>(frame.height);
    auto pixels = std::make_shared<std::vector<std::uint8_t>>(compactBytes);
    const auto* source = static_cast<const std::uint8_t*>(frame.data);
    for (int row = 0; row < frame.height; ++row) {
        std::memcpy(
            pixels->data() + static_cast<std::size_t>(rowBytes) * static_cast<std::size_t>(row),
            source + static_cast<std::size_t>(sourceStride) * static_cast<std::size_t>(row),
            static_cast<std::size_t>(rowBytes));
    }

    VideoFrame compact = frame;
    compact.data = pixels->data();
    compact.stride = rowBytes;
    std::shared_ptr<const void> owner(pixels, static_cast<const void*>(pixels->data()));
    (void)submitOwnedFrame(compact, compactBytes, std::move(owner));
}

bool RealtimeFrameView::submitOwnedFrame(
    const VideoFrame& frame,
    std::size_t pixelBytes,
    std::shared_ptr<const void> owner) {
    int rowBytes = 0;
    int sourceStride = 0;
    std::size_t requiredBytes = 0;
    if (!owner || !frameLayout(frame, rowBytes, sourceStride, requiredBytes) || pixelBytes < requiredBytes) {
        return false;
    }

    auto stored = std::make_shared<StoredVideoFrame>();
    stored->owner = std::move(owner);
    stored->pixels = static_cast<const std::uint8_t*>(frame.data);
    stored->pixelBytes = pixelBytes;
    stored->width = frame.width;
    stored->height = frame.height;
    stored->stride = sourceStride;
    stored->format = frame.format;
    stored->frameId = frame.frameId;
    stored->timestampUs = frame.timestampUs;

    std::shared_ptr<StoredVideoFrame> previous;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        previous = std::exchange(latestFrame_, std::move(stored));
    }
    // The owner can invoke an application release callback. Never run it while
    // holding the frame mutex because callbacks may re-enter the view.
    previous.reset();
    invalidate();
    return true;
}

bool RealtimeFrameView::submitDamage(
    int frameWidth,
    int frameHeight,
    PixelFormat format,
    const VideoFramePatch* patches,
    std::size_t patchCount,
    std::uint64_t frameId,
    std::uint64_t timestampUs) {
    constexpr std::size_t kMaximumPatchCount = 64;
    const int bpp = bytesPerPixel(format);
    if (!patches || patchCount == 0 || patchCount > kMaximumPatchCount ||
        frameWidth <= 0 || frameHeight <= 0 || bpp <= 0 ||
        frameWidth > std::numeric_limits<int>::max() / bpp) {
        return false;
    }

    struct ValidatedPatch {
        const VideoFramePatch* patch = nullptr;
        int rowBytes = 0;
        int sourceStride = 0;
    };
    std::vector<ValidatedPatch> validated;
    validated.reserve(patchCount);
    int dirtyLeft = frameWidth;
    int dirtyTop = frameHeight;
    int dirtyRight = 0;
    int dirtyBottom = 0;
    for (std::size_t index = 0; index < patchCount; ++index) {
        int rowBytes = 0;
        int sourceStride = 0;
        std::size_t requiredBytes = 0;
        if (!patchLayout(
                patches[index], bpp, frameWidth, frameHeight,
                rowBytes, sourceStride, requiredBytes)) {
            return false;
        }
        validated.push_back(ValidatedPatch{&patches[index], rowBytes, sourceStride});
        dirtyLeft = std::min(dirtyLeft, patches[index].x);
        dirtyTop = std::min(dirtyTop, patches[index].y);
        dirtyRight = std::max(dirtyRight, patches[index].x + patches[index].width);
        dirtyBottom = std::max(dirtyBottom, patches[index].y + patches[index].height);
    }

    std::shared_ptr<const void> releasedOwner;
    ScaleMode mode = ScaleMode::Fit;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!latestFrame_ || latestFrame_->width != frameWidth || latestFrame_->height != frameHeight ||
            latestFrame_->format != format) {
            return false;
        }

        const int compactStride = frameWidth * bpp;
        if (!latestFrame_->mutablePixels) {
            auto pixels = std::make_shared<std::vector<std::uint8_t>>(
                static_cast<std::size_t>(compactStride) * static_cast<std::size_t>(frameHeight));
            for (int row = 0; row < frameHeight; ++row) {
                std::memcpy(
                    pixels->data() + static_cast<std::size_t>(compactStride) * static_cast<std::size_t>(row),
                    latestFrame_->pixels + static_cast<std::size_t>(latestFrame_->stride) * static_cast<std::size_t>(row),
                    static_cast<std::size_t>(compactStride));
            }
            releasedOwner = std::move(latestFrame_->owner);
            latestFrame_->mutablePixels = std::move(pixels);
            latestFrame_->pixels = latestFrame_->mutablePixels->data();
            latestFrame_->pixelBytes = latestFrame_->mutablePixels->size();
            latestFrame_->stride = compactStride;
        }

        for (const auto& item : validated) {
            const auto& patch = *item.patch;
            const auto* source = static_cast<const std::uint8_t*>(patch.data);
            auto* destination = latestFrame_->mutablePixels->data() +
                static_cast<std::size_t>(patch.y) * static_cast<std::size_t>(latestFrame_->stride) +
                static_cast<std::size_t>(patch.x) * static_cast<std::size_t>(bpp);
            for (int row = 0; row < patch.height; ++row) {
                std::memcpy(
                    destination + static_cast<std::size_t>(row) * static_cast<std::size_t>(latestFrame_->stride),
                    source + static_cast<std::size_t>(row) * static_cast<std::size_t>(item.sourceStride),
                    static_cast<std::size_t>(item.rowBytes));
            }
        }
        latestFrame_->frameId = frameId;
        latestFrame_->timestampUs = timestampUs;
        mode = scaleMode_;
    }
    // The old full-frame owner may call application code while being released.
    // Keep that callback outside the view lock.
    releasedOwner.reset();

    const Rect bounds = frame();
    const Rect content = scaledContentRect(
        bounds,
        Size{static_cast<float>(frameWidth), static_cast<float>(frameHeight)},
        mode);
    const float scaleX = content.width / static_cast<float>(frameWidth);
    const float scaleY = content.height / static_cast<float>(frameHeight);
    Rect dirty{
        content.x + static_cast<float>(dirtyLeft) * scaleX,
        content.y + static_cast<float>(dirtyTop) * scaleY,
        static_cast<float>(dirtyRight - dirtyLeft) * scaleX,
        static_cast<float>(dirtyBottom - dirtyTop) * scaleY};
    dirty = intersectRects(dirty, bounds);
    if (dirty.width > 0.0f && dirty.height > 0.0f) {
        invalidateRect(dirty);
    }
    return true;
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

    return scaledContentRect(bounds, videoSize, mode);
}

std::optional<VideoFrameSnapshot> RealtimeFrameView::latestFrame() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto& stored = latestFrame_;
    if (!stored) {
        return std::nullopt;
    }

    const int bpp = bytesPerPixel(stored->format);
    if (bpp <= 0) {
        return std::nullopt;
    }
    const int rowBytes = static_cast<int>(
        static_cast<std::size_t>(stored->width) * static_cast<std::size_t>(bpp));
    VideoFrameSnapshot snapshot;
    snapshot.width = stored->width;
    snapshot.height = stored->height;
    snapshot.stride = rowBytes;
    snapshot.format = stored->format;
    snapshot.frameId = stored->frameId;
    snapshot.timestampUs = stored->timestampUs;
    snapshot.pixels.resize(static_cast<std::size_t>(rowBytes) * static_cast<std::size_t>(stored->height));
    for (int row = 0; row < stored->height; ++row) {
        std::memcpy(
            snapshot.pixels.data() + static_cast<std::size_t>(rowBytes) * static_cast<std::size_t>(row),
            stored->pixels + static_cast<std::size_t>(stored->stride) * static_cast<std::size_t>(row),
            static_cast<std::size_t>(rowBytes));
    }
    return snapshot;
}

void RealtimeFrameView::paint(Canvas& canvas) {
    std::lock_guard<std::mutex> lock(mutex_);
    const Color background = backgroundColor_;
    const ScaleMode mode = scaleMode_;
    const auto& snapshot = latestFrame_;

    const Rect bounds = frame();
    if (background.a > 0) {
        canvas.fillRect(bounds, background, 0.0f);
    }
    const Size videoSize = snapshot
        ? Size{static_cast<float>(snapshot->width), static_cast<float>(snapshot->height)}
        : Size{};
    const Rect content = scaledContentRect(bounds, videoSize, mode);

    CanvasPixelFormat canvasFormat = CanvasPixelFormat::Bgra8888;
    if (snapshot && snapshot->pixels && toCanvasPixelFormat(snapshot->format, canvasFormat)) {
        canvas.drawPixels(content, snapshot->pixels, snapshot->width, snapshot->height, snapshot->stride, canvasFormat);
        return;
    }

    if (background.a > 0) {
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
