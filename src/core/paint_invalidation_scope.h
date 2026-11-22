#pragma once

namespace oneui::detail {

// Propagate a paint-only rectangle through the existing synchronous chain.
class PaintInvalidationScope {
public:
    explicit PaintInvalidationScope(bool paintOnly);
    ~PaintInvalidationScope();

    PaintInvalidationScope(const PaintInvalidationScope&) = delete;
    PaintInvalidationScope& operator=(const PaintInvalidationScope&) = delete;

private:
    bool previous_;
};

} // namespace oneui::detail
