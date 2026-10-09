#pragma once
#include <oneui/controls/realtime_frame_view.h>
#include <oneui/controls/remote_input_region.h>

namespace lab {
// Local synthetic data only. No transport, codec or remote connection is opened.
class RemoteScene {
  public:
    enum class FrameKind { Rgba, Bgra, OwnedRgba };
    std::shared_ptr<oneui::RealtimeFrameView> frame = std::make_shared<oneui::RealtimeFrameView>();
    std::shared_ptr<oneui::RemoteInputRegion> input = std::make_shared<oneui::RemoteInputRegion>();
    RemoteScene();
    void submit(FrameKind);
    bool damage();
    bool probeNv12();
    void cursor(oneui::RemoteCursorMode);
    std::uint64_t sequence() const { return sequence_; }
    std::size_t bytes() const { return bytes_; }
    std::weak_ptr<const void> ownedBuffer() const { return owner_; }
    FrameKind kind() const { return kind_; }

  private:
    FrameKind kind_ = FrameKind::Rgba;
    std::uint64_t sequence_ = 0;
    std::size_t bytes_ = 0;
    std::weak_ptr<const void> owner_;
};
} // namespace lab
