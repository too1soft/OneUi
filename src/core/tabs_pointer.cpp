#include "oneui/controls/tabs.h"

#include "oneui/icon.h"
#include "oneui/style.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace oneui {
bool Tabs::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (editingIndex_ >= 0) { return editor_->onMouseMove(event); }
    if (dragOutIndex_ >= 0) {
        if (auto callback = onTabDragOut_) callback(dragOutIndex_, 1, event.position);
        return true;
    }
    const int next = hitIndex(event.position);
    const int nextClose = hitCloseIndex(event.position);
    bool interactionChanged = next != hoveredIndex_ || nextClose != hoveredCloseIndex_;
    if (reorderEnabled_ && pressedIndex_ >= 0 && pressedCloseIndex_ < 0) {
        const float deltaX = event.position.x - dragStart_.x;
        const float deltaY = event.position.y - dragStart_.y;
        if (!dragging_ && std::hypot(deltaX, deltaY) >= 5.0f) {
            dragging_ = true;
            dragSourceIndex_ = pressedIndex_;
            interactionChanged = true;
        }
        if (dragging_ && next >= 0 && dragTargetIndex_ != next) {
            dragTargetIndex_ = next;
            interactionChanged = true;
        }
        if (dragging_ && onTabDragOut_ && !frame().contains(event.position)) {
            dragOutIndex_ = dragSourceIndex_;
            dragging_ = false;
            dragSourceIndex_ = -1;
            dragTargetIndex_ = -1;
            hoveredIndex_ = -1;
            invalidate();
            auto callback = onTabDragOut_;
            const int index = dragOutIndex_;
            callback(index, 0, event.position);
            if (dragOutIndex_ == index) callback(index, 1, event.position);
            return true;
        }
        if (dragging_ && onDetachRequested_ && !frame().contains(event.position)) {
            auto callback = onDetachRequested_;
            const auto index = dragSourceIndex_;
            clearInteractionState();
            callback(index, event.position);
            return true;
        }
    }
    if (!interactionChanged) {
        return false;
    }
    hoveredIndex_ = next;
    hoveredCloseIndex_ = nextClose;
    invalidate();
    return true;
}

void Tabs::setOnDetachRequested(std::function<void(int, Point)> callback) {
    onDetachRequested_ = std::move(callback);
}

void Tabs::setOnTabDragOut(std::function<void(int, int, Point)> callback) {
    onTabDragOut_ = std::move(callback);
}

bool Tabs::onMouseDown(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (editingIndex_ >= 0) {
        updateEditorFrame();
        if (editor_->frame().contains(event.position)) { return editor_->onMouseDown(event); }
        finishEdit(TabEditReason::FocusLost);
    }
    if (event.button == MouseButton::Middle) {
        pressedCloseIndex_ = hitIndex(event.position);
        invalidate();
        return pressedCloseIndex_ >= 0;
    }
    if (event.button == MouseButton::Right) {
        contextPressedIndex_ = hitIndex(event.position);
        return contextPressedIndex_ >= 0;
    }
    if (event.button != MouseButton::Left) {
        return false;
    }
    const int closeIndex = hitCloseIndex(event.position);
    if (closeIndex >= 0) {
        pressedCloseIndex_ = closeIndex;
        invalidate();
        return true;
    }
    const int index = hitIndex(event.position);
    if (index < 0) {
        return false;
    }
    pressedIndex_ = index;
    dragStart_ = event.position;
    dragSourceIndex_ = -1;
    dragTargetIndex_ = -1;
    dragging_ = false;
    invalidate();
    return true;
}

bool Tabs::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        return false;
    }
    if (editingIndex_ >= 0) { return editor_->onMouseUp(event); }
    if (dragOutIndex_ >= 0 && event.button == MouseButton::Left) {
        const int index = dragOutIndex_;
        dragOutIndex_ = -1;
        pressedIndex_ = -1;
        invalidate();
        if (auto callback = onTabDragOut_) callback(index, 2, event.position);
        return true;
    }
    const int wasClosePressed = pressedCloseIndex_;
    pressedCloseIndex_ = -1;
    const int closeIndex = event.button == MouseButton::Middle
        ? hitIndex(event.position)
        : hitCloseIndex(event.position);
    if (wasClosePressed >= 0 && wasClosePressed == closeIndex) {
        if (onCloseRequested_) {
            onCloseRequested_(closeIndex);
        }
        invalidate();
        return true;
    }
    if (event.button == MouseButton::Right) {
        const int wasContextPressed = contextPressedIndex_;
        contextPressedIndex_ = -1;
        const int contextIndex = hitIndex(event.position);
        if (wasContextPressed >= 0 && wasContextPressed == contextIndex) {
            if (onContextMenuRequested_) {
                onContextMenuRequested_(contextIndex, event.position);
            }
            invalidate();
            return true;
        }
        invalidate();
        return wasContextPressed >= 0;
    }
    if (event.button != MouseButton::Left) {
        invalidate();
        return false;
    }
    const int wasPressed = pressedIndex_;
    pressedIndex_ = -1;
    const bool wasDragging = dragging_;
    const int dragSource = dragSourceIndex_;
    const int dragTarget = dragTargetIndex_;
    dragging_ = false;
    dragSourceIndex_ = -1;
    dragTargetIndex_ = -1;
    if (wasDragging) {
        if (dragSource >= 0 && dragTarget >= 0 && dragSource != dragTarget
            && onReorderRequested_) {
            onReorderRequested_(dragSource, dragTarget);
        }
        invalidate();
        return true;
    }
    const int index = hitIndex(event.position);
    if (wasPressed >= 0 && wasPressed == index) {
        assignSelectedIndex(index);
    }
    invalidate();
    return wasPressed >= 0;
}

bool Tabs::onMouseWheel(const MouseWheelEvent& event) {
    if (editingIndex_ >= 0) { return true; }
    if (!interactive() || sizingMode_ != TabsSizingMode::Compact
        || !frame().contains(event.position) || maximumScrollOffset() <= 0.0f
        || std::abs(event.deltaY) < 0.001f) {
        return false;
    }
    const float next = std::clamp(
        scrollOffset_ - event.deltaY * 48.0f,
        0.0f,
        maximumScrollOffset());
    if (next == scrollOffset_) {
        return false;
    }
    scrollOffset_ = next;
    invalidate();
    return true;
}

bool Tabs::hasInteractionState() const {
    return editingIndex_ >= 0 || hoveredIndex_ >= 0 || pressedIndex_ >= 0 || hoveredCloseIndex_ >= 0
        || pressedCloseIndex_ >= 0 || contextPressedIndex_ >= 0 || dragging_ || dragOutIndex_ >= 0;
}

void Tabs::resetInteractionState() {
    // Views also reset pointer state when the mouse leaves a child. That is
    // not keyboard focus loss and must not discard an in-progress label.
    if (!interactive()) { cancelEdit(); }
    if (editingIndex_ >= 0) { editor_->clearInteractionState(); }
    hoveredIndex_ = -1;
    pressedIndex_ = -1;
    hoveredCloseIndex_ = -1;
    pressedCloseIndex_ = -1;
    contextPressedIndex_ = -1;
    dragSourceIndex_ = -1;
    dragTargetIndex_ = -1;
    dragging_ = false;
    if (dragOutIndex_ >= 0) {
        const int index = dragOutIndex_;
        dragOutIndex_ = -1;
        if (auto callback = onTabDragOut_) callback(index, 3, Point{});
    }
}

} // namespace oneui
