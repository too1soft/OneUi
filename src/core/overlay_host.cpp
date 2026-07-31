#include "oneui/layout/overlay_host.h"

#include <algorithm>
#include <utility>

namespace oneui {

void OverlayHost::setContent(std::shared_ptr<Widget> child) {
    if (content_) {
        content_->onFocusChanged(false);
    }
    content_ = std::move(child);
    if (content_) {
        installOverlayHostCallbacks(*content_);
    }
    pressedContent_ = nullptr;
    invalidate();
}

void OverlayHost::addOverlay(std::shared_ptr<Widget> child, int layer) {
    addOverlay(std::move(child), OverlayOptions{layer, false, false});
}

void OverlayHost::addOverlay(std::shared_ptr<Widget> child, OverlayOptions options) {
    if (!child) {
        return;
    }
    installOverlayHostCallbacks(*child);
    overlays_.push_back(OverlayEntry{std::move(child), options.layer, options.trapsFocus, options.blocksOutsidePointer});
    invalidate();
}

void OverlayHost::addAnchoredOverlay(
    std::shared_ptr<Widget> child,
    OverlayOptions options,
    Size size,
    Insets margin,
    int horizontalAlignment,
    int verticalAlignment) {
    if (!child) {
        return;
    }
    installOverlayHostCallbacks(*child);
    OverlayEntry entry;
    entry.child = std::move(child);
    entry.layer = options.layer;
    entry.trapsFocus = options.trapsFocus;
    entry.blocksOutsidePointer = options.blocksOutsidePointer;
    entry.size = size;
    entry.margin = margin;
    entry.horizontalAlignment = std::clamp(horizontalAlignment, 0, 2);
    entry.verticalAlignment = std::clamp(verticalAlignment, 0, 2);
    entry.anchored = true;
    overlays_.push_back(std::move(entry));
    invalidate();
}

bool OverlayHost::updateAnchoredOverlay(
    const Widget* child,
    Size size,
    Insets margin,
    int horizontalAlignment,
    int verticalAlignment) {
    auto it = std::find_if(overlays_.begin(), overlays_.end(), [child](const OverlayEntry& entry) {
        return entry.child.get() == child;
    });
    if (it == overlays_.end()) {
        return false;
    }

    it->size = size;
    it->margin = margin;
    it->horizontalAlignment = std::clamp(horizontalAlignment, 0, 2);
    it->verticalAlignment = std::clamp(verticalAlignment, 0, 2);
    it->anchored = true;
    invalidate();
    return true;
}

bool OverlayHost::removeOverlay(const Widget* child) {
    auto it = std::find_if(overlays_.begin(), overlays_.end(), [child](const OverlayEntry& entry) {
        return entry.child.get() == child;
    });
    if (it == overlays_.end()) {
        return false;
    }

    const bool restoreFocus = focusedOverlay_ == it->child.get();
    const bool restoreFocusVisible = restoreFocus && it->child->focusVisible();
    clearOverlayReferences(it->child.get(), false);
    overlays_.erase(it);
    if (restoreFocus) {
        if (!restorePreviousOverlayFocus()) {
            focusNextOverlay(false, restoreFocusVisible);
        }
    }
    invalidate();
    return true;
}

void OverlayHost::clearOverlays() {
    focusOverlay(nullptr);
    overlays_.clear();
    overlayFocusHistory_.clear();
    pressedOverlay_ = nullptr;
    invalidate();
}

const std::vector<OverlayEntry>& OverlayHost::overlays() const {
    return overlays_;
}

void OverlayHost::setInvalidator(std::function<void()> invalidator) {
    View::setInvalidator(std::move(invalidator));
    if (content_) {
        installOverlayHostCallbacks(*content_);
    }
    for (const auto& entry : overlays_) {
        installOverlayHostCallbacks(*entry.child);
    }
}

void OverlayHost::setRectInvalidator(std::function<void(Rect)> invalidator) {
    View::setRectInvalidator(std::move(invalidator));
    if (content_) {
        installOverlayHostCallbacks(*content_);
    }
    for (const auto& entry : overlays_) {
        installOverlayHostCallbacks(*entry.child);
    }
}

void OverlayHost::setAnimationScheduler(std::function<void()> scheduler) {
    View::setAnimationScheduler(std::move(scheduler));
    if (content_) {
        installOverlayHostCallbacks(*content_);
    }
    for (const auto& entry : overlays_) {
        installOverlayHostCallbacks(*entry.child);
    }
}

void OverlayHost::paint(Canvas& canvas) {
    if (content_ && content_->visible()) {
        content_->setFrame(frame());
        content_->paint(canvas);
    }
    View::paint(canvas);
    layoutAnchoredOverlays();
    for (const std::size_t index : paintOrder()) {
        const auto& child = overlays_[index].child;
        if (child->visible()) {
            child->paint(canvas);
        }
    }
}

bool OverlayHost::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        clearInteractionState();
        return false;
    }

    layoutAnchoredOverlays();
    for (const std::size_t index : hitOrder()) {
        const auto& entry = overlays_[index];
        auto& child = overlays_[index].child;
        if (!isInteractive(child.get())) {
            continue;
        }
        if (!child->hitTest(event.position)) {
            if (entry.blocksOutsidePointer) {
                bool changed = false;
                for (const auto& overlay : overlays_) {
                    if (overlay.child->visible()) {
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
        bool changed = child->onMouseMove(event);
        for (const auto& other : overlays_) {
            if (other.child.get() != child.get() && other.child->visible()) {
                changed = other.child->clearInteractionState() || changed;
            }
        }
        if (content_ && content_->visible()) {
            changed = content_->clearInteractionState() || changed;
        }
        return changed;
    }

    bool changed = false;
    for (const auto& entry : overlays_) {
        if (entry.child->visible()) {
            changed = entry.child->clearInteractionState() || changed;
        }
    }
    if (content_ && content_->visible() && content_->hitTest(event.position)) {
        return content_->onMouseMove(event) || changed;
    }
    return View::onMouseMove(event) || changed;
}

bool OverlayHost::onMouseDown(const MouseEvent& event) {
    if (!interactive()) {
        clearInteractionState();
        focusOverlay(nullptr);
        return false;
    }

    layoutAnchoredOverlays();
    for (const std::size_t index : hitOrder()) {
        const auto& entry = overlays_[index];
        auto& child = overlays_[index].child;
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
        const bool handled = child->onMouseDown(event);
        if (handled || child->isFocusable()) {
            focusOverlay(child.get(), false);
        }
        return true;
    }

    pressedOverlay_ = nullptr;
    if (content_ && content_->visible() && !content_->disabled() && content_->hitTest(event.position)) {
        pressedContent_ = content_.get();
        if (focusedOverlay_) {
            focusedOverlay_->onFocusChanged(false);
            focusedOverlay_ = nullptr;
            overlayFocusHistory_.clear();
            previousFocusedChild_ = nullptr;
            previousFocusVisible_ = false;
        }
        const bool handled = content_->onMouseDown(event);
        if (handled || content_->isFocusable()) {
            content_->onFocusChanged(true);
            content_->setFocusVisible(false);
        }
        return handled || content_->isFocusable();
    }
    pressedContent_ = nullptr;
    focusOverlay(nullptr);
    return View::onMouseDown(event);
}

bool OverlayHost::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        pressedOverlay_ = nullptr;
        pressedContent_ = nullptr;
        return false;
    }

    if (pressedOverlay_) {
        Widget* child = pressedOverlay_;
        pressedOverlay_ = nullptr;
        return isInteractive(child) ? child->onMouseUp(event) : false;
    }

    if (pressedContent_) {
        Widget* child = pressedContent_;
        pressedContent_ = nullptr;
        return isInteractive(child) ? child->onMouseUp(event) : false;
    }

    layoutAnchoredOverlays();
    for (const std::size_t index : hitOrder()) {
        const auto& entry = overlays_[index];
        auto& child = overlays_[index].child;
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

bool OverlayHost::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive()) {
        return false;
    }

    layoutAnchoredOverlays();
    for (const std::size_t index : hitOrder()) {
        const auto& entry = overlays_[index];
        auto& child = overlays_[index].child;
        if (!isInteractive(child.get())) {
            continue;
        }
        if (child->hitTest(event.position)) {
            if (child->onMouseWheel(event)) {
                return true;
            }
            continue;
        }
        if (entry.blocksOutsidePointer) {
            return true;
        }
    }
    if (content_ && content_->visible() && !content_->disabled() && content_->hitTest(event.position)) {
        return content_->onMouseWheel(event);
    }
    return View::onMouseWheel(event);
}

bool OverlayHost::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (event.key == Key::Tab) {
        // 模态 overlay 在自身内部回绕；非模态 overlay 按层级参与 Tab 序。
        // 若没有可聚焦 overlay，则内容层内部回绕。窗口 chrome 以 tabStop=false
        // 排除出焦点序，故不会“跳到标题栏按钮”。
        if (focusedOverlay_ && isInteractive(focusedOverlay_) && hasActiveFocusTrap()) {
            if (focusedOverlay_->onKeyDown(event)) {
                return true;
            }
            if (event.shift) {
                focusedOverlay_->focusLastLeaf();
            } else {
                focusedOverlay_->focusFirstLeaf();
            }
            return true;
        }
        if (focusNextOverlay(event.shift, true)) {
            return true;
        }
        if (content_ && content_->visible() && !content_->disabled()) {
            if (content_->onKeyDown(event)) {
                return true;
            }
            if (event.shift) {
                content_->focusLastLeaf();
            } else {
                content_->focusFirstLeaf();
            }
            return true;
        }
        return false;
    }

    if (focusedOverlay_ && !isInteractive(focusedOverlay_)) {
        focusOverlay(nullptr);
    }
    if (focusedOverlay_ && focusedOverlay_->onKeyDown(event)) {
        return true;
    }
    if (content_ && content_->visible() && !content_->disabled() && content_->onKeyDown(event)) {
        return true;
    }
    return View::onKeyDown(event);
}

