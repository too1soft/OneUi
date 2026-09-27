#pragma once
#include <array>
#include <chrono>
#include <cstddef>

namespace oneui::internal {
// Opt-in wall-clock attribution. Categories can overlap; recursive spans of
// the same category are counted only once. This is not GPU timestamp timing.
enum class FrameStage : std::size_t { Layout, TextLayout, Fill, Path, Count };
struct FrameProfile {
    std::array<double, static_cast<std::size_t>(FrameStage::Count)> ms{};
    std::array<unsigned, static_cast<std::size_t>(FrameStage::Count)> depth{};
};
inline thread_local FrameProfile* activeFrameProfile = nullptr;
class FrameProfileSession {
    FrameProfile* previous_;
public:
    FrameProfile data;
    explicit FrameProfileSession(bool enabled) : previous_(activeFrameProfile) {
        activeFrameProfile = enabled ? &data : nullptr;
    }
    ~FrameProfileSession() { activeFrameProfile = previous_; }
    FrameProfileSession(const FrameProfileSession&) = delete;
    FrameProfileSession& operator=(const FrameProfileSession&) = delete;
};
class FrameSpan {
    FrameProfile* profile_ = activeFrameProfile;
    std::size_t index_;
    std::chrono::steady_clock::time_point start_;
public:
    explicit FrameSpan(FrameStage stage) : index_(static_cast<std::size_t>(stage)) {
        if (profile_ && profile_->depth[index_]++ == 0) start_ = std::chrono::steady_clock::now();
    }
    ~FrameSpan() {
        if (profile_ && --profile_->depth[index_] == 0)
            profile_->ms[index_] += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_).count();
    }
    FrameSpan(const FrameSpan&) = delete;
    FrameSpan& operator=(const FrameSpan&) = delete;
};
} // namespace oneui::internal
