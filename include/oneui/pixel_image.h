#pragma once

#include "oneui/canvas.h"

#include <functional>
#include <memory>
#include <mutex>

namespace oneui {

/// A renderer resource may retain the original pixel owner, but must not retain
/// its PixelImage: the image owns the resource and that would create a cycle.
class ONEUI_API PixelImageResource {
public:
    virtual ~PixelImageResource() = default;
};

/// Immutable, owned pixels. Repainting an image preserves its renderer identity.
/// The producer must not write to the buffer until its owner is released.
class ONEUI_API PixelImage final {
public:
    static std::shared_ptr<const PixelImage> makeOwned(
        const std::uint8_t* pixels, std::size_t pixelBytes,
        int width, int height, int stride, CanvasPixelFormat format,
        std::shared_ptr<const void> owner);

    const std::uint8_t* pixels() const { return pixels_; }
    std::size_t pixelBytes() const { return pixelBytes_; }
    int width() const { return width_; }
    int height() const { return height_; }
    int stride() const { return stride_; }
    CanvasPixelFormat format() const { return format_; }
    std::shared_ptr<const void> pixelOwner() const { return owner_; }

    // At most one backend resource is retained per image. Factories and losing
    // resources are invoked/released outside the lock; concurrent factories may
    // run, but callers for the same key receive the same published resource.
    std::shared_ptr<PixelImageResource> rendererResource(
        const void* backendKey,
        const std::function<std::shared_ptr<PixelImageResource>()>& factory) const;

private:
    PixelImage(const std::uint8_t* pixels, std::size_t pixelBytes,
               int width, int height, int stride, CanvasPixelFormat format,
               std::shared_ptr<const void> owner);

    const std::uint8_t* pixels_;
    std::size_t pixelBytes_;
    int width_;
    int height_;
    int stride_;
    CanvasPixelFormat format_;
    std::shared_ptr<const void> owner_;
    mutable std::mutex resourceMutex_;
    mutable const void* resourceKey_ = nullptr;
    mutable std::shared_ptr<PixelImageResource> resource_;
};

/// Optional extension keeps the existing Canvas vtable and custom backends
/// compatible. Backends without it continue through drawPixels.
class ONEUI_API PixelImageCanvas {
public:
    virtual ~PixelImageCanvas() = default;
    virtual void drawPixelImage(Rect rect, const std::shared_ptr<const PixelImage>& image) = 0;
};

ONEUI_API void drawPixelImage(Canvas& canvas, Rect rect,
                             const std::shared_ptr<const PixelImage>& image);

} // namespace oneui