bool OverlayHost::focusFirstLeaf() {
    if (content_ && content_->visible() && !content_->disabled() && content_->focusFirstLeaf()) {
        focusOverlay(nullptr);
        return true;
    }
    const auto overlays = focusableOverlays();
    if (!overlays.empty()) {
        focusOverlay(overlays.front(), true);
        overlays.front()->focusFirstLeaf();
        return true;
    }
    return false;
}

bool OverlayHost::focusLastLeaf() {
    const auto overlays = focusableOverlays();
    if (!overlays.empty()) {
        focusOverlay(overlays.back(), true);
        overlays.back()->focusLastLeaf();
        return true;
    }
    if (content_ && content_->visible() && !content_->disabled() && content_->focusLastLeaf()) {
        focusOverlay(nullptr);
        return true;
    }
    return false;
}

bool OverlayHost::onKeyUp(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (focusedOverlay_ && !isInteractive(focusedOverlay_)) {
        focusOverlay(nullptr);
    }
    if (focusedOverlay_ && focusedOverlay_->onKeyUp(event)) {
        return true;
    }
    if (content_ && content_->visible() && !content_->disabled() && content_->onKeyUp(event)) {
        return true;
    }
    return View::onKeyUp(event);
}

bool OverlayHost::onTextInput(wchar_t character) {
    if (!interactive()) {
        return false;
    }

    if (focusedOverlay_ && !isInteractive(focusedOverlay_)) {
        focusOverlay(nullptr);
    }
    if (focusedOverlay_ && focusedOverlay_->onTextInput(character)) {
        return true;
    }
    if (content_ && content_->visible() && !content_->disabled() && content_->onTextInput(character)) {
        return true;
    }
    return View::onTextInput(character);
}

