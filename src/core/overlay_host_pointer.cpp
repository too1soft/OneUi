#include "oneui/layout/overlay_host.h"

namespace oneui {
std::vector<OverlayEntry> OverlayHost::pointerOrder() const {
    std::vector<OverlayEntry> snapshot;
    snapshot.reserve(overlays_.size());
    for (const auto index : hitOrder())
        snapshot.push_back(overlays_[index]);
    return snapshot;
}

const std::wstring *OverlayHost::tooltipAt(Point point) const {
    if (!visible() || disabled())
        return nullptr;
    // Match paint/hit order. A covering overlay must not expose hints from
    // controls beneath it; decorative disabled overlays remain transparent.
    for (const auto index : hitOrder()) {
        const auto &entry = overlays_[index];
        if (!isInteractive(entry.child.get()))
            continue;
        if (entry.child->hitTest(point))
            return entry.child->tooltipAt(point);
        if (entry.blocksOutsidePointer)
            return nullptr;
    }
    if (content_ && content_->visible() && content_->hitTest(point)) {
        if (const auto *text = content_->tooltipAt(point))
            return text;
    }
    return View::tooltipAt(point);
}

bool OverlayHost::onMouseMove(const MouseEvent &event) {
    if (!interactive()) {
        clearInteractionState();
        return false;
    }

    // Content and overlays are owned outside View::children_. Remember that
    // they received pointer events so an ancestor leaving this host cannot
    // skip resetInteractionState() via View's empty-child fast path.
    pointerInteraction_ = true;

    // Preserve pointer capture across the overlay boundary.  Content controls
    // such as TerminalView and TextField use move events after mouse-down to
    // extend a selection.  Re-running overlay hit testing here would clear the
    // pressed content state and turn a drag into a one-cell click.
    if (pressedOverlay_ && isInteractive(pressedOverlay_)) {
        return pressedOverlay_->onMouseMove(event);
    }
    if (pressedContent_ && isInteractive(pressedContent_)) {
        return pressedContent_->onMouseMove(event);
    }

    layoutAnchoredOverlays();
    for (const auto &entry : pointerOrder()) {
        if (!containsOverlay(entry.child.get()))
            continue;
        // Event callbacks may remove/reorder overlays or destroy the host.
        const auto child = entry.child;
        if (!isInteractive(child.get())) {
            continue;
        }
        if (!child->hitTest(event.position)) {
            if (entry.blocksOutsidePointer) {
                bool changed = false;
                for (const auto &overlay : overlays_) {
                    if (overlay.child->visible() && !overlay.child->paintsAboveSiblings()) {
                        changed = overlay.child->clearInteractionState() || changed;
                    }
                }
                return changed;
            }
            continue;
        }

        // 命中的最上层可交互 overlay 独占指针：无论其内部状态是否变化，事件都
        // 不再下落。否则指针静置在 overlay 内时，第二次 move 会落到下面的
        // “清空全部 overlay 交互态”，把刚设置的 hover 清掉，造成 hover 逐帧闪烁。
        const auto life = lifetimeToken();
        bool changed = child->onMouseMove(event);
        if (life.expired())
            return changed;
        for (const auto &other : overlays_) {
            if (other.child.get() != child.get() && other.child->visible() &&
                !other.child->paintsAboveSiblings()) {
                changed = other.child->clearInteractionState() || changed;
            }
        }
        if (content_ && content_->visible() && !child->paintsAboveSiblings()) {
            changed = content_->clearInteractionState() || changed;
        }
        return changed;
    }

    bool changed = false;
    for (const auto &entry : overlays_) {
        if (entry.child->visible() && !entry.child->paintsAboveSiblings()) {
            changed = entry.child->clearInteractionState() || changed;
        }
    }
    if (content_ && content_->visible() && content_->hitTest(event.position)) {
        return content_->onMouseMove(event) || changed;
    }
    return View::onMouseMove(event) || changed;
}

bool OverlayHost::onMouseDown(const MouseEvent &event) {
    if (!interactive()) {
        clearInteractionState();
        focusOverlay(nullptr);
        return false;
    }

    pointerInteraction_ = true;

    layoutAnchoredOverlays();
    for (const auto &entry : pointerOrder()) {
        if (!containsOverlay(entry.child.get()))
            continue;
        // Event callbacks may remove/reorder overlays or destroy the host.
        const auto child = entry.child;
        if (!isInteractive(child.get())) {
            continue;
        }
        if (!child->hitTest(event.position)) {
            if (entry.blocksOutsidePointer) {
                pressedOverlay_ = nullptr;
                return true;
            }
            continue;
        }

        // 同 onMouseMove：命中 overlay 即消费按下事件，绝不下落到视觉上被
        // 遮住的 content（否则点击会穿透遮罩打到弹窗下面的控件）。
        pressedOverlay_ = child.get();
        const auto life = lifetimeToken();
        const bool handled = child->onMouseDown(event);
        if (life.expired())
            return true;
        if (containsOverlay(child.get()) && (handled || child->isFocusable())) {
            focusOverlay(child.get(), false);
        }
        return true;
    }

    pressedOverlay_ = nullptr;
    if (content_ && content_->visible() && !content_->disabled() &&
        content_->hitTest(event.position)) {
        pressedContent_ = content_.get();
        if (focusedOverlay_) {
            focusedOverlay_->onFocusChanged(false);
            focusedOverlay_ = nullptr;
            overlayFocusHistory_.clear();
            previousFocusedChild_ = nullptr;
            previousFocusVisible_ = false;
        }
        const auto content = content_;
        const auto life = lifetimeToken();
        const bool handled = content->onMouseDown(event);
        if (life.expired())
            return handled;
        if (content_ == content && (handled || content->isFocusable())) {
            content->onFocusChanged(true);
            content->setFocusVisible(false);
        }
        // Keyboard focusability is not pointer consumption. A decorative
        // overlay host inside a draggable header may contain buttons, while
        // blank header space still belongs to the parent drag surface.
        return handled;
    }
    pressedContent_ = nullptr;
    focusOverlay(nullptr);
    return View::onMouseDown(event);
}

bool OverlayHost::onMouseUp(const MouseEvent &event) {
    if (!interactive()) {
        pressedOverlay_ = nullptr;
        pressedContent_ = nullptr;
        return false;
    }

    if (pressedOverlay_) {
        std::shared_ptr<Widget> child;
        for (const auto &entry : overlays_)
            if (entry.child.get() == pressedOverlay_)
                child = entry.child;
        pressedOverlay_ = nullptr;
        return isInteractive(child.get()) ? child->onMouseUp(event) : false;
    }

    if (pressedContent_) {
        const auto child = content_;
        const bool retained = child.get() == pressedContent_;
        pressedContent_ = nullptr;
        return retained && isInteractive(child.get()) ? child->onMouseUp(event) : false;
    }

    layoutAnchoredOverlays();
    for (const auto &entry : pointerOrder()) {
        if (!containsOverlay(entry.child.get()))
            continue;
        // Event callbacks may remove/reorder overlays or destroy the host.
        const auto child = entry.child;
        if (!isInteractive(child.get())) {
            continue;
        }
        if (child->hitTest(event.position)) {
            return child->onMouseUp(event);
        }
        if (entry.blocksOutsidePointer) {
            return true;
        }
    }

    return View::onMouseUp(event);
}

bool OverlayHost::onMouseWheel(const MouseWheelEvent &event) {
    if (!interactive()) {
        return false;
    }

    layoutAnchoredOverlays();
    for (const auto &entry : pointerOrder()) {
        if (!containsOverlay(entry.child.get()))
            continue;
        // Event callbacks may remove/reorder overlays or destroy the host.
        const auto child = entry.child;
        if (!isInteractive(child.get())) {
            continue;
        }
        if (child->hitTest(event.position)) {
            const auto life = lifetimeToken();
            if (child->onMouseWheel(event))
                return true;
            if (life.expired())
                return true;
            continue;
        }
        if (entry.blocksOutsidePointer) {
            return true;
        }
    }
    if (content_ && content_->visible() && !content_->disabled() &&
        content_->hitTest(event.position)) {
        return content_->onMouseWheel(event);
    }
    return View::onMouseWheel(event);
}

CursorKind OverlayHost::cursor(Point point) const {
    if (!interactive() || !hitTest(point)) {
        return CursorKind::Default;
    }

    // OverlayHost owns a content layer plus optional floating overlays instead
    // of storing them as regular View children, so cursor hit testing follows
    // the same generic overlay-first order as pointer event dispatch.
    const_cast<OverlayHost *>(this)->layoutAnchoredOverlays();
    for (const auto &entry : pointerOrder()) {
        if (!containsOverlay(entry.child.get()))
            continue;
        const auto &child = entry.child;
        if (!isInteractive(child.get())) {
            continue;
        }
        if (child->hitTest(point)) {
            return child->cursor(point);
        }
        if (entry.blocksOutsidePointer) {
            return CursorKind::Default;
        }
    }

    if (content_ && content_->visible() && !content_->disabled()) {
        const_cast<Widget *>(content_.get())->setFrame(frame());
        if (content_->hitTest(point)) {
            return content_->cursor(point);
        }
    }
    return Widget::cursor(point);
}

} // namespace oneui
