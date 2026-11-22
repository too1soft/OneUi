#include "oneui/controls/image_view.h"
#include "internal/raster_image.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace oneui {

ImageView::ImageView() {
    setPreferredSize(Size{40.0f, 40.0f});
    setAccessibleRole(AccessibilityRole::Custom);
}

bool ImageView::setRgbaPixels(
    const std::uint8_t* pixels,
    std::size_t length,
    int width,
    int height,
    int stride) {
    if (!pixels || width <= 0 || height <= 0 || stride <= 0 || width > stride / 4) {
        return false;
    }
    const auto rows = static_cast<std::size_t>(height);
    const auto rowBytes = static_cast<std::size_t>(stride);
    if (rows > std::numeric_limits<std::size_t>::max() / rowBytes || length < rows * rowBytes) {
        return false;
    }
    auto bytes = SkData::MakeWithCopy(pixels, rows * rowBytes);
    if (!bytes) {
        return false;
    }
    const auto info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    image_ = std::make_shared<const rendering::RasterImage>(std::move(bytes), info, rowBytes);
    width_ = width;
    height_ = height;
    invalidate();
    return true;
}

void ImageView::clearImage() {
    if (!image_) {
        return;
    }
    image_.reset();
    width_ = 0;
    height_ = 0;
    invalidate();
}

void ImageView::setContentMode(ImageContentMode mode) {
    if (contentMode_ == mode) {
        return;
    }
    contentMode_ = mode;
    invalidate();
}

void ImageView::setContentAlignment(
    ImageContentAlignment horizontal,
    ImageContentAlignment vertical) {
    if (horizontalAlignment_ == horizontal && verticalAlignment_ == vertical) {
        return;
    }
    horizontalAlignment_ = horizontal;
    verticalAlignment_ = vertical;
    invalidate();
}

void ImageView::setCornerRadius(float radius) {
    const float next = std::max(0.0f, radius);
    if (std::fabs(cornerRadius_ - next) < 0.01f) {
        return;
    }
    cornerRadius_ = next;
    invalidate();
}

void ImageView::setBackground(Color color) {
    if (background_.r == color.r && background_.g == color.g &&
        background_.b == color.b && background_.a == color.a) {
        return;
    }
    background_ = color;
    invalidate();
}

void ImageView::paint(Canvas& canvas) {
    const Rect bounds = frame();
    if (background_.a != 0) {
        canvas.fillRect(bounds, background_, cornerRadius_);
    }
    if (!image_ || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return;
    }

    Rect target = bounds;
    if (contentMode_ != ImageContentMode::Stretch) {
        const float scaleX = bounds.width / static_cast<float>(width_);
        const float scaleY = bounds.height / static_cast<float>(height_);
        float scale = 1.0f;
        if (contentMode_ != ImageContentMode::ActualSize) {
            scale = contentMode_ == ImageContentMode::Cover
                ? std::max(scaleX, scaleY)
                : std::min(scaleX, scaleY);
        }
        target.width = static_cast<float>(width_) * scale;
        target.height = static_cast<float>(height_) * scale;
        const auto alignedOffset = [](float available, ImageContentAlignment alignment) {
            switch (alignment) {
                case ImageContentAlignment::Start: return 0.0f;
                case ImageContentAlignment::End: return available;
                case ImageContentAlignment::Center: return available * 0.5f;
            }
            return available * 0.5f;
        };
        target.x += alignedOffset(bounds.width - target.width, horizontalAlignment_);
        target.y += alignedOffset(bounds.height - target.height, verticalAlignment_);
    }

    canvas.save();
    canvas.clipRect(bounds);
    if (auto* rasterCanvas = dynamic_cast<rendering::RasterImageCanvas*>(&canvas)) {
        rasterCanvas->drawRasterImage(target, *image_);
    } else {
        const auto& pixels = image_->pixels;
        canvas.drawPixels(target, static_cast<const std::uint8_t*>(pixels.addr()),
            width_, height_, static_cast<int>(pixels.rowBytes()), CanvasPixelFormat::Rgba8888);
    }
    canvas.restore();
}

} // namespace oneui
