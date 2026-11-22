#pragma once

namespace oneui::detail {

// Carry paint and layout damage through the existing synchronous chain.
class PaintInvalidationScope {
public:
    explicit PaintInvalidationScope(bool paintOnly, bool boundedLayout = false);
    ~PaintInvalidationScope();

    PaintInvalidationScope(const PaintInvalidationScope&) = delete;
    PaintInvalidationScope& operator=(const PaintInvalidationScope&) = delete;

private:
    bool previous_;
    bool previousBoundedLayout_;
};

} // namespace oneui::detail