bool OverlayHost::onFocusChanged(bool focused) {
    if (!focused) {
        focusOverlay(nullptr);
        if (content_) {
            content_->onFocusChanged(false);
        }
        return View::onFocusChanged(false);
    }

    Widget::onFocusChanged(true);
    if (focusedOverlay_ && !isInteractive(focusedOverlay_)) {
        focusOverlay(nullptr);
    }
    if (focusedOverlay_) {
        return true;
    }

    if (!hasActiveFocusTrap() && content_ && content_->visible() && !content_->disabled() && content_->isFocusable()) {
        content_->onFocusChanged(true);
        content_->setFocusVisible(false);
        return true;
    }

    if (!focusedOverlay_ && focusNextOverlay(false, false)) {
        return true;
    }
    return View::onFocusChanged(true);
}

bool OverlayHost::isFocusable() const {
    if (content_ && content_->visible() && !content_->disabled() && content_->isFocusable()) {
        return true;
    }
    if (View::isFocusable()) {
        return true;
    }
    return std::any_of(overlays_.begin(), overlays_.end(), [](const OverlayEntry& entry) {
        return isInteractive(entry.child.get()) && entry.child->isFocusable();
    });
}

CursorKind OverlayHost::cursor(Point point) const {
    if (!interactive() || !hitTest(point)) {
        return CursorKind::Default;
    }

    // OverlayHost owns a content layer plus optional floating overlays instead
    // of storing them as regular View children, so cursor hit testing follows
    // the same generic overlay-first order as pointer event dispatch.
    const_cast<OverlayHost*>(this)->layoutAnchoredOverlays();
    for (const std::size_t index : hitOrder()) {
        const auto& entry = overlays_[index];
        const auto& child = entry.child;
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
        const_cast<Widget*>(content_.get())->setFrame(frame());
        if (content_->hitTest(point)) {
            return content_->cursor(point);
        }
    }
    return Widget::cursor(point);
}

