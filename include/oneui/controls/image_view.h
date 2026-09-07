#pragma once

#include "oneui/widget.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace oneui {

enum class ImageContentMode {
    Contain = 0,
    Cover = 1,
    Stretch = 2,
};

/// Displays an owned RGBA image without exposing a renderer-specific image type.
/// Pixel data is copied by setRgbaPixels, so callers may release their buffer
/// immediately after the call returns.
class ONEUI_API ImageView final : public Widget {
public:
    ImageView();

    bool setRgbaPixels(const std::uint8_t* pixels, std::size_t length, int width, int height, int stride);
    void clearImage();
    bool hasImage() const { return !pixels_.empty(); }
    int imageWidth() const { return width_; }
    int imageHeight() const { return height_; }

    void setContentMode(ImageContentMode mode);
    ImageContentMode contentMode() const { return contentMode_; }
    void setCornerRadius(float radius);
    float cornerRadius() const { return cornerRadius_; }
    void setBackground(Color color);
    Color background() const { return background_; }

    void paint(Canvas& canvas) override;

private:
    std::vector<std::uint8_t> pixels_;
    int width_ = 0;
    int height_ = 0;
    int stride_ = 0;
    ImageContentMode contentMode_ = ImageContentMode::Contain;
    float cornerRadius_ = 0.0f;
    Color background_{0, 0, 0, 0};
};

} // namespace oneui
