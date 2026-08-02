#include "oneui/controls/popup.h"

#include <algorithm>
#include <utility>

namespace oneui {
namespace {

PopupPreferredPlacement opposite(PopupPreferredPlacement placement) {
    switch (placement) {
    case PopupPreferredPlacement::BottomStart:
        return PopupPreferredPlacement::TopStart;
    case PopupPreferredPlacement::BottomEnd:
        return PopupPreferredPlacement::TopEnd;
    case PopupPreferredPlacement::TopStart:
        return PopupPreferredPlacement::BottomStart;
    case PopupPreferredPlacement::TopEnd:
        return PopupPreferredPlacement::BottomEnd;
    case PopupPreferredPlacement::LeftStart:
        return PopupPreferredPlacement::RightStart;
    case PopupPreferredPlacement::RightStart:
        return PopupPreferredPlacement::LeftStart;
    }
    return PopupPreferredPlacement::BottomStart;
}

bool isTop(PopupPreferredPlacement placement) {
    return placement == PopupPreferredPlacement::TopStart || placement == PopupPreferredPlacement::TopEnd;
}

bool isLeft(PopupPreferredPlacement placement) {
    return placement == PopupPreferredPlacement::LeftStart;
}

bool isRight(PopupPreferredPlacement placement) {
    return placement == PopupPreferredPlacement::RightStart;
}

bool isSide(PopupPreferredPlacement placement) {
    return isLeft(placement) || isRight(placement);
}

bool isEnd(PopupPreferredPlacement placement) {
    return placement == PopupPreferredPlacement::BottomEnd || placement == PopupPreferredPlacement::TopEnd;
}

Rect candidateRect(Rect anchor, Size preferred, PopupPreferredPlacement placement, float offset) {
    if (isLeft(placement)) {
        return Rect{anchor.x - preferred.width - offset, anchor.y, preferred.width, preferred.height};
    }
    if (isRight(placement)) {
        return Rect{anchor.x + anchor.width + offset, anchor.y, preferred.width, preferred.height};
    }

    const float x = isEnd(placement) ? anchor.x + anchor.width - preferred.width : anchor.x;
    const float y = isTop(placement) ? anchor.y - preferred.height - offset : anchor.y + anchor.height + offset;
    return Rect{x, y, preferred.width, preferred.height};
}

bool fitsVertically(Rect rect, Rect viewport) {
    return rect.y >= viewport.y && rect.y + rect.height <= viewport.y + viewport.height;
}

bool fitsHorizontally(Rect rect, Rect viewport) {
    return rect.x >= viewport.x && rect.x + rect.width <= viewport.x + viewport.width;
}

float primaryOverflow(Rect rect, Rect viewport, PopupPreferredPlacement placement) {
    if (isSide(placement)) {
        const float viewportRight = viewport.x + viewport.width;
        return std::max(0.0f, viewport.x - rect.x) + std::max(0.0f, rect.x + rect.width - viewportRight);
    }

    const float viewportBottom = viewport.y + viewport.height;
    return std::max(0.0f, viewport.y - rect.y) + std::max(0.0f, rect.y + rect.height - viewportBottom);
}

float clamp(float value, float minimum, float maximum) {
    if (maximum < minimum) {
        return minimum;
    }
    return std::clamp(value, minimum, maximum);
}

void applyPopupStyleOverride(PopupStyle& style, const PopupStyleOverride& override) {
    if (override.background) {
        style.background = *override.background;
    }
    if (override.foreground) {
        style.foreground = *override.foreground;
    }
    if (override.border) {
        style.border = *override.border;
    }
    if (override.borderWidth) {
        style.borderWidth = *override.borderWidth;
    }
    if (override.radius) {
        style.radius = *override.radius;
    }
    if (override.padding) {
        style.padding = *override.padding;
    }
    if (override.offset) {
        style.offset = *override.offset;
    }
    if (override.elevation) {
        style.elevation = *override.elevation;
    }
    if (override.layer) {
        style.layer = *override.layer;
    }
}

} // namespace

PopupPlacementResult PopupPlacement::resolve(const PopupPlacementRequest& request) {
    const PopupPreferredPlacement fallback = opposite(request.preferredPlacement);
    PopupPreferredPlacement placement = request.preferredPlacement;
    Rect rect = candidateRect(request.anchor, request.preferredSize, placement, request.offset);
    bool flipped = false;

    const bool fitsPrimaryAxis = isSide(placement) ? fitsHorizontally(rect, request.viewport) : fitsVertically(rect, request.viewport);
    if (!fitsPrimaryAxis) {
        const Rect flippedRect = candidateRect(request.anchor, request.preferredSize, fallback, request.offset);
        const float currentOverflow = primaryOverflow(rect, request.viewport, placement);
        const float flippedOverflow = primaryOverflow(flippedRect, request.viewport, fallback);
        if (flippedOverflow <= currentOverflow) {
            placement = fallback;
            rect = flippedRect;
            flipped = true;
        }
    }

    rect.width = std::min(std::max(0.0f, rect.width), std::max(0.0f, request.viewport.width));
    rect.height = std::min(std::max(0.0f, rect.height), std::max(0.0f, request.viewport.height));
    rect.x = clamp(rect.x, request.viewport.x, request.viewport.x + request.viewport.width - rect.width);
    rect.y = clamp(rect.y, request.viewport.y, request.viewport.y + request.viewport.height - rect.height);
    return PopupPlacementResult{rect, placement, flipped};
}

Popup::Popup() {
    setPreferredSize(Size{0.0f, 0.0f});
}

void Popup::setAnchor(std::shared_ptr<Widget> anchor) {
    anchor_ = std::move(anchor);
    if (anchor_) {
        anchor_->setInvalidator([this] {
            invalidate();
        });
        setPreferredSize(anchor_->preferredSize());
    }
    invalidate();
}

std::shared_ptr<Widget> Popup::anchor() const {
    return anchor_;
}

void Popup::setContent(std::shared_ptr<Widget> content) {
    content_ = std::move(content);
    if (content_) {
        content_->setInvalidator([this] {
            invalidate();
        });
    }
    invalidate();
}

std::shared_ptr<Widget> Popup::content() const {
    return content_;
}

void Popup::setOpen(bool open) {
    if (isOpen() == open) {
        return;
    }
    openBinding_.set(open, open_);
    if (!open) {
        pressedChild_ = nullptr;
        if (focusedChild_ == content_.get()) {
            focusChild(nullptr);
        }
    }
    invalidate();
}

bool Popup::isOpen() const {
    return openBinding_.get(open_);
}

void Popup::bindOpen(State<bool>& state) {
    openBinding_ = Binding<bool>(state, [this] {
        if (!isOpen()) {
            pressedChild_ = nullptr;
            if (focusedChild_ == content_.get()) {
                focusChild(nullptr);
            }
        }
        invalidate();
    });
    invalidate();
}

void Popup::setPreferredPlacement(PopupPreferredPlacement placement) {
    preferredPlacement_ = placement;
    invalidate();
}

PopupPreferredPlacement Popup::preferredPlacement() const {
    return preferredPlacement_;
}

void Popup::setViewport(std::optional<Rect> viewport) {
    viewport_ = viewport;
    invalidate();
}

void Popup::clearViewport() {
    setViewport(std::nullopt);
}

void Popup::setAnchorRect(std::optional<Rect> rect) {
    anchorRect_ = rect;
    invalidate();
}

void Popup::clearAnchorRect() {
    setAnchorRect(std::nullopt);
}

void Popup::setCloseOnOutsideClick(bool close) {
    setOutsidePointerPolicy(close ? PopupOutsidePointerPolicy::Close : PopupOutsidePointerPolicy::PassThrough);
    interactionMode_ = close ? PopupInteractionMode::LightDismiss : PopupInteractionMode::Modeless;
}

void Popup::setInteractionMode(PopupInteractionMode mode) {
    interactionMode_ = mode;
    switch (interactionMode_) {
    case PopupInteractionMode::Modeless:
        outsidePointerPolicy_ = PopupOutsidePointerPolicy::PassThrough;
        break;
    case PopupInteractionMode::LightDismiss:
        outsidePointerPolicy_ = PopupOutsidePointerPolicy::Close;
        break;
    case PopupInteractionMode::Modal:
        outsidePointerPolicy_ = PopupOutsidePointerPolicy::Block;
        break;
    }
    invalidate();
}

PopupInteractionMode Popup::interactionMode() const {
    return interactionMode_;
}

OverlayOptions Popup::overlayOptions(int layer) const {
    if (interactionMode_ == PopupInteractionMode::Modal) {
        return OverlayOptions::modal(layer);
    }
    return OverlayOptions::modeless(layer);
}

void Popup::setOutsidePointerPolicy(PopupOutsidePointerPolicy policy) {
    outsidePointerPolicy_ = policy;
    invalidate();
}

PopupOutsidePointerPolicy Popup::outsidePointerPolicy() const {
    return outsidePointerPolicy_;
}

void Popup::setCloseOnEscape(bool close) {
    closeOnEscape_ = close;
}

void Popup::setStyleOverride(PopupStyleOverride style) {
    styleOverride_ = std::move(style);
    invalidate();
}

void Popup::clearStyleOverride() {
    styleOverride_.reset();
    invalidate();
}

PopupStyle Popup::resolvedStyle() const {
    PopupStyle style = theme().popup;
    if (styleOverride_) {
        applyPopupStyleOverride(style, *styleOverride_);
    }
    return style;
}

Rect Popup::resolvedContentRect() const {
    const PopupStyle style = resolvedStyle();
    const Size preferred = content_ ? content_->preferredSize() : Size{};
    return PopupPlacement::resolve(PopupPlacementRequest{resolvedAnchorRect(), preferred, resolvedViewport(), preferredPlacement_, style.offset}).rect;
}

void Popup::setInvalidator(std::function<void()> invalidator) {
    Widget::setInvalidator(std::move(invalidator));
    if (anchor_) {
        anchor_->setInvalidator([this] {
            invalidate();
        });
    }
    if (content_) {
        content_->setInvalidator([this] {
            invalidate();
        });
    }
}

void Popup::paint(Canvas& canvas) {
    layoutAnchor();
    if (anchor_ && anchor_->visible()) {
        anchor_->paint(canvas);
    }

    if (!isOpen() || !content_ || !content_->visible()) {
        return;
    }

    layoutContent();
    const Rect popup = resolvedContentRect();
    const PopupStyle style = resolvedStyle();
    if (style.elevation > 0.0f) {
        const float shadowOffset = std::max(1.0f, style.elevation * 2.0f);
        canvas.drawBoxShadow(popup, BoxShadow{theme().shadowColor, Point{0.0f, shadowOffset}, style.elevation * 10.0f, 0.0f}, style.radius);
    }
    canvas.fillRect(popup, style.background, style.radius);
    if (style.borderWidth > 0.0f) {
        canvas.strokeRect(popup, style.border, style.radius, style.borderWidth);
    }
    content_->paint(canvas);
}

bool Popup::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        clearInteractionState();
        return false;
    }

    layoutAnchor();
    layoutContent();
    bool changed = false;
    if (isOpen() && isInteractive(content_.get()) && content_->hitTest(event.position)) {
        changed = content_->onMouseMove(event);
        if (anchor_) {
            changed = anchor_->clearInteractionState() || changed;
        }
        return changed;
    }
    if (isInteractive(anchor_.get()) && anchor_->hitTest(event.position)) {
        changed = anchor_->onMouseMove(event);
    }
    if (content_ && !content_->hitTest(event.position)) {
        changed = content_->clearInteractionState() || changed;
    }
    if (isOpen() && outsidePointerPolicy_ != PopupOutsidePointerPolicy::PassThrough) {
        return true;
    }
    return changed;
}

