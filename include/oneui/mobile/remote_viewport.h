#pragma once

#include <algorithm>
#include <cmath>

namespace oneui::mobile {
struct RemotePoint {
    float x = 0, y = 0;
};
struct RemoteExtent {
    float width = 0, height = 0;
};

// A single transform for drawing and inverse pointer mapping. Coordinates are
// local to the view. Zoom preserves the pixel under the gesture midpoint.
class RemoteViewport final {
  public:
    void setSizes(RemoteExtent local, RemoteExtent remote) {
        if (local.width == local_.width && local.height == local_.height &&
            remote.width == remote_.width && remote.height == remote_.height)
            return;
        const auto oldCenter = center();
        const auto anchor = project(oldCenter);
        const bool sameDisplay = valid() && remote.width == remote_.width && remote.height == remote_.height;
        local_ = local;
        remote_ = remote;
        if (sameDisplay && valid())
            place(anchor, center());
        else
            reset();
    }
    void reset() {
        zoom_ = 1;
        origin_ = {};
        constrain();
    }
    bool valid() const {
        return positive(local_.width) && positive(local_.height) &&
               positive(remote_.width) && positive(remote_.height);
    }
    float zoom() const { return zoom_; }
    float scale() const {
        return valid() ? std::min(local_.width / remote_.width, local_.height / remote_.height) * zoom_ : 1.f;
    }
    RemotePoint origin() const { return origin_; }
    RemoteExtent contentSize() const { return {remote_.width * scale(), remote_.height * scale()}; }
    RemotePoint project(RemotePoint local) const {
        return {(local.x - origin_.x) / scale(), (local.y - origin_.y) / scale()};
    }
    RemotePoint toLocal(RemotePoint remote) const {
        return {origin_.x + remote.x * scale(), origin_.y + remote.y * scale()};
    }
    bool contains(RemotePoint local) const {
        if (!valid())
            return false;
        const auto point = project(local);
        return point.x >= 0 && point.y >= 0 && point.x < remote_.width && point.y < remote_.height;
    }
    bool transform(RemotePoint from, RemotePoint to, float factor) {
        if (!valid() || !positive(factor))
            return false;
        const auto anchor = project(from);
        const auto previous = origin_;
        const float previousZoom = zoom_;
        zoom_ = std::clamp(zoom_ * factor, 1.f, 5.f);
        place(anchor, to);
        return previousZoom != zoom_ || previous.x != origin_.x || previous.y != origin_.y;
    }

  private:
    static bool positive(float n) { return n > 0 && std::isfinite(n); }
    RemotePoint center() const { return {local_.width / 2, local_.height / 2}; }
    void place(RemotePoint anchor, RemotePoint at) {
        origin_ = {at.x - anchor.x * scale(), at.y - anchor.y * scale()};
        constrain();
    }
    void constrain() {
        if (!valid())
            return;
        const auto content = contentSize();
        const auto axis = [](float local, float content, float origin) {
            return content <= local ? (local - content) / 2 : std::clamp(origin, local - content, 0.f);
        };
        origin_ = {axis(local_.width, content.width, origin_.x), axis(local_.height, content.height, origin_.y)};
    }
    RemoteExtent local_, remote_;
    RemotePoint origin_;
    float zoom_ = 1;
};
} // namespace oneui::mobile
