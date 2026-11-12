#include "oneui/pixel_image.h"

#include <limits>
#include <utility>

namespace oneui {

PixelImage::PixelImage(const std::uint8_t* pixels, std::size_t pixelBytes,
                       int width, int height, int stride, CanvasPixelFormat format,
                       std::shared_ptr<const void> owner)
    : pixels_(pixels), pixelBytes_(pixelBytes), width_(width), height_(height),
      stride_(stride), format_(format), owner_(std::move(owner)) {}

std::shared_ptr<const PixelImage> PixelImage::makeOwned(
    const std::uint8_t* pixels, std::size_t pixelBytes,
    int width, int height, int stride, CanvasPixelFormat format,
    std::shared_ptr<const void> owner) {
    if (!pixels || !owner || width <= 0 || height <= 0 ||
        width > std::numeric_limits<int>::max() / 4 ||
        (format != CanvasPixelFormat::Bgra8888 && format != CanvasPixelFormat::Rgba8888)) {
        return {};
    }
    const int rowBytes = width * 4;
    if (stride <= 0) stride = rowBytes;
    if (stride < rowBytes) return {};
    const auto rows = static_cast<std::size_t>(height - 1);
    const auto row = static_cast<std::size_t>(rowBytes);
    const auto step = static_cast<std::size_t>(stride);
    if (rows > (std::numeric_limits<std::size_t>::max() - row) / step) return {};
    const auto required = rows * step + row;
    if (pixelBytes < required) return {};
    return std::shared_ptr<const PixelImage>(new PixelImage(
        pixels, required, width, height, stride, format, std::move(owner)));
}

std::shared_ptr<PixelImageResource> PixelImage::rendererResource(
    const void* backendKey,
    const std::function<std::shared_ptr<PixelImageResource>()>& factory) const {
    if (!backendKey || !factory) return {};
    {
        std::lock_guard<std::mutex> lock(resourceMutex_);
        if (resource_ && resourceKey_ == backendKey) return resource_;
    }
    auto created = factory();
    if (!created) return {};
    std::shared_ptr<PixelImageResource> result;
    {
        std::lock_guard<std::mutex> lock(resourceMutex_);
        if (!resource_) {
            resourceKey_ = backendKey;
            resource_ = created;
        }
        result = resourceKey_ == backendKey ? resource_ : created;
    }
    return result;
}

void drawPixelImage(Canvas& canvas, Rect rect, const std::shared_ptr<const PixelImage>& image) {
    if (!image) return;
    if (auto* renderer = dynamic_cast<PixelImageCanvas*>(&canvas)) {
        renderer->drawPixelImage(rect, image);
    } else {
        canvas.drawPixels(rect, image->pixels(), image->width(), image->height(),
                          image->stride(), image->format());
    }
}

} // namespace oneui