bool Popup::onMouseDown(const MouseEvent& event) {
    if (!interactive()) {
        clearInteractionState();
        focusChild(nullptr);
        return false;
    }

    layoutAnchor();
    layoutContent();
    if (isOpen() && isInteractive(content_.get()) && content_->hitTest(event.position)) {
        pressedChild_ = content_.get();
        const bool handled = content_->onMouseDown(event);
        if (handled || content_->isFocusable()) {
            focusChild(content_.get(), false);
        }
        return handled || content_->isFocusable();
    }

    if (isInteractive(anchor_.get()) && anchor_->hitTest(event.position)) {
        pressedChild_ = anchor_.get();
        const bool handled = anchor_->onMouseDown(event);
        if (handled || anchor_->isFocusable()) {
            focusChild(anchor_.get(), false);
        }
        return handled || anchor_->isFocusable();
    }

    pressedChild_ = nullptr;
    if (isOpen()) {
        if (outsidePointerPolicy_ == PopupOutsidePointerPolicy::Close) {
            setOpen(false);
            return true;
        }
        if (outsidePointerPolicy_ == PopupOutsidePointerPolicy::Block) {
            return true;
        }
    }
    return false;
}

bool Popup::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        pressedChild_ = nullptr;
        return false;
    }

    Widget* child = pressedChild_;
    pressedChild_ = nullptr;
    return isInteractive(child) ? child->onMouseUp(event) : false;
}

