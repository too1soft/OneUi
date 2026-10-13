#include "oneui/layout/overlay_host.h"
#include "oneui/controls/popup.h"
#include "internal/key_dispatch_scope.h"

#include <algorithm>
#include <utility>

namespace oneui {

std::shared_ptr<Widget> OverlayHost::activeFocusChild() const {
    if (!interactive())
        return {};
    for (const auto &entry : overlays_)
        if (entry.child.get() == focusedOverlay_ && isInteractive(entry.child.get()) &&
            entry.child->focused() && isFocusAllowed(entry.child.get()))
            return entry.child;
    if (hasActiveFocusTrap())
        return {};
    if (isInteractive(content_.get()) && content_->focused())
        return content_;
    return View::activeFocusChild();
}

void OverlayHost::setTextEnvironment(std::wstring family, float scale) {
    View::setTextEnvironment(family, scale);
    family = textFontFamily();
    auto content = content_;
    const auto overlays = overlays_;
    if (content)
        content->setTextEnvironment(family, scale);
    for (const auto &entry : overlays)
        entry.child->setTextEnvironment(family, scale);
}

OverlayHost::~OverlayHost() {
    if (content_) {
        content_->detachFromOwner(this);
    }
    for (const auto &entry : overlays_) {
        entry.child->detachFromOwner(this);
    }
}

void OverlayHost::setContent(std::shared_ptr<Widget> child) {
    if (content_) {
        if (previousFocusedChild_ == content_.get()) {
            previousFocusedChild_ = nullptr;
            previousFocusVisible_ = false;
        }
        if (focusedChild() == content_.get()) focusChild(nullptr);
        else content_->onFocusChanged(false);
        content_->detachFromOwner(this);
    }
    content_ = std::move(child);
    if (content_) {
        installOverlayHostCallbacks(*content_);
    }
    pressedContent_ = nullptr;
    invalidate();
}

std::shared_ptr<Widget> OverlayHost::content() const { return content_; }

void OverlayHost::addOverlay(std::shared_ptr<Widget> child, int layer) {
    addOverlay(std::move(child), OverlayOptions::modeless(layer));
}

void OverlayHost::addOverlay(std::shared_ptr<Widget> child, OverlayOptions options) {
    if (!child) {
        return;
    }
    installOverlayHostCallbacks(*child);
    OverlayEntry entry;
    entry.child = std::move(child);
    entry.layer = options.layer;
    entry.trapsFocus = options.trapsFocus;
    entry.blocksOutsidePointer = options.blocksOutsidePointer;
    entry.backdrop = options.backdrop;
    overlays_.push_back(std::move(entry));
    invalidate();
}

void OverlayHost::addAnchoredOverlay(std::shared_ptr<Widget> child, OverlayOptions options,
                                     Size size, Insets margin, int horizontalAlignment,
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
    entry.backdrop = options.backdrop;
    entry.size = size;
    entry.margin = margin;
    entry.horizontalAlignment = std::clamp(horizontalAlignment, 0, 2);
    entry.verticalAlignment = std::clamp(verticalAlignment, 0, 2);
    entry.anchored = true;
    overlays_.push_back(std::move(entry));
    invalidate();
}

bool OverlayHost::updateAnchoredOverlay(const Widget *child, Size size, Insets margin,
                                        int horizontalAlignment, int verticalAlignment) {
    auto it = std::find_if(overlays_.begin(), overlays_.end(), [child](const OverlayEntry &entry) {
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

bool OverlayHost::removeOverlay(const Widget *child) {
    auto it = std::find_if(overlays_.begin(), overlays_.end(), [child](const OverlayEntry &entry) {
        return entry.child.get() == child;
    });
    if (it == overlays_.end()) {
        return false;
    }

    const bool restoreFocus = focusedOverlay_ == it->child.get();
    const bool restoreFocusVisible = restoreFocus && it->child->focusVisible();
    clearOverlayReferences(it->child.get(), false);
    it->child->detachFromOwner(this);
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
    for (const auto &entry : overlays_) {
        entry.child->detachFromOwner(this);
    }
    overlays_.clear();
    overlayFocusHistory_.clear();
    pressedOverlay_ = nullptr;
    invalidate();
}

const std::vector<OverlayEntry> &OverlayHost::overlays() const { return overlays_; }

void OverlayHost::setInvalidator(std::function<void()> invalidator) {
    View::setInvalidator(std::move(invalidator));
    if (content_) {
        content_->attachInvalidatorToOwner(this, [this] { invalidate(); });
    }
    for (const auto &entry : overlays_) {
        entry.child->attachInvalidatorToOwner(this, [this] { invalidate(); });
    }
}

void OverlayHost::setRectInvalidator(std::function<void(Rect)> invalidator) {
    View::setRectInvalidator(std::move(invalidator));
    if (content_) {
        content_->attachRectInvalidatorToOwner(this, [this](Rect rect) { invalidateRect(rect); });
    }
    for (const auto &entry : overlays_) {
        entry.child->attachRectInvalidatorToOwner(this,
                                                  [this](Rect rect) { invalidateRect(rect); });
    }
}

void OverlayHost::setAnimationScheduler(std::function<void()> scheduler) {
    View::setAnimationScheduler(std::move(scheduler));
    if (content_) {
        content_->attachAnimationSchedulerToOwner(this, [this] { requestAnimationFrame(); });
    }
    for (const auto &entry : overlays_) {
        entry.child->attachAnimationSchedulerToOwner(this, [this] { requestAnimationFrame(); });
    }
}

void OverlayHost::paint(Canvas &canvas) {
    if (content_ && content_->visible()) {
        content_->setFrame(frame());
        content_->paint(canvas);
    }
    View::paint(canvas);
    layoutAnchoredOverlays();
    for (const std::size_t index : paintOrder()) {
        const auto &entry = overlays_[index];
        const auto &child = entry.child;
        if (child->visible()) {
            if (entry.backdrop.a != 0) {
                canvas.fillRect(frame(), entry.backdrop);
            }
            child->paint(canvas);
        }
    }
}

bool OverlayHost::hitTest(Point point) const {
    if (View::hitTest(point)) {
        return true;
    }

    // OverlayHost owns content and overlay layers outside View::children_.
    // Include them in subtree hit testing so a popup extending beyond an
    // intermediate host remains reachable through any number of ancestors.
    auto *self = const_cast<OverlayHost *>(this);
    self->layoutAnchoredOverlays();
    for (const std::size_t index : hitOrder()) {
        const auto &child = overlays_[index].child;
        if (child && child->visible() && child->hitTest(point)) {
            return true;
        }
    }
    return content_ && content_->visible() && content_->hitTest(point);
}

bool OverlayHost::paintsAboveSiblings() const {
    if (View::paintsAboveSiblings()) {
        return true;
    }
    if (content_ && content_->visible() && content_->paintsAboveSiblings()) {
        return true;
    }
    return std::any_of(overlays_.begin(), overlays_.end(), [](const OverlayEntry &entry) {
        return entry.child && entry.child->visible() && entry.child->paintsAboveSiblings();
    });
}

bool OverlayHost::onKeyDown(const KeyEvent &event) {
    const internal::KeyDispatchScope dispatch(this);
    if (!interactive()) {
        return false;
    }

    // A modeless popup can remain open after programmatic focus returns to
    // the document (for example when its native window is reactivated).
    // Dismiss the frontmost eligible popup before forwarding Escape to that
    // document. Keep modal focus boundaries and the popup's close policy.
    if (event.key == Key::Escape) {
        for (const auto index : hitOrder()) {
            const auto child = overlays_[index].child;
            auto *popup = dynamic_cast<Popup *>(child.get());
            if (popup && popup->isOpen() && isInteractive(child.get()) &&
                isFocusAllowed(child.get())) {
                if (popup->onKeyDown(event))
                    return true;
                break;
            }
        }
    }

    // A modal may be added while a native window retains its previous focus.
    // Route the first key (especially Escape) to the modal without requiring a
    // mouse click first; lower overlays must not keep receiving input.
    if (hasActiveFocusTrap() &&
        (!focusedOverlay_ || !isInteractive(focusedOverlay_) || !isFocusAllowed(focusedOverlay_))) {
        for (const auto index : hitOrder()) {
            const auto &entry = overlays_[index];
            if (entry.trapsFocus && isInteractive(entry.child.get())) {
                focusOverlay(entry.child.get(), true);
                entry.child->focusFirstLeaf();
                break;
            }
        }
    }

    if (event.key == Key::Tab) {
        // 模态 overlay 在自身内部回绕；非模态 overlay 按层级参与 Tab 序。
        // Without focusable overlays, only the outer dispatch scope wraps;
        // nested content boundaries bubble to their parent's next sibling.
        if (focusedOverlay_ && isInteractive(focusedOverlay_)) {
            // Composite light-dismiss popups contain real form controls too.
            // Traverse that subtree before moving to another overlay.
            const auto life = lifetimeToken();
            if (focusedOverlay_->onKeyDown(event))
                return true;
            if (life.expired())
                return true;
            if (hasActiveFocusTrap() && focusedOverlay_) {
                if (event.shift)
                    focusedOverlay_->focusLastLeaf();
                else
                    focusedOverlay_->focusFirstLeaf();
                return true;
            }
        }
        if (focusNextOverlay(event.shift, true)) {
            if (focusedOverlay_) {
                if (event.shift)
                    focusedOverlay_->focusLastLeaf();
                else
                    focusedOverlay_->focusFirstLeaf();
            }
            return true;
        }
        if (content_ && content_->visible() && !content_->disabled()) {
            if (content_->onKeyDown(event)) {
                return true;
            }
            if (dispatch.hasParent() && !hasActiveFocusTrap()) return false;
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
    if (hasActiveFocusTrap())
        return true;
    if (content_ && content_->visible() && !content_->disabled() && content_->onKeyDown(event)) {
        return true;
    }
    return View::onKeyDown(event);
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
    for (const auto &entry : overlays_) {
        if (entry.child && entry.child->visible()) {
            running = entry.child->tickAnimations(nowMs) || running;
        }
    }
    return running;
}

bool OverlayHost::hasInteractionState() const {
    return pointerInteraction_ || pressedOverlay_ || pressedContent_ || focusedOverlay_ ||
           View::hasInteractionState();
}

void OverlayHost::resetInteractionState() {
    pointerInteraction_ = false;
    View::resetInteractionState();
    pressedOverlay_ = nullptr;
    pressedContent_ = nullptr;
    if (content_) {
        content_->clearInteractionState();
    }
    for (const auto &entry : overlays_) {
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
    for (auto &entry : overlays_) {
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
        float width = entry.size.width < 0.0f   ? availableWidth
                      : entry.size.width > 0.0f ? entry.size.width
                                                : std::max(0.0f, preferred.width);
        float height = entry.size.height < 0.0f   ? availableHeight
                       : entry.size.height > 0.0f ? entry.size.height
                                                  : std::max(0.0f, preferred.height);
        // Modal dialogs must keep their body and fixed action bar inside the viewport.
        // Modeless overlays retain their explicitly requested geometry.
        if (entry.trapsFocus && entry.blocksOutsidePointer) {
            width = std::min(width, std::max(0.0f, availableWidth - 32.0f));
            height = std::min(height, std::max(0.0f, availableHeight - 32.0f));
        }
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

void OverlayHost::styleScopeChanged() {
    View::styleScopeChanged();
    if (content_)
        content_->inheritStyleScope(styleScope(), this);
    const auto overlays = overlays_;
    for (const auto &entry : overlays)
        entry.child->inheritStyleScope(styleScope(), this);
}

void OverlayHost::installOverlayHostCallbacks(Widget &child) {
    child.inheritStyleScope(styleScope(), this);
    child.setTextEnvironment(textFontFamily(), textDpiScale());
    child.attachToOwner(
        this, [this] { invalidate(); }, [this](Rect rect) { invalidateRect(rect); },
        [this] { requestAnimationFrame(); });
}

bool OverlayHost::isInteractive(const Widget *child) {
    return child && child->visible() && !child->disabled();
}

} // namespace oneui
