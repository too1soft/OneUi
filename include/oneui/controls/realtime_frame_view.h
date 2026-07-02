#pragma once

#include "oneui/export.h"
#include "oneui/widget.h"

#include <cstdint>
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
    void setScaleMode(ScaleMode mode);
    ScaleMode scaleMode() const;
    Rect contentRect() const;
    std::optional<VideoFrameSnapshot> latestFrame() const;

    void paint(Canvas& canvas) override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    Size videoSizeLocked() const;

    mutable std::mutex mutex_;
    ScaleMode scaleMode_ = ScaleMode::Fit;
    std::optional<VideoFrameSnapshot> latestFrame_;
};

} // namespace oneui
