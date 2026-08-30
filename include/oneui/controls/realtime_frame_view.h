#pragma once

#include "oneui/export.h"
#include "oneui/widget.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace oneui {

enum class PixelFormat {
    Bgra8888,
    Rgba8888,
    Nv12
};

enum class ScaleMode {
    ActualSize,
    Fit,
    Fill,
    Stretch
};

struct VideoFrame {
    const void* data = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
    PixelFormat format = PixelFormat::Bgra8888;
    std::uint64_t frameId = 0;
    std::uint64_t timestampUs = 0;
};

struct VideoFrameSnapshot {
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
    int stride = 0;
    PixelFormat format = PixelFormat::Bgra8888;
    std::uint64_t frameId = 0;
    std::uint64_t timestampUs = 0;
};

class ONEUI_API RealtimeFrameView final : public Widget {
public:
    RealtimeFrameView();

    void submitFrame(const VideoFrame& frame);
    // Transfers an immutable frame buffer into the view without copying it.
    // `owner` must keep `frame.data` alive until the frame is replaced or the
    // view is destroyed. Returns false when metadata or buffer bounds are
    // invalid; in that case the owner is released before returning.
    bool submitOwnedFrame(
        const VideoFrame& frame,
        std::size_t pixelBytes,
        std::shared_ptr<const void> owner);
    void setScaleMode(ScaleMode mode);
    ScaleMode scaleMode() const;
    // setBackgroundColor 设背景/信箱底色（默认黑，适合视频信箱）。设 alpha=0 则不铺底，
    // 让透明像素与身后内容合成——用于显示带透明通道的图（如登录页品牌 logo）。
    void setBackgroundColor(Color color);
    Rect contentRect() const;
    std::optional<VideoFrameSnapshot> latestFrame() const;

    void paint(Canvas& canvas) override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    struct StoredVideoFrame {
        std::shared_ptr<const void> owner;
        const std::uint8_t* pixels = nullptr;
        std::size_t pixelBytes = 0;
        int width = 0;
        int height = 0;
        int stride = 0;
        PixelFormat format = PixelFormat::Bgra8888;
        std::uint64_t frameId = 0;
        std::uint64_t timestampUs = 0;
    };

    Size videoSizeLocked() const;

    mutable std::mutex mutex_;
    ScaleMode scaleMode_ = ScaleMode::Fit;
    Color backgroundColor_{0, 0, 0, 255};
    std::shared_ptr<const StoredVideoFrame> latestFrame_;
};

} // namespace oneui
