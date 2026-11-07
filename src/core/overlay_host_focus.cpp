#include "oneui/controls/popup.h"
#include "oneui/layout/overlay_host.h"
#include <algorithm>

namespace oneui {
bool OverlayHost::navigateTab(const KeyEvent& event, bool hasParentScope) {
    const auto life = lifetimeToken();
    Widget* active = isInteractive(focusedOverlay_) ? focusedOverlay_ :
        (isInteractive(content_.get()) ? content_.get() : nullptr);
    if (active && active->onKeyDown(event)) return true;
    if (life.expired()) return true;

    if (hasActiveFocusTrap()) {
        if (!isInteractive(focusedOverlay_)) return false;
        if (event.shift) focusedOverlay_->focusLastLeaf();
        else focusedOverlay_->focusFirstLeaf();
        return true;
    }
    // An open popup owns its local keyboard cycle even when light-dismiss is
    // modeless. Ordinary overlay layout layers still bubble at their boundary.
    if (auto* popup = dynamic_cast<Popup*>(active); popup && popup->isOpen()) {
        if (event.shift) popup->focusLastLeaf();
        else popup->focusFirstLeaf();
        return true;
    }

    // Content comes before modeless overlays, in their existing hit order.
    // Let the active subtree finish before advancing to another focus domain.
    auto order = focusableOverlays();
    if (isInteractive(content_.get()) && content_->isFocusable()) {
        order.insert(order.begin(), content_.get());
    }
    if (order.empty()) return false;
    const auto current = std::find(order.begin(), order.end(), active);
    int index = current == order.end() ?
        (event.shift ? static_cast<int>(order.size()) : -1) :
        static_cast<int>(current - order.begin());
    index += event.shift ? -1 : 1;
    if (index < 0 || index >= static_cast<int>(order.size())) {
        // A nested modeless host must return control to its parent's sibling;
        // only the outer keyboard scope wraps around the entire route.
        if (hasParentScope) return false;
        index = event.shift ? static_cast<int>(order.size()) - 1 : 0;
    }
    Widget* target = order[static_cast<std::size_t>(index)];
    // Focus callbacks may remove a layer; keep its widget alive until this key
    // finishes, without extending the lifetime of the host itself.
    std::shared_ptr<Widget> targetOwner;
    if (target == content_.get()) {
        targetOwner = content_;
    } else {
        const auto entry = std::find_if(overlays_.begin(), overlays_.end(),
            [target](const OverlayEntry& entry) { return entry.child.get() == target; });
        if (entry == overlays_.end()) return false;
        targetOwner = entry->child;
    }
    if (target == content_.get()) {
        focusOverlay(nullptr);
        focusChild(target, true);
    } else {
        focusOverlay(target, true);
    }
    if (life.expired()) return true;
    if (event.shift) targetOwner->focusLastLeaf();
    else focusFirstKeyboardStop(targetOwner.get());
    return true;
}

bool OverlayHost::focusFirstLeaf() {
    if (!hasActiveFocusTrap() && content_ && content_->visible() && !content_->disabled() &&
        content_->isFocusable()) {
        focusOverlay(nullptr);
        focusChild(content_.get(), true);
        if (!content_->focused()) {
            content_->onFocusChanged(true);
            content_->setFocusVisible(true);
        }
        content_->focusFirstLeaf();
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
    if (!hasActiveFocusTrap() && content_ && content_->visible() && !content_->disabled() &&
        content_->isFocusable()) {
        focusOverlay(nullptr);
        focusChild(content_.get(), true);
        if (!content_->focused()) {
            content_->onFocusChanged(true);
            content_->setFocusVisible(true);
        }
        content_->focusLastLeaf();
        return true;
    }
    return false;
}

bool OverlayHost::requestFocus(Widget *descendant, bool focusVisible) {
    if (!descendant || !descendant->visible() || descendant->disabled()) {
        return false;
    }

    for (const auto &entry : overlays_) {
        Widget *overlay = entry.child.get();
        if (!isInteractive(overlay) || !isFocusAllowed(overlay)) {
            continue;
        }
        bool found = false;
        if (overlay == descendant) {
            found = descendant->isFocusable();
        } else if (auto *view = dynamic_cast<View *>(overlay)) {
            found = view->requestFocus(descendant, focusVisible);
        } else if (auto *popup = dynamic_cast<Popup *>(overlay)) {
            found = popup->requestFocus(descendant, focusVisible);
        }
        if (found) {
            focusOverlay(overlay, focusVisible);
            invalidate();
            return true;
        }
    }

    if (hasActiveFocusTrap() || !isInteractive(content_.get())) {
        return false;
    }
    bool found = false;
    if (content_.get() == descendant) {
        found = descendant->isFocusable();
    } else if (auto *view = dynamic_cast<View *>(content_.get())) {
        found = view->requestFocus(descendant, focusVisible);
    }
    if (!found) {
        return false;
    }

    focusOverlay(nullptr);
    content_->onFocusChanged(true);
    content_->setFocusVisible(focusVisible);
    invalidate();
    return true;
}

bool OverlayHost::onKeyUp(const KeyEvent &event) {
    auto child = activeFocusChild();
    return child && child->onKeyUp(event);
}

bool OverlayHost::onTextInput(wchar_t character) {
    auto child = activeFocusChild();
    return child && child->onTextInput(character);
}

bool OverlayHost::onTextInputText(const std::wstring &text) {
    auto child = activeFocusChild();
    return child && child->onTextInputText(text);
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

    if (!hasActiveFocusTrap() && content_ && content_->visible() && !content_->disabled() &&
        content_->isFocusable()) {
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
    return std::any_of(overlays_.begin(), overlays_.end(), [](const OverlayEntry &entry) {
        return isInteractive(entry.child.get()) && entry.child->isFocusable();
    });
}

Widget *OverlayHost::focusedOverlay() const { return focusedOverlay_; }

void OverlayHost::focusOverlay(Widget *child, bool focusVisible) {
    if (focusedOverlay_ == child) {
        if (focusedOverlay_) {
            focusedOverlay_->setFocusVisible(focusVisible);
        } else if (previousFocusedChild_) {
            Widget *previous = previousFocusedChild_;
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
            if (!overlayFocusHistory_.empty() &&
                overlayFocusHistory_.back().child == focusedOverlay_) {
                overlayFocusHistory_.back() = record;
            } else {
                overlayFocusHistory_.push_back(record);
            }
        }
        if (!focusedOverlay_ && !previousFocusedChild_) {
            previousFocusedChild_ = focusedChild();
            if (!previousFocusedChild_ && content_ && content_->focused())
                previousFocusedChild_ = content_.get();
            if (!previousFocusedChild_) {
                for (const auto &viewChild : children()) {
                    if (viewChild && viewChild->focused()) {
                        previousFocusedChild_ = viewChild.get();
                        break;
                    }
                }
            }
            previousFocusVisible_ =
                previousFocusedChild_ ? previousFocusedChild_->focusVisible() : false;
        }
        focusChild(nullptr);
        // Content is owned outside View::children_ and may have gained focus
        // through requestFocus/onFocusChanged without setting focusedChild_.
        if (content_ && content_->focused()) content_->onFocusChanged(false);
    }
    if (focusedOverlay_) {
        focusedOverlay_->onFocusChanged(false);
    }
    focusedOverlay_ = child;
    if (focusedOverlay_) {
        focusedOverlay_->onFocusChanged(true);
        focusedOverlay_->setFocusVisible(focusVisible);
    } else {
        Widget *previous = previousFocusedChild_;
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

std::vector<Widget *> OverlayHost::focusableOverlays() const {
    std::vector<Widget *> result;
    for (const std::size_t index : hitOrder()) {
        Widget *child = overlays_[index].child.get();
        if (isInteractive(child) && child->tabStop() && child->isFocusable() &&
            isFocusAllowed(child)) {
            result.push_back(child);
        }
    }
    return result;
}

bool OverlayHost::restorePreviousOverlayFocus() {
    while (!overlayFocusHistory_.empty()) {
        OverlayFocusRecord record = overlayFocusHistory_.back();
        overlayFocusHistory_.pop_back();
        if (record.child && record.child != focusedOverlay_ && containsOverlay(record.child) &&
            isInteractive(record.child) && record.child->isFocusable() &&
            isFocusAllowed(record.child)) {
            focusOverlay(record.child, record.focusVisible);
            return true;
        }
    }
    return false;
}

bool OverlayHost::containsOverlay(const Widget *child) const {
    return std::any_of(overlays_.begin(), overlays_.end(),
                       [child](const OverlayEntry &entry) { return entry.child.get() == child; });
}

bool OverlayHost::hasActiveFocusTrap() const {
    return std::any_of(overlays_.begin(), overlays_.end(), [](const OverlayEntry &entry) {
        return entry.trapsFocus && isInteractive(entry.child.get());
    });
}

bool OverlayHost::isFocusAllowed(const Widget *child) const {
    const auto order = hitOrder();
    auto trapIt = std::find_if(order.begin(), order.end(), [this](std::size_t index) {
        const auto &entry = overlays_[index];
        return entry.trapsFocus && isInteractive(entry.child.get());
    });
    if (trapIt == order.end()) {
        return true;
    }

    return std::find_if(order.begin(), std::next(trapIt), [this, child](std::size_t index) {
               return overlays_[index].child.get() == child;
           }) != std::next(trapIt);
}

void OverlayHost::clearOverlayReferences(Widget *child, bool restorePreviousFocus) {
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
    overlayFocusHistory_.erase(
        std::remove_if(overlayFocusHistory_.begin(), overlayFocusHistory_.end(),
                       [child](const OverlayFocusRecord &record) { return record.child == child; }),
        overlayFocusHistory_.end());
    if (previousFocusedChild_ == child) {
        previousFocusedChild_ = nullptr;
        previousFocusVisible_ = false;
    }
}

} // namespace oneui
