#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include "oneui/mobile/remote_viewport.h"

namespace oneui::mobile {
enum class RemoteTouchPhase { Down, Move, Up, Cancel };
enum class RemoteTouchMode { Direct, Trackpad };
struct RemoteTouchSample {
    int id = 0;
    RemoteTouchPhase phase = RemoteTouchPhase::Cancel;
    RemotePoint position;
    uint64_t timeMs = 0;
};
struct RemoteMouseAction {
    RemotePoint position;
    int button = 0; // 0 move, 1 primary, 3 secondary; USB protocol adapter stays in the product.
    bool pressed = false;
    float scrollX = 0, scrollY = 0;
};

// Pure interaction state, shared by desktop touch, Android, iOS and tests.
// No protocol, device identity, pixel buffer or OS permission belongs here.
class RemoteInteraction final {
  public:
    using Sink = std::function<void(const RemoteMouseAction &)>;
    explicit RemoteInteraction(Sink sink = {}) : sink_(std::move(sink)) {}
    void setSink(Sink sink) {
        cancel();
        sink_ = std::move(sink);
    }
    void setViewport(RemoteExtent local, RemoteExtent remote) {
        if (local_.width == local.width && local_.height == local.height &&
            remote_.width == remote.width && remote_.height == remote.height)
            return;
        cancel();
        local_ = local;
        remote_ = remote;
        if (zoom_)
            zoom_->setSizes(local, remote);
        cursor_ = clamp(cursor_);
    }
    void setZoomViewport(RemoteViewport *viewport, std::function<void()> changed) {
        cancel();
        zoom_ = viewport;
        zoomChanged_ = std::move(changed);
        if (zoom_)
            zoom_->setSizes(local_, remote_);
    }
    void setMode(RemoteTouchMode mode) {
        cancel();
        mode_ = mode;
    }
    void setEnabled(bool enabled) {
        if (!enabled)
            cancel();
        enabled_ = enabled;
    }
    RemotePoint cursor() const { return cursor_; }
    void setCursorPosition(RemotePoint point) {
        if (!active() && finite(point))
            cursor_ = clamp(point);
    }
    bool active() const { return !points_.empty(); }
    RemotePoint mapFit(RemotePoint point) const {
        if (!valid())
            return {};
        if (zoom_)
            return clamp(zoom_->project(point));
        const float scale = std::min(local_.width / remote_.width, local_.height / remote_.height);
        return clamp({(point.x - (local_.width - remote_.width * scale) / 2) / scale,
                      (point.y - (local_.height - remote_.height * scale) / 2) / scale});
    }
    void cancel() {
        if (dragging_)
            emit(1, false);
        dragging_ = false;
        multi_ = false;
        moved_ = false;
        zoomGesture_ = false;
        scrolling_ = false;
        pinchStart_ = 0;
        points_.clear();
        lastTap_ = 0;
    }
    void pointer(const RemoteTouchSample &sample) {
        if (sample.phase == RemoteTouchPhase::Cancel) {
            cancel();
            return;
        }
        if (!enabled_ || !valid() || !finite(sample.position))
            return;
        if (sample.phase == RemoteTouchPhase::Down) {
            if (points_.size() >= 10 || points_.count(sample.id))
                return;
            points_[sample.id] = sample.position;
            if (points_.size() > 1) {
                multi_ = true;
                if (zoom_ && points_.size() == 2) {
                    pinchStart_ = distance(points_.begin()->second, points_.rbegin()->second);
                    zoomGesture_ = mode_ == RemoteTouchMode::Direct && zoom_->zoom() > 1.f;
                    multiOrigin_ = midpoint(points_.begin()->second, points_.rbegin()->second);
                    multiLast_ = multiOrigin_;
                    scrolling_ = false;
                }
                if (dragging_) {
                    emit(1, false);
                    dragging_ = false;
                }
                return;
            }
            origin_ = sample.position;
            down_ = sample.timeMs;
            moved_ = false;
            multi_ = false;
            if (mode_ == RemoteTouchMode::Direct)
                cursor_ = mapFit(sample.position);
            if (lastTap_ && (mode_ != RemoteTouchMode::Direct || !zoom_ || zoom_->contains(sample.position)) &&
                sample.timeMs >= lastTap_ && sample.timeMs - lastTap_ < 300 &&
                distance(sample.position, lastTapPosition_) < 24) {
                dragging_ = true;
                emit(1, true);
            }
            return;
        }
        auto current = points_.find(sample.id);
        if (current == points_.end())
            return;
        const auto previous = current->second;
        current->second = sample.position;
        if (sample.phase == RemoteTouchPhase::Move) {
            moved_ |= distance(origin_, sample.position) > 6;
            if (multi_) {
                moveMultiple(sample, previous, current == points_.begin());
            } else {
                if (mode_ == RemoteTouchMode::Direct)
                    cursor_ = mapFit(sample.position);
                else
                    cursor_ = clamp({cursor_.x + (sample.position.x - previous.x) * 1.5f,
                                     cursor_.y + (sample.position.y - previous.y) * 1.5f});
                emit(0, false);
            }
            return;
        }
        if (sample.phase != RemoteTouchPhase::Up)
            return;
        points_.erase(current);
        if (!points_.empty())
            return;
        if (dragging_) {
            emit(1, false);
            dragging_ = false;
            lastTap_ = 0;
        } else if (!multi_ && !moved_ &&
                   (mode_ != RemoteTouchMode::Direct || !zoom_ || zoom_->contains(sample.position))) {
            if (mode_ == RemoteTouchMode::Direct)
                cursor_ = mapFit(sample.position);
            const auto elapsed = sample.timeMs >= down_ ? sample.timeMs - down_ : 0;
            const int button = elapsed >= 500 ? 3 : 1;
            emit(button, true);
            emit(button, false);
            lastTap_ = button == 1 ? sample.timeMs : 0;
            lastTapPosition_ = sample.position;
        } else
            lastTap_ = 0;
        multi_ = false;
        zoomGesture_ = false;
    }

