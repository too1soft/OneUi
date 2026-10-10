#include "terminal_view_helpers.h"

namespace oneui {
using namespace terminal_detail;

TerminalView::GridMetrics TerminalView::gridMetrics(const Canvas& canvas) const {
    const float measured = canvas.measureTextCellsWidth(
        L"M", fontSize_, fontFamily_, TextFontFamily::Monospace, 400);
    return GridMetrics{
        std::max(1.0f, measured + letterSpacing_),
        std::max(fontSize_ * lineHeight_, fontSize_ + 2.0f),
    };
}

float TerminalView::lineNumberGutterWidth(GridMetrics metrics) const {
    if (!lineNumbersVisible_ || firstVisibleLineNumber_ == 0) {
        return 0.0f;
    }
    const std::uint64_t last = firstVisibleLineNumber_ + (rows_ == 0 ? 0 : rows_ - 1);
    const float digits = static_cast<float>(std::to_wstring(last).size());
    return std::ceil((digits + 1.65f) * std::max(1.0f, metrics.cellWidth));
}

Rect TerminalView::contentBounds(Rect bounds, GridMetrics metrics) const {
    const float gutter = std::min(bounds.width, lineNumberGutterWidth(metrics));
    return Rect{bounds.x + gutter, bounds.y, std::max(0.0f, bounds.width - gutter), bounds.height};
}

void TerminalView::reportViewport(Rect bounds, GridMetrics metrics) {
    if (viewportHeld_) return;
    bounds = contentBounds(bounds, metrics);
    const float cellWidth = std::max(1.0f, metrics.cellWidth);
    const float cellHeight = std::max(1.0f, metrics.cellHeight);

    // Layout can temporarily collapse a terminal while its parent replaces or
    // remeasures content. Publishing that transient size as a 1x1 viewport
    // resizes the backing PTY and can make interactive shells continuously
    // repaint. Keep the last stable viewport until at least one complete cell
    // fits in both dimensions.
    if (bounds.width < cellWidth || bounds.height < cellHeight) {
        return;
    }
    const auto columns = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor(bounds.width / cellWidth)),
        1,
        static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
    const auto rows = static_cast<std::uint16_t>(std::clamp(
        static_cast<int>(std::floor(bounds.height / cellHeight)),
        1,
        static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
    const TerminalViewport viewport{rows, columns};
    const bool changed = viewport.rows != viewport_.rows || viewport.columns != viewport_.columns;
    if (!changed && !workspaceViewportPending_) return;
    viewport_ = viewport;
    workspaceViewportPending_ = false;
    auto observer = workspaceViewportObserver_;
    auto legacy = onViewportChanged_;
    // Workspace observation only queues an event. Business VM invocation runs
    // on the next UI update, outside this paint stack.
    if (observer) observer({rows, columns});
    if (changed && legacy) legacy(viewport);
}

void TerminalView::holdViewportNotifications(bool hold) {
    if (viewportHeld_ == hold) return;
    viewportHeld_ = hold;
    if (!hold) invalidate();
}

void TerminalView::setWorkspaceViewportObserver(std::function<void(ContentViewport)> callback) {
    workspaceViewportObserver_ = std::move(callback);
    workspaceViewportPending_ = bool(workspaceViewportObserver_);
    if (workspaceViewportPending_) invalidate();
}

} // namespace oneui
