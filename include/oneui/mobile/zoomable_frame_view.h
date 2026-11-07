#pragma once

#include "oneui/canvas.h"
#include "oneui/controls/realtime_frame_view.h"
#include "oneui/mobile/remote_viewport.h"
#include "oneui/mobile/action_icons.h"

namespace oneui::mobile {
// Hosts a frame renderer with a clipped pan/zoom transform. Frame storage and
// the rendering backend remain in RealtimeFrameView.
class ZoomableFrameView final : public Widget {
  public:
    explicit ZoomableFrameView(RemoteViewport &viewport) : viewport_(viewport) {
        renderer_.setScaleMode(ScaleMode::Stretch);
        renderer_.setBackgroundColor({0, 0, 0, 0});
        cursorRenderer_.setScaleMode(ScaleMode::Stretch);
        cursorRenderer_.setBackgroundColor({0, 0, 0, 0});
    }
    bool submitOwnedFrame(const VideoFrame &frame, std::size_t bytes, std::shared_ptr<const void> owner) {
        if (!renderer_.submitOwnedFrame(frame, bytes, std::move(owner)))
            return false;
        invalidate();
        return true;
    }
    void viewportChanged() { invalidate(); }
    bool submitCursorFrame(const VideoFrame &frame, std::size_t bytes, std::shared_ptr<const void> owner,
                           RemotePoint hotspot) {
        if (!std::isfinite(hotspot.x) || !std::isfinite(hotspot.y) || hotspot.x < 0 || hotspot.y < 0 ||
            hotspot.x >= frame.width || hotspot.y >= frame.height ||
            !cursorRenderer_.submitOwnedFrame(frame, bytes, std::move(owner)))
            return false;
        cursorSize_ = {float(frame.width), float(frame.height)};
        hotspot_ = hotspot;
        invalidate();
        return true;
    }
    void setCursorPosition(RemotePoint position) {
        if (!std::isfinite(position.x) || !std::isfinite(position.y))
            return;
        cursor_ = position;
        cursorPositionKnown_ = true;
        invalidate();
    }
    void setCursorVisible(bool visible) {
        cursorVisible_ = visible;
        invalidate();
    }
    void clearCursorFrame() {
        cursorSize_ = {};
        invalidate();
    }
    void paint(Canvas &canvas) override {
        const auto bounds = frame();
        canvas.fillRect(bounds, {13, 15, 19}, 0.f);
        if (!viewport_.valid())
            return;
        const auto origin = viewport_.origin();
        const auto extent = viewport_.contentSize();
        renderer_.setFrame({bounds.x + origin.x, bounds.y + origin.y, extent.width, extent.height});
        canvas.save();
        canvas.clipRect(bounds);
        renderer_.paint(canvas);
        if (cursorVisible_ && cursorPositionKnown_) {
            const auto point = viewport_.toLocal(cursor_);
            if (viewport_.contains(point)) {
                if (cursorSize_.width > 0) {
                    const float minimumScale = std::min(1.f, 24.f / std::max(cursorSize_.width, cursorSize_.height));
                    const float scale = std::max(viewport_.scale(), minimumScale);
                    cursorRenderer_.setFrame({bounds.x + point.x - hotspot_.x * scale,
                                             bounds.y + point.y - hotspot_.y * scale,
                                             cursorSize_.width * scale, cursorSize_.height * scale});
                    cursorRenderer_.paint(canvas);
                } else {
                    const Rect pointer{bounds.x + point.x - 4, bounds.y + point.y - 2, 24, 24};
                    paintPointerIcon(canvas, pointer, {16, 18, 22});
                    paintPointerIcon(canvas, {pointer.x + 1, pointer.y + 1, 22, 22}, {255, 255, 255});
                }
            }
        }
        canvas.restore();
    }
    AccessibilityInfo accessibilityInfo() const override {
        auto info = Widget::accessibilityInfo();
        info.role = AccessibilityRole::Custom;
        return info;
    }

  private:
    RemoteViewport &viewport_;
    RealtimeFrameView renderer_;
    RealtimeFrameView cursorRenderer_;
    RemoteExtent cursorSize_;
    RemotePoint cursor_, hotspot_;
    bool cursorVisible_ = true, cursorPositionKnown_ = false;
};
} // namespace oneui::mobile
