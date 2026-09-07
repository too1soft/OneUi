#include "oneui/controls/image_view.h"

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
    if (!pixels || width <= 0 || height <= 0 || stride < width * 4) {
        return false;
    }
    const auto rows = static_cast<std::size_t>(height);
    const auto rowBytes = static_cast<std::size_t>(stride);
    if (rows > std::numeric_limits<std::size_t>::max() / rowBytes || length < rows * rowBytes) {
        return false;
    }
    pixels_.assign(pixels, pixels + rows * rowBytes);
    width_ = width;
    height_ = height;
    stride_ = stride;
    invalidate();
    return true;
}

void ImageView::clearImage() {
    if (pixels_.empty()) {
        return;
    }
    pixels_.clear();
    width_ = 0;
    height_ = 0;
    stride_ = 0;
    invalidate();
}

void ImageView::setContentMode(ImageContentMode mode) {
    if (contentMode_ == mode) {
        return;
    }
    contentMode_ = mode;
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
    if (pixels_.empty() || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return;
    }

    Rect target = bounds;
    if (contentMode_ != ImageContentMode::Stretch) {
        const float scaleX = bounds.width / static_cast<float>(width_);
        const float scaleY = bounds.height / static_cast<float>(height_);
        const float scale = contentMode_ == ImageContentMode::Cover
            ? std::max(scaleX, scaleY)
            : std::min(scaleX, scaleY);
        target.width = static_cast<float>(width_) * scale;
        target.height = static_cast<float>(height_) * scale;
        target.x += (bounds.width - target.width) * 0.5f;
        target.y += (bounds.height - target.height) * 0.5f;
    }

    canvas.save();
    canvas.clipRect(bounds);
    canvas.drawPixels(target, pixels_.data(), width_, height_, stride_, CanvasPixelFormat::Rgba8888);
    canvas.restore();
}

} // namespace oneui
