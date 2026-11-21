#pragma once

#include <array>
#include <chrono>
#include <cstddef>

namespace oneui::detail::view_mount_timing {

enum Stage : std::size_t {
    Instantiate, ReadImage, StyleParse, Build, Create, Style, ThemeReplay,
    Properties, Events, InitialUpdate, BaseLayout, Publish, Count
};

inline constexpr std::array<const char*, Count> names{
    "instantiate", "read_image", "style_parse", "build", "create", "style", "theme_replay",
    "properties", "events", "initial_update", "base_layout", "publish"
};

using Clock = std::chrono::steady_clock;
using Duration = Clock::duration;

struct Measurements {
    std::array<Duration, Count> exclusive{};
    std::array<unsigned, Count> calls{};
    Duration total{};
};

class Scope;
inline thread_local Scope* current = nullptr;

class Scope {
public:
    explicit Scope(Stage stage, Measurements* root = nullptr) noexcept
        : parent_(current), data_(root ? root : parent_ ? parent_->data_ : nullptr), stage_(stage) {
        if (!data_) return;
        start_ = Clock::now();
        current = this;
    }

    ~Scope() {
        if (!data_) return;
        const auto elapsed = Clock::now() - start_;
        data_->exclusive[stage_] += elapsed - children_;
        ++data_->calls[stage_];
        if (parent_ && parent_->data_ == data_) parent_->children_ += elapsed;
        if (stage_ == Instantiate) data_->total = elapsed;
        current = parent_;
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    Scope* parent_;
    Measurements* data_;
    Stage stage_;
    Clock::time_point start_{};
    Duration children_{};
};

} // namespace oneui::detail::view_mount_timing