  private:
    void moveMultiple(const RemoteTouchSample &sample, RemotePoint previous, bool primary) {
        if (zoom_ && points_.size() == 2) {
            const auto other = primary ? points_.rbegin()->second : points_.begin()->second;
            float oldDistance = distance(previous, other);
            const float newDistance = distance(sample.position, other);
            auto from = midpoint(previous, other);
            const auto to = midpoint(sample.position, other);
            if (!zoomGesture_ && !scrolling_ && std::abs(newDistance - pinchStart_) >= 12.f) {
                zoomGesture_ = true;
                oldDistance = pinchStart_;
                from = multiOrigin_;
            }
            if (zoomGesture_) {
                if (oldDistance > 1.f && zoom_->transform(from, to, newDistance / oldDistance) && zoomChanged_)
                    zoomChanged_();
                return;
            }
            if (mode_ == RemoteTouchMode::Trackpad && (scrolling_ || distance(to, multiOrigin_) >= 6)) {
                scrolling_ = true;
                emit(0, false, (multiLast_.x - to.x) * 2, (multiLast_.y - to.y) * 2);
                multiLast_ = to;
            }
            return;
        }
        // Consumers without a zoom viewport retain their two-finger scroll mapping.
        if (points_.size() >= 2 && primary)
            emit(0, false, (previous.x - sample.position.x) * 2, (previous.y - sample.position.y) * 2);
    }
    static RemotePoint midpoint(RemotePoint a, RemotePoint b) {
        return {(a.x + b.x) / 2, (a.y + b.y) / 2};
    }
    static bool finite(RemotePoint p) { return std::isfinite(p.x) && std::isfinite(p.y); }
    bool valid() const {
        return local_.width > 0 && local_.height > 0 && remote_.width > 0 && remote_.height > 0 &&
               std::isfinite(local_.width) && std::isfinite(local_.height) &&
               std::isfinite(remote_.width) && std::isfinite(remote_.height);
    }
    static float distance(RemotePoint a, RemotePoint b) { return std::hypot(a.x - b.x, a.y - b.y); }
    RemotePoint clamp(RemotePoint p) const {
        return {std::clamp(p.x, 0.f, std::max(0.f, remote_.width - 1)),
                std::clamp(p.y, 0.f, std::max(0.f, remote_.height - 1))};
    }
    void emit(int button, bool pressed, float x = 0, float y = 0) {
        if (sink_)
            sink_({cursor_, button, pressed, x, y});
    }
    Sink sink_;
    RemoteViewport *zoom_ = nullptr;
    std::function<void()> zoomChanged_;
    RemoteExtent local_, remote_;
    RemotePoint cursor_, origin_, lastTapPosition_, multiOrigin_, multiLast_;
    std::map<int, RemotePoint> points_;
    uint64_t down_ = 0, lastTap_ = 0;
    RemoteTouchMode mode_ = RemoteTouchMode::Trackpad;
    float pinchStart_ = 0;
    bool enabled_ = true, moved_ = false, multi_ = false, dragging_ = false, zoomGesture_ = false, scrolling_ = false;
};
} // namespace oneui::mobile