void OverlayHost::setFocusVisible(bool visible) {
    View::setFocusVisible(visible);
    if (focusedOverlay_) {
        focusedOverlay_->setFocusVisible(visible);
    }
}

bool OverlayHost::tickAnimations(double nowMs) {
    bool running = View::tickAnimations(nowMs);
    if (content_ && content_->visible()) {
        running = content_->tickAnimations(nowMs) || running;
    }
    for (const auto& entry : overlays_) {
        if (entry.child && entry.child->visible()) {
            running = entry.child->tickAnimations(nowMs) || running;
        }
    }
    return running;
}

void OverlayHost::resetInteractionState() {
    View::resetInteractionState();
    pressedOverlay_ = nullptr;
    pressedContent_ = nullptr;
    if (content_) {
        content_->clearInteractionState();
    }
    for (const auto& entry : overlays_) {
        entry.child->clearInteractionState();
    }
}

std::vector<std::size_t> OverlayHost::paintOrder() const {
    std::vector<std::size_t> order(overlays_.size());
    for (std::size_t i = 0; i < overlays_.size(); ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [this](std::size_t a, std::size_t b) {
        return overlays_[a].layer < overlays_[b].layer;
    });
    return order;
}

std::vector<std::size_t> OverlayHost::hitOrder() const {
    std::vector<std::size_t> order = paintOrder();
    std::reverse(order.begin(), order.end());
    return order;
}

void OverlayHost::layoutAnchoredOverlays() {
    const Rect host = frame();
    for (auto& entry : overlays_) {
        if (!entry.child) {
            continue;
        }
        if (!entry.anchored) {
            continue;
        }
        const Size preferred = entry.child->preferredSize();
        const float left = host.x + entry.margin.left;
        const float right = host.x + std::max(0.0f, host.width - entry.margin.right);
        const float top = host.y + entry.margin.top;
        const float bottom = host.y + std::max(0.0f, host.height - entry.margin.bottom);
        const float availableWidth = std::max(0.0f, right - left);
        const float availableHeight = std::max(0.0f, bottom - top);
        const float width = entry.size.width < 0.0f ? availableWidth
                          : entry.size.width > 0.0f ? entry.size.width
                                                     : std::max(0.0f, preferred.width);
        const float height = entry.size.height < 0.0f ? availableHeight
                           : entry.size.height > 0.0f ? entry.size.height
                                                       : std::max(0.0f, preferred.height);
        float x = left;
        if (entry.horizontalAlignment == 1) {
            x = left + std::max(0.0f, right - left - width) / 2.0f;
        } else if (entry.horizontalAlignment == 2) {
            x = right - width;
        }
        float y = top;
        if (entry.verticalAlignment == 1) {
            y = top + std::max(0.0f, bottom - top - height) / 2.0f;
        } else if (entry.verticalAlignment == 2) {
            y = bottom - height;
        }
        entry.child->setFrame(Rect{x, y, width, height});
    }
}

Widget* OverlayHost::focusedOverlay() const {
    return focusedOverlay_;
}

void OverlayHost::focusOverlay(Widget* child, bool focusVisible) {
    if (focusedOverlay_ == child) {
        if (focusedOverlay_) {
            focusedOverlay_->setFocusVisible(focusVisible);
        } else if (previousFocusedChild_) {
            Widget* previous = previousFocusedChild_;
            const bool previousFocusVisible = previousFocusVisible_;
            previousFocusedChild_ = nullptr;
            previousFocusVisible_ = false;
            if (isInteractive(previous) && previous->isFocusable()) {
                focusChild(previous, previousFocusVisible);
            }
        }
        return;
    }

    if (child) {
        if (focusedOverlay_ && focusedOverlay_ != child) {
            OverlayFocusRecord record{focusedOverlay_, focusedOverlay_->focusVisible()};
            if (!overlayFocusHistory_.empty() && overlayFocusHistory_.back().child == focusedOverlay_) {
                overlayFocusHistory_.back() = record;
            } else {
                overlayFocusHistory_.push_back(record);
            }
        }
        if (!focusedOverlay_ && !previousFocusedChild_) {
            previousFocusedChild_ = focusedChild();
            if (!previousFocusedChild_) {
                for (const auto& viewChild : children()) {
                    if (viewChild && viewChild->focused()) {
                        previousFocusedChild_ = viewChild.get();
                        break;
                    }
                }
            }
            previousFocusVisible_ = previousFocusedChild_ ? previousFocusedChild_->focusVisible() : false;
        }
        focusChild(nullptr);
    }
    if (focusedOverlay_) {
        focusedOverlay_->onFocusChanged(false);
    }
    focusedOverlay_ = child;
    if (focusedOverlay_) {
        focusedOverlay_->onFocusChanged(true);
        focusedOverlay_->setFocusVisible(focusVisible);
    } else {
        Widget* previous = previousFocusedChild_;
        const bool focusVisible = previousFocusVisible_;
        previousFocusedChild_ = nullptr;
        previousFocusVisible_ = false;
        if (isInteractive(previous) && previous->isFocusable()) {
            focusChild(previous, focusVisible);
        }
    }
}