bool Popup::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive()) {
        return false;
    }

    layoutContent();
    if (isOpen() && isInteractive(content_.get()) && content_->hitTest(event.position)) {
        return content_->onMouseWheel(event);
    }
    if (isInteractive(anchor_.get()) && anchor_->hitTest(event.position)) {
        return anchor_->onMouseWheel(event);
    }
    return isOpen() && outsidePointerPolicy_ != PopupOutsidePointerPolicy::PassThrough;
}

bool Popup::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (isOpen() && closeOnEscape_ && event.key == Key::Escape) {
        setOpen(false);
        return true;
    }
    if (focusedChild_ && !isInteractive(focusedChild_)) {
        focusChild(nullptr);
    }
    return focusedChild_ ? focusedChild_->onKeyDown(event) : false;
}

bool Popup::onTextInput(wchar_t character) {
    if (!interactive()) {
        return false;
    }
    if (focusedChild_ && !isInteractive(focusedChild_)) {
        focusChild(nullptr);
    }
    return focusedChild_ ? focusedChild_->onTextInput(character) : false;
}

bool Popup::onTextInputText(const std::wstring& text) {
    if (!interactive() || text.empty()) {
        return false;
    }
    if (focusedChild_ && !isInteractive(focusedChild_)) {
        focusChild(nullptr);
    }
    return focusedChild_ ? focusedChild_->onTextInputText(text) : false;
}

