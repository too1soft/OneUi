#include "oneui/controls/realtime_frame_view.h"
#include "oneui/pixel_image.h"
#include "platform/shared/skia_pixel_image.h"
#include "platform/shared/skia_canvas.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"

#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

std::shared_ptr<const oneui::PixelImage> imageFrom(
    const std::shared_ptr<std::vector<std::uint8_t>>& pixels,
    oneui::CanvasPixelFormat format = oneui::CanvasPixelFormat::Rgba8888) {
    return oneui::PixelImage::makeOwned(pixels->data(), pixels->size(), 2, 2, 12, format, pixels);
}

void rasterOwnershipAndColors(oneui::CanvasPixelFormat format) {
    int released = 0;
    auto pixels = std::shared_ptr<std::vector<std::uint8_t>>(
        new std::vector<std::uint8_t>(20, 0), [&](auto* value) { ++released; delete value; });
    // Two pixels per row, padded first row, no trailing padding after last row.
    const auto red = format == oneui::CanvasPixelFormat::Rgba8888 ? 0 : 2;
    for (int offset : {0, 4, 12, 16}) {
        (*pixels)[offset + red] = 255;
        (*pixels)[offset + 3] = 255;
    }
    auto image = imageFrom(pixels, format);
    require(image && image->pixelBytes() == 20, "exact padded layout rejected");
    auto raster = oneui::rendering::skiaPixelImage(image);
    require(raster != nullptr, "raster creation failed");
    SkPixmap source;
    require(raster->peekPixels(&source) && source.addr() == pixels->data(), "raster copied owned pixels");
    require(oneui::rendering::skiaPixelImage(image)->uniqueID() == raster->uniqueID(), "repaint changed image identity");
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
    for (int repaint = 0; repaint < 2; ++repaint) {
        surface->getCanvas()->clear(SK_ColorBLACK);
        auto canvas = oneui::rendering::makeSkiaCanvas(*surface->getCanvas());
        require(dynamic_cast<oneui::PixelImageCanvas*>(canvas.get()) != nullptr, "Skia canvas lacks owned-image extension");
        oneui::drawPixelImage(*canvas, {0, 0, 2, 2}, image);
        require(oneui::rendering::skiaPixelImage(image)->uniqueID() == raster->uniqueID(), "new Canvas recreated raster");
        SkPixmap output;
        require(surface->peekPixels(&output), "output pixels unavailable");
        require(output.getColor(0, 0) == SK_ColorRED && output.getColor(1, 1) == SK_ColorRED,
                "channel order, stride or repaint colors changed");
    }
    pixels.reset();
    image.reset();
    require(released == 0, "pixel owner released before retained raster");
    raster.reset();
    require(released == 1, "pixel owner leaked or released more than once");
}

void layoutAndCachePublication() {
    auto pixels = std::make_shared<std::vector<std::uint8_t>>(20);
    const auto make = [&](int width, int height, int stride, std::size_t size) {
        return oneui::PixelImage::makeOwned(pixels->data(), size, width, height, stride,
                                           oneui::CanvasPixelFormat::Rgba8888, pixels);
    };
    require(!make(2, 2, 12, 19), "short last row accepted");
    require(!make(2, 2, 7, 20), "short stride accepted");
    require(!make(std::numeric_limits<int>::max(), 2, 12, 20), "overflowing row accepted");
    require(!make(0, 2, 12, 20), "empty width accepted");
    require(!oneui::PixelImage::makeOwned(pixels->data(), 20, 2, 2, 12,
        oneui::CanvasPixelFormat::Rgba8888, {}), "unowned buffer accepted");
    auto image = imageFrom(pixels);
    struct Resource : oneui::PixelImageResource {};
    const int key = 0;
    std::vector<std::shared_ptr<oneui::PixelImageResource>> results(8);
    std::vector<std::thread> workers;
    for (auto& result : results) {
        workers.emplace_back([&image, &result, &key]() {
            result = image->rendererResource(&key, [] { return std::make_shared<Resource>(); });
        });
    }
    for (auto& worker : workers) worker.join();
    for (const auto& result : results) require(result == results.front(), "concurrent cache identity differs");
}