bool OverlayHost::focusNextOverlay(bool reverse, bool focusVisible) {
    const auto focusable = focusableOverlays();
    if (focusable.empty()) {
        focusOverlay(nullptr);
        return false;
    }

    auto it = std::find(focusable.begin(), focusable.end(), focusedOverlay_);
    int index = it == focusable.end() ? -1 : static_cast<int>(it - focusable.begin());
    index += reverse ? -1 : 1;

    if (index < 0) {
        index = static_cast<int>(focusable.size()) - 1;
    } else if (index >= static_cast<int>(focusable.size())) {
        index = 0;
    }

    focusOverlay(focusable[static_cast<std::size_t>(index)], focusVisible);
    return true;
}

std::vector<Widget*> OverlayHost::focusableOverlays() const {
    std::vector<Widget*> result;
    for (const std::size_t index : hitOrder()) {
        Widget* child = overlays_[index].child.get();
        if (isInteractive(child) && child->tabStop() && child->isFocusable() && isFocusAllowed(child)) {
            result.push_back(child);
        }
    }
    return result;
}

bool OverlayHost::restorePreviousOverlayFocus() {
    while (!overlayFocusHistory_.empty()) {
        OverlayFocusRecord record = overlayFocusHistory_.back();
        overlayFocusHistory_.pop_back();
        if (record.child && record.child != focusedOverlay_ && containsOverlay(record.child) && isInteractive(record.child) &&
            record.child->isFocusable() && isFocusAllowed(record.child)) {
            focusOverlay(record.child, record.focusVisible);
            return true;
        }
    }
    return false;
}

bool OverlayHost::containsOverlay(const Widget* child) const {
    return std::any_of(overlays_.begin(), overlays_.end(), [child](const OverlayEntry& entry) {
        return entry.child.get() == child;
    });
}

bool OverlayHost::hasActiveFocusTrap() const {
    return std::any_of(overlays_.begin(), overlays_.end(), [](const OverlayEntry& entry) {
        return entry.trapsFocus && isInteractive(entry.child.get());
    });
}

bool OverlayHost::isFocusAllowed(const Widget* child) const {
    const auto order = hitOrder();
    auto trapIt = std::find_if(order.begin(), order.end(), [this](std::size_t index) {
        const auto& entry = overlays_[index];
        return entry.trapsFocus && isInteractive(entry.child.get());
    });
    if (trapIt == order.end()) {
        return true;
    }

    return std::find_if(order.begin(), std::next(trapIt), [this, child](std::size_t index) {
        return overlays_[index].child.get() == child;
    }) != std::next(trapIt);
}

void OverlayHost::clearOverlayReferences(Widget* child, bool restorePreviousFocus) {
    if (focusedOverlay_ == child) {
        if (restorePreviousFocus) {
            focusOverlay(nullptr);
        } else {
            focusedOverlay_->onFocusChanged(false);
            focusedOverlay_ = nullptr;
        }
    }
    if (pressedOverlay_ == child) {
        pressedOverlay_ = nullptr;
    }
    overlayFocusHistory_.erase(std::remove_if(overlayFocusHistory_.begin(),
                                             overlayFocusHistory_.end(),
                                             [child](const OverlayFocusRecord& record) {
                                                 return record.child == child;
                                             }),
                               overlayFocusHistory_.end());
    if (previousFocusedChild_ == child) {
        previousFocusedChild_ = nullptr;
        previousFocusVisible_ = false;
    }
}

void OverlayHost::installOverlayHostCallbacks(Widget& child) {
    child.setInvalidator([this] {
        invalidate();
    });
    child.setRectInvalidator([this](Rect rect) {
        invalidateRect(rect);
    });
    child.setAnimationScheduler([this] {
        requestAnimationFrame();
    });
}

bool OverlayHost::isInteractive(const Widget* child) {
    return child && child->visible() && !child->disabled();
}

} // namespace oneui
