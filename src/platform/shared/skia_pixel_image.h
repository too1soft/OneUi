#pragma once

#include "oneui/pixel_image.h"
#include "include/core/SkImage.h"

class SkCanvas;

namespace oneui::rendering {

sk_sp<SkImage> skiaPixelImage(const std::shared_ptr<const PixelImage>& image);
void drawSkiaPixelImage(SkCanvas& canvas, Rect rect,
                        const std::shared_ptr<const PixelImage>& image);
void drawSkiaPixels(SkCanvas& canvas, Rect rect, const std::uint8_t* pixels,
                    int width, int height, int stride, CanvasPixelFormat format);

} // namespace oneui::rendering
