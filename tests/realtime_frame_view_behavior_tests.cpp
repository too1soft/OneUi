#include "oneui/controls/realtime_frame_view.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

class NullCanvas final : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float = 0.0f) override {}
    void strokeRect(oneui::Rect, oneui::Color, float, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring&, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {}
};

class RecordingCanvas final : public oneui::Canvas {
public:
    void save() override {}
    void restore() override {}
    void clipRect(oneui::Rect) override {}
    void clear(oneui::Color) override {}
    void fillRect(oneui::Rect, oneui::Color, float = 0.0f) override {
        ++fillCount;
    }
    void strokeRect(oneui::Rect, oneui::Color, float, float = 1.0f) override {}
    void fillEllipse(oneui::Rect, oneui::Color) override {}
    void strokeEllipse(oneui::Rect, oneui::Color, float = 1.0f) override {}
    void drawLine(oneui::Point, oneui::Point, oneui::Color, float = 1.0f) override {}
    void drawText(const std::wstring&, oneui::Rect, oneui::Color, float, oneui::TextAlign = oneui::TextAlign::Center) override {}
    void drawPixels(oneui::Rect rect, const std::uint8_t*, int width, int height, int stride, oneui::CanvasPixelFormat format) override {
        ++drawPixelsCount;
        lastRect = rect;
        lastWidth = width;
        lastHeight = height;
        lastStride = stride;
        lastFormat = format;
    }

    int fillCount = 0;
    int drawPixelsCount = 0;
    int lastWidth = 0;
    int lastHeight = 0;
    int lastStride = 0;
    oneui::Rect lastRect;
    oneui::CanvasPixelFormat lastFormat = oneui::CanvasPixelFormat::Bgra8888;
};

void expectEqual(const char* name, int actual, int expected) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectUInt64Equal(const char* name, std::uint64_t actual, std::uint64_t expected) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectNear(const char* name, float actual, float expected) {
    if (std::fabs(actual - expected) > 0.001f) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        ++failures;
    }
}

void expectRect(const char* name, oneui::Rect actual, oneui::Rect expected) {
    expectNear((std::string(name) + " x").c_str(), actual.x, expected.x);
    expectNear((std::string(name) + " y").c_str(), actual.y, expected.y);
    expectNear((std::string(name) + " width").c_str(), actual.width, expected.width);
    expectNear((std::string(name) + " height").c_str(), actual.height, expected.height);
}

void submitSizedFrame(oneui::RealtimeFrameView& view, int width, int height) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0x7f);
    view.submitFrame(oneui::VideoFrame{pixels.data(), width, height, width * 4, oneui::PixelFormat::Bgra8888, 1, 100});
}

void testContentRectScaleModes() {
    oneui::RealtimeFrameView view;
    view.setFrame(oneui::Rect{10.0f, 20.0f, 800.0f, 600.0f});
    submitSizedFrame(view, 1920, 1080);

    view.setScaleMode(oneui::ScaleMode::Fit);
    expectRect("RealtimeFrameView fit content rect", view.contentRect(), oneui::Rect{10.0f, 95.0f, 800.0f, 450.0f});

    view.setScaleMode(oneui::ScaleMode::Fill);
    expectRect("RealtimeFrameView fill content rect", view.contentRect(), oneui::Rect{-123.333313f, 20.0f, 1066.666626f, 600.0f});

    view.setScaleMode(oneui::ScaleMode::Stretch);
    expectRect("RealtimeFrameView stretch content rect", view.contentRect(), oneui::Rect{10.0f, 20.0f, 800.0f, 600.0f});

    view.setScaleMode(oneui::ScaleMode::ActualSize);
    expectRect("RealtimeFrameView actual size content rect", view.contentRect(), oneui::Rect{-550.0f, -220.0f, 1920.0f, 1080.0f});
}

