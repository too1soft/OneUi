#include "skia_pixel_image.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSamplingOptions.h"

namespace oneui::rendering {
namespace {

const int skiaBackendKey = 0;

struct SkiaPixelResource final : PixelImageResource {
    explicit SkiaPixelResource(sk_sp<SkImage> value) : image(std::move(value)) {}
    sk_sp<SkImage> image;
};

SkImageInfo imageInfo(int width, int height, CanvasPixelFormat format) {
    return SkImageInfo::Make(width, height,
        format == CanvasPixelFormat::Rgba8888 ? kRGBA_8888_SkColorType : kBGRA_8888_SkColorType,
        kPremul_SkAlphaType);
}

void paintImage(SkCanvas& canvas, Rect rect, const sk_sp<SkImage>& image) {
    if (!image || rect.width <= 0.0f || rect.height <= 0.0f) return;
    SkPaint paint;
    paint.setAntiAlias(true);
    // Preserve existing pixel scaling quality for both owned and raw images.
    canvas.drawImageRect(image, SkRect::MakeXYWH(rect.x, rect.y, rect.width, rect.height),
                         SkSamplingOptions(SkCubicResampler::Mitchell()), &paint);
}

} // namespace

sk_sp<SkImage> skiaPixelImage(const std::shared_ptr<const PixelImage>& image) {
    if (!image) return {};
    auto resource = image->rendererResource(&skiaBackendKey, [&]() -> std::shared_ptr<PixelImageResource> {
        // Retain only the original pixels, not PixelImage (which owns this cache).
        auto owner = std::make_unique<std::shared_ptr<const void>>(image->pixelOwner());
        auto data = SkData::MakeWithProc(image->pixels(), image->pixelBytes(),
            [](const void*, void* context) {
                delete static_cast<std::shared_ptr<const void>*>(context);
            }, owner.get());
        if (!data) return {};
        owner.release();
        auto raster = SkImages::RasterFromData(
            imageInfo(image->width(), image->height(), image->format()),
            std::move(data), static_cast<std::size_t>(image->stride()));
        if (!raster) return {};
        return std::make_shared<SkiaPixelResource>(std::move(raster));
    });
    return resource ? static_cast<SkiaPixelResource&>(*resource).image : nullptr;
}

void drawSkiaPixelImage(SkCanvas& canvas, Rect rect,
                        const std::shared_ptr<const PixelImage>& image) {
    paintImage(canvas, rect, skiaPixelImage(image));
}

void drawSkiaPixels(SkCanvas& canvas, Rect rect, const std::uint8_t* pixels,
                    int width, int height, int stride, CanvasPixelFormat format) {
    if (!pixels || width <= 0 || height <= 0 || stride <= 0 ||
        rect.width <= 0.0f || rect.height <= 0.0f) return;
    const SkPixmap pixmap(imageInfo(width, height, format), pixels, static_cast<std::size_t>(stride));
    paintImage(canvas, rect, SkImages::RasterFromPixmapCopy(pixmap));
}

} // namespace oneui::rendering
