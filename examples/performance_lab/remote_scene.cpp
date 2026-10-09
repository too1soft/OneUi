#include "remote_scene.hpp"
#include <chrono>
#include <stdexcept>

namespace lab {
using namespace oneui;
namespace {
constexpr int width = 640, height = 360;
std::uint64_t timestamp() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
} // namespace
RemoteScene::RemoteScene() {
    input->setRemoteSize({width, height});
    submit(kind_);
}
void RemoteScene::submit(FrameKind kind) {
    kind_ = kind;
    const auto sequence = ++sequence_;
    auto pixels = std::make_shared<std::vector<std::uint8_t>>(width * height * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool check = ((x + int(sequence % 80) * 8) / 32 + y / 32) % 2;
            const auto at = (y * width + x) * 4;
            const std::uint8_t r = check ? 196 : 48, g = check ? 237 : 98, b = check ? 135 : 78;
            (*pixels)[at] = kind == FrameKind::Bgra ? b : r;
            (*pixels)[at + 1] = g;
            (*pixels)[at + 2] = kind == FrameKind::Bgra ? r : b;
            (*pixels)[at + 3] = 255;
        }
    }
    bytes_ = pixels->size();
    const auto format = kind == FrameKind::Bgra ? PixelFormat::Bgra8888 : PixelFormat::Rgba8888;
    const VideoFrame video{pixels->data(), width, height, width * 4, format, sequence, timestamp()};
    if (kind == FrameKind::OwnedRgba) {
        owner_ = pixels;
        if (!frame->submitOwnedFrame(video, bytes_, pixels))
            throw std::runtime_error("Synthetic owned frame rejected");
    } else {
        frame->submitFrame(video);
    }
}
bool RemoteScene::probeNv12() {
    auto pixels = std::make_shared<std::vector<std::uint8_t>>(6, 128);
    return frame->submitOwnedFrame(
        {pixels->data(), 2, 2, 2, PixelFormat::Nv12, sequence_ + 1, timestamp()}, pixels->size(),
        pixels);
}
bool RemoteScene::damage() {
    constexpr int edge = 48;
    std::vector<std::uint8_t> patch(edge * edge * 4, 255);
    const int x = int((sequence_ * 53) % (width - edge));
    const int y = int((sequence_ * 29) % (height - edge));
    const VideoFramePatch update{patch.data(), patch.size(), x, y, edge, edge, edge * 4};
    const auto format = kind_ == FrameKind::Bgra ? PixelFormat::Bgra8888 : PixelFormat::Rgba8888;
    if (!frame->submitDamage(width, height, format, &update, 1, sequence_ + 1, timestamp()))
        return false;
    ++sequence_;
    return true;
}
void RemoteScene::cursor(RemoteCursorMode mode) {
    if (mode == RemoteCursorMode::Bitmap) {
        constexpr int edge = 20;
        std::vector<std::uint8_t> bitmap(edge * edge * 4, 0);
        for (int y = 0; y < edge; ++y)
            for (int x = 0; x < edge; ++x)
                if (x == edge / 2 || y == edge / 2) {
                    const int at = (y * edge + x) * 4;
                    bitmap[at] = 196;
                    bitmap[at + 1] = 237;
                    bitmap[at + 2] = 135;
                    bitmap[at + 3] = 255;
                }
        input->setRemoteCursorBitmap(bitmap.data(), bitmap.size(), edge, edge, edge * 4, edge / 2,
                                     edge / 2);
        input->setRemoteCursorPosition({width / 2, height / 2});
    }
    input->setRemoteCursorMode(mode);
}
} // namespace lab