void testSubmitFrameKeepsOnlyLatestSnapshot() {
    oneui::RealtimeFrameView view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 500.0f, 500.0f});

    const std::vector<std::uint8_t> first{
        1, 2, 3, 4,
        5, 6, 7, 8
    };
    view.submitFrame(oneui::VideoFrame{first.data(), 1, 2, 4, oneui::PixelFormat::Bgra8888, 7, 700});

    std::vector<std::uint8_t> latest{
        10, 11, 12, 13, 0, 0, 0, 0,
        14, 15, 16, 17, 0, 0, 0, 0
    };
    view.submitFrame(oneui::VideoFrame{latest.data(), 1, 2, 8, oneui::PixelFormat::Rgba8888, 8, 900});

    latest[0] = 99;

    const auto snapshot = view.latestFrame();
    expectEqual("RealtimeFrameView latest snapshot exists", snapshot.has_value() ? 1 : 0, 1);
    if (!snapshot) {
        return;
    }

    expectEqual("RealtimeFrameView latest width", snapshot->width, 1);
    expectEqual("RealtimeFrameView latest height", snapshot->height, 2);
    expectEqual("RealtimeFrameView latest stride", snapshot->stride, 4);
    expectEqual("RealtimeFrameView latest format", static_cast<int>(snapshot->format), static_cast<int>(oneui::PixelFormat::Rgba8888));
    expectUInt64Equal("RealtimeFrameView latest frameId", snapshot->frameId, 8);
    expectUInt64Equal("RealtimeFrameView latest timestamp", snapshot->timestampUs, 900);
    expectEqual("RealtimeFrameView latest copied pixel count", static_cast<int>(snapshot->pixels.size()), 8);
    expectEqual("RealtimeFrameView latest copied first pixel survives caller mutation", snapshot->pixels[0], 10);
    expectEqual("RealtimeFrameView latest copied second row", snapshot->pixels[4], 14);
}

void testOwnedFrameRetainsAllocationWithoutCopyingOnSubmit() {
    oneui::RealtimeFrameView view;
    auto pixels = std::make_shared<std::vector<std::uint8_t>>(std::initializer_list<std::uint8_t>{
        10, 11, 12, 13, 0, 0, 0, 0,
        14, 15, 16, 17, 0, 0, 0, 0,
    });
    std::weak_ptr<std::vector<std::uint8_t>> lifetime = pixels;
    std::shared_ptr<const void> owner(pixels, static_cast<const void*>(pixels->data()));
    const bool accepted = view.submitOwnedFrame(
        oneui::VideoFrame{pixels->data(), 1, 2, 8, oneui::PixelFormat::Rgba8888, 17, 1700},
        pixels->size(),
        std::move(owner));
    pixels.reset();

    expectEqual("RealtimeFrameView accepts owned frame", accepted ? 1 : 0, 1);
    expectEqual("RealtimeFrameView retains owned frame allocation", lifetime.expired() ? 1 : 0, 0);
    const auto snapshot = view.latestFrame();
    expectEqual("RealtimeFrameView owned snapshot exists", snapshot.has_value() ? 1 : 0, 1);
    if (snapshot) {
        expectEqual("RealtimeFrameView owned snapshot compacts stride", snapshot->stride, 4);
        expectEqual("RealtimeFrameView owned snapshot second row", snapshot->pixels[4], 14);
    }

    submitSizedFrame(view, 1, 1);
    expectEqual("RealtimeFrameView replacement releases owned allocation", lifetime.expired() ? 1 : 0, 1);
}

void testEmptyFrameDoesNotCrash() {
    oneui::RealtimeFrameView view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 320.0f, 180.0f});

    view.submitFrame(oneui::VideoFrame{});
    expectEqual("RealtimeFrameView empty frame leaves no snapshot", view.latestFrame().has_value() ? 1 : 0, 0);
    expectRect("RealtimeFrameView empty frame content rect", view.contentRect(), oneui::Rect{0.0f, 0.0f, 320.0f, 180.0f});

    NullCanvas canvas;
    view.paint(canvas);
}

void testPaintDrawsLatestFramePixelsIntoContentRect() {
    oneui::RealtimeFrameView view;
    view.setFrame(oneui::Rect{0.0f, 0.0f, 800.0f, 600.0f});
    submitSizedFrame(view, 1920, 1080);

    RecordingCanvas canvas;
    view.paint(canvas);

    expectEqual("RealtimeFrameView paint draws pixels once", canvas.drawPixelsCount, 1);
    expectEqual("RealtimeFrameView paint fills background only", canvas.fillCount, 1);
    expectRect("RealtimeFrameView paint pixel rect", canvas.lastRect, oneui::Rect{0.0f, 75.0f, 800.0f, 450.0f});
    expectEqual("RealtimeFrameView paint pixel width", canvas.lastWidth, 1920);
    expectEqual("RealtimeFrameView paint pixel height", canvas.lastHeight, 1080);
    expectEqual("RealtimeFrameView paint pixel stride", canvas.lastStride, 1920 * 4);
    expectEqual("RealtimeFrameView paint pixel format", static_cast<int>(canvas.lastFormat), static_cast<int>(oneui::CanvasPixelFormat::Bgra8888));
}

} // namespace

int main() {
    testContentRectScaleModes();
    testSubmitFrameKeepsOnlyLatestSnapshot();
    testOwnedFrameRetainsAllocationWithoutCopyingOnSubmit();
    testEmptyFrameDoesNotCrash();
    testPaintDrawsLatestFramePixelsIntoContentRect();

    if (failures != 0) {
        std::cerr << failures << " realtime frame view behavior test(s) failed.\n";
        return 1;
    }

    return 0;
}