class RecordingCanvas : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float) override {}
    void strokeRect(oneui::Rect, oneui::Color, float, float) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float) override {}
    void drawText(const std::wstring&, oneui::Rect, oneui::Color, float, oneui::TextAlign) override {}
    void drawPixels(oneui::Rect, const std::uint8_t* pixels, int, int, int, oneui::CanvasPixelFormat) override {
        ++rawDraws;
        firstByte = pixels[0];
    }
    int rawDraws = 0;
    int firstByte = -1;
};

class ImageCanvas final : public RecordingCanvas, public oneui::PixelImageCanvas {
public:
    void drawPixelImage(oneui::Rect, const std::shared_ptr<const oneui::PixelImage>& value) override {
        ++imageDraws;
        image = value;
    }
    int imageDraws = 0;
    std::shared_ptr<const oneui::PixelImage> image;
};

void frameReuseDamageAndFallback() {
    oneui::RealtimeFrameView view;
    view.setFrame({0, 0, 2, 2});
    int released = 0;
    auto pixels = std::shared_ptr<std::vector<std::uint8_t>>(
        new std::vector<std::uint8_t>(16, 3), [&](auto* value) {
            // Re-entering the view proves the final release occurs outside its mutex.
            (void)view.latestFrame();
            ++released;
            delete value;
        });
    oneui::VideoFrame frame{pixels->data(), 2, 2, 8, oneui::PixelFormat::Rgba8888, 1, 10};
    require(view.submitOwnedFrame(frame, pixels->size(), pixels), "owned frame rejected");
    pixels.reset();
    ImageCanvas first, second;
    view.paint(first);
    view.paint(second);
    require(first.image && first.image == second.image, "view repaint recreated immutable image");
    RecordingCanvas legacy;
    view.paint(legacy);
    require(legacy.rawDraws == 1 && legacy.firstByte == 3, "legacy Canvas fallback changed");
    auto retainedRaster = oneui::rendering::skiaPixelImage(first.image);
    first.image.reset();
    second.image.reset();
    const std::uint8_t patchPixels[]{9, 8, 7, 255};
    oneui::VideoFramePatch patch{patchPixels, sizeof(patchPixels), 0, 0, 1, 1, 4};
    require(view.submitDamage(2, 2, oneui::PixelFormat::Rgba8888, &patch, 1, 2, 20), "damage rejected");
    require(released == 0, "damage freed pixels still referenced by raster");
    view.paint(first);
    require(first.imageDraws == 1 && first.rawDraws == 1 && first.firstByte == 9,
            "damage reused stale immutable pixels");
    SkPixmap oldPixels;
    require(retainedRaster->peekPixels(&oldPixels) && *static_cast<const std::uint8_t*>(oldPixels.addr()) == 3,
            "damage mutated pixels retained by renderer");
    retainedRaster.reset();
    require(released == 1, "retained pixel owner leaked");

    // Exercise the actual damage-release callback without an external image ref.
    auto next = std::shared_ptr<std::vector<std::uint8_t>>(
        new std::vector<std::uint8_t>(16, 4), [&](auto* value) {
            (void)view.latestFrame();
            ++released;
            delete value;
        });
    frame.data = next->data();
    require(view.submitOwnedFrame(frame, next->size(), next), "replacement frame rejected");
    next.reset();
    require(view.submitDamage(2, 2, oneui::PixelFormat::Rgba8888, &patch, 1, 3, 30), "replacement damage rejected");
    require(released == 2, "damage release callback did not complete");
}

} // namespace

int main() {
    try {
        layoutAndCachePublication();
        rasterOwnershipAndColors(oneui::CanvasPixelFormat::Rgba8888);
        rasterOwnershipAndColors(oneui::CanvasPixelFormat::Bgra8888);
        frameReuseDamageAndFallback();
        std::cout << "Owned pixel reuse, colors, stride, lifetime, concurrency, damage and fallback passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