bool Popup::onFocusChanged(bool focused) {
    Widget::onFocusChanged(focused);
    if (!focused && focusedChild_) {
        focusedChild_->onFocusChanged(false);
    } else if (focused && focusedChild_) {
        focusedChild_->onFocusChanged(true);
    }
    return true;
}

bool Popup::isFocusable() const {
    return interactive()
        && ((anchor_ && anchor_->isFocusable())
            || (isOpen() && content_ && content_->isFocusable()));
}

bool Popup::hitTest(Point point) const {
    if (!interactive()) {
        return false;
    }
    const bool inAnchor = anchor_ ? resolvedAnchorRect().contains(point) : contains(point);
    const bool inContent = isOpen() && content_ && resolvedContentRect().contains(point);
    return inAnchor || inContent || (isOpen() && outsidePointerPolicy_ != PopupOutsidePointerPolicy::PassThrough);
}

bool Popup::paintsAboveSiblings() const {
    return isOpen();
}

void Popup::setFocusVisible(bool visible) {
    Widget::setFocusVisible(visible);
    if (focusedChild_) {
        focusedChild_->setFocusVisible(visible);
    }
}

Rect Popup::resolvedAnchorRect() const {
    if (anchorRect_) {
        return *anchorRect_;
    }
    const Rect rect = frame();
    const Size preferred = anchor_ ? anchor_->preferredSize() : preferredSize();
    const float width = preferred.width > 0.0f ? std::min(preferred.width, rect.width) : rect.width;
    const float height = preferred.height > 0.0f ? std::min(preferred.height, rect.height) : rect.height;
    return Rect{rect.x, rect.y, width, height};
}

Rect Popup::resolvedViewport() const {
    if (viewport_) {
        return *viewport_;
    }
    return frame();
}

void Popup::layoutAnchor() {
    if (anchor_) {
        anchor_->setFrame(resolvedAnchorRect());
    }
}

void Popup::layoutContent() {
    if (!isOpen() || !content_) {
        return;
    }
    const PopupStyle style = resolvedStyle();
    const Rect outer = resolvedContentRect();
    content_->setFrame(outer.inset(style.padding));
}

void Popup::focusChild(Widget* child, bool focusVisible) {
    if (focusedChild_ == child) {
        if (focusedChild_) {
            focusedChild_->setFocusVisible(focusVisible);
        }
        return;
    }

    if (focusedChild_) {
        focusedChild_->onFocusChanged(false);
    }
    focusedChild_ = child;
    if (focusedChild_) {
        focusedChild_->onFocusChanged(true);
        focusedChild_->setFocusVisible(focusVisible);
    }
}

Widget* Popup::focusedChild() const {
    return focusedChild_;
}

bool Popup::isInteractive(const Widget* child) {
    return child && child->visible() && !child->disabled();
}

void Popup::resetInteractionState() {
    pressedChild_ = nullptr;
    if (anchor_) {
        anchor_->clearInteractionState();
    }
    if (content_) {
        content_->clearInteractionState();
    }
}

} // namespace oneui
