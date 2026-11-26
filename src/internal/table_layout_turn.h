#pragma once

#include "oneui/geometry.h"
#include <chrono>
#include <optional>

namespace oneui::internal {

// A first viewport may keep prepared rows detached between message turns.
// Only the platform's startup preparation pass sets a finite row budget.
class TableLayoutTurn {
public:
    explicit TableLayoutTurn(std::chrono::milliseconds budget) noexcept
        : TableLayoutTurn(std::nullopt) {
        deadline_ = Clock::now() + budget;
    }

    explicit TableLayoutTurn(std::optional<Point> pointer) noexcept
        : previous_(current), pointer_(pointer) { current = this; }
    ~TableLayoutTurn() { current = previous_; }
    TableLayoutTurn(const TableLayoutTurn&) = delete;
    TableLayoutTurn& operator=(const TableLayoutTurn&) = delete;

    bool skipUnpublishedViewport(Rect clip) const {
        return pointer_ && !clip.contains(*pointer_);
    }

    bool budgetElapsed() const {
        return deadline_ && (deferred_ || Clock::now() >= *deadline_);
    }

    bool deferred() const { return deferred_; }
    void defer() { deferred_ = true; }

    inline static thread_local TableLayoutTurn* current = nullptr;

private:
    using Clock = std::chrono::steady_clock;
    TableLayoutTurn* previous_;
    std::optional<Clock::time_point> deadline_;
    std::optional<Point> pointer_;
    bool deferred_ = false;
};

} // namespace oneui::internal
