#pragma once

#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkPixmap.h"
#include "oneui/canvas.h"

namespace oneui::rendering {

class RasterImage {
    const sk_sp<SkData> bytes_;

public:
    RasterImage(sk_sp<SkData> bytes, const SkImageInfo& info, std::size_t stride)
        : bytes_(std::move(bytes)), pixels(info, bytes_->data(), stride),
          skImage(SkImages::RasterFromData(info, bytes_, stride)) {}

    const SkPixmap pixels;
    const sk_sp<SkImage> skImage;
};

// Private capability. Custom canvases can still implement the raw-pixel API.
class RasterImageCanvas {
public:
    virtual void drawRasterImage(Rect target, const RasterImage& image) = 0;

protected:
    ~RasterImageCanvas() = default;
};

} // namespace oneui::rendering
