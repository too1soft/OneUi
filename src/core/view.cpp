#include "oneui/view.h"

#include <algorithm>

namespace oneui {
namespace {

bool intersects(Rect lhs, Rect rhs) {
    return lhs.x < rhs.x + rhs.width &&
           lhs.x + lhs.width > rhs.x &&
           lhs.y < rhs.y + rhs.height &&
           lhs.y + lhs.height > rhs.y;
}

} // namespace

void View::add(std::shared_ptr<Widget> child) {
    child->setInvalidator([this] {
        invalidate();
    });
    child->setRectInvalidator([this](Rect rect) {
        invalidateRect(rect);
    });
    child->setAnimationScheduler([this] {
        requestAnimationFrame();
    });
    children_.push_back(std::move(child));
}

void View::clearChildren() {
    if (focusedChild_) {
        focusedChild_->onFocusChanged(false);
    }
    focusedChild_ = nullptr;
    pressedChild_ = nullptr;
    hoveredChild_ = nullptr;
    children_.clear();
}

const std::vector<std::shared_ptr<Widget>>& View::children() const {
    return children_;
}

void View::setInvalidator(std::function<void()> invalidator) {
    Widget::setInvalidator(std::move(invalidator));
    for (const auto& child : children_) {
        child->setInvalidator([this] {
            invalidate();
        });
    }
}

void View::setRectInvalidator(std::function<void(Rect)> invalidator) {
    Widget::setRectInvalidator(std::move(invalidator));
    for (const auto& child : children_) {
        child->setRectInvalidator([this](Rect rect) {
            invalidateRect(rect);
        });
    }
}

void View::setAnimationScheduler(std::function<void()> scheduler) {
    Widget::setAnimationScheduler(std::move(scheduler));
    for (const auto& child : children_) {
        child->setAnimationScheduler([this] {
            requestAnimationFrame();
        });
    }
}

void View::paint(Canvas& canvas) {
    layoutChildren();
    const auto clip = canvas.clipBounds();
    const bool canCullByClip = clip && clip->width > 0.0f && clip->height > 0.0f;
    bool hasAboveSiblings = false;
    for (const auto& child : children_) {
        if (!child->visible()) {
            continue;
        }
        if (canCullByClip && !intersects(child->frame(), *clip)) {
            continue;
        }
        if (child->paintsAboveSiblings()) {
            hasAboveSiblings = true;
            continue;
        }
        child->paint(canvas);
    }

    if (!hasAboveSiblings) {
        return;
    }

    for (const auto& child : children_) {
        if (!child->visible() || !child->paintsAboveSiblings()) {
            continue;
        }
        if (canCullByClip && !intersects(child->frame(), *clip)) {
            continue;
        }
        child->paint(canvas);
    }
}

bool View::onMouseMove(const MouseEvent& event) {
    if (!interactive()) {
        clearInteractionState();
        return false;
    }

    if (pressedChild_ && isChildInteractive(pressedChild_)) {
        const bool changed = pressedChild_->onMouseMove(event);
        if (changed) {
            if (hoveredChild_ && hoveredChild_ != pressedChild_) {
                clearHoveredChildExcept(pressedChild_);
            }
            hoveredChild_ = pressedChild_;
        }
        return changed;
    }

    Widget* child = hitTestChild(event.position);
    bool changed = false;
    if (child != hoveredChild_) {
        changed = clearHoveredChildExcept(child);
        hoveredChild_ = child;
    }

    if (!child || !isChildInteractive(child)) {
        return changed;
    }

    return child->onMouseMove(event) || changed;
}

bool View::onMouseDown(const MouseEvent& event) {
    if (!interactive()) {
        clearInteractionState();
        focusChild(nullptr);
        return false;
    }

    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Widget& child = **it;
        if (!child.visible() || child.disabled() || !child.paintsAboveSiblings() || !child.hitTest(event.position)) {
            continue;
        }

        pressedChild_ = &child;
        const bool handled = child.onMouseDown(event);
        if (handled) {
            focusChild(&child, false);
            return true;
        }
        pressedChild_ = nullptr;
    }

    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Widget& child = **it;
        if (!child.visible() || child.disabled() || child.paintsAboveSiblings() || !child.hitTest(event.position)) {
            continue;
        }

        pressedChild_ = &child;
        const bool handled = child.onMouseDown(event);
        if (handled || child.isFocusable()) {
            focusChild(&child, false);
        }
        return handled || child.isFocusable();
    }

    focusChild(nullptr);
    pressedChild_ = nullptr;
    return false;
}

bool View::onMouseUp(const MouseEvent& event) {
    if (!interactive()) {
        pressedChild_ = nullptr;
        return false;
    }

    if (!pressedChild_) {
        return false;
    }

    Widget* child = pressedChild_;
    pressedChild_ = nullptr;
    if (!isChildInteractive(child)) {
        return false;
    }
    return child->onMouseUp(event);
}

bool View::onMouseWheel(const MouseWheelEvent& event) {
    if (!interactive()) {
        return false;
    }

    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Widget& child = **it;
        if (!child.visible() || child.disabled() || !child.hitTest(event.position)) {
            continue;
        }
        if (child.onMouseWheel(event)) {
            return true;
        }
    }

    return false;
}

bool View::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (event.key == Key::Tab) {
        // 深度优先：先让当前聚焦子树在更深层推进（表单里逐个字段切换）；
        // 到本层边界（focusNext 返回 false，不回绕）再冒泡给上层——回绕由焦点作用域
        // （模态 overlay 或窗口内容，见 OverlayHost::onKeyDown）统一负责。
        // 键盘导航即使已经抵达当前作用域的边界，也必须让当前焦点显示焦点环。
        // 这样首次通过 Tab 进入窗口时不会保留鼠标焦点样式。
        if (focusedChild_ && isChildInteractive(focusedChild_)) {
            focusedChild_->setFocusVisible(true);
        }
        if (focusedChild_ && isChildInteractive(focusedChild_) && focusedChild_->onKeyDown(event)) {
            return true;
        }
        return focusNext(event.shift);
    }

    if (focusedChild_ && !isChildInteractive(focusedChild_)) {
        focusChild(nullptr);
    }

    if (focusedChild_ && focusedChild_->onKeyDown(event)) {
        return true;
    }

    return false;
}

bool View::onKeyUp(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (focusedChild_ && !isChildInteractive(focusedChild_)) {
        focusChild(nullptr);
    }

    return focusedChild_ ? focusedChild_->onKeyUp(event) : false;
}

bool View::onTextInput(wchar_t character) {
    if (!interactive()) {
        return false;
    }

    if (focusedChild_ && !isChildInteractive(focusedChild_)) {
        focusChild(nullptr);
    }
    return focusedChild_ ? focusedChild_->onTextInput(character) : false;
}

bool View::onFocusChanged(bool focused) {
    setFocused(focused);
    if (!focused && focusedChild_) {
        focusedChild_->onFocusChanged(false);
    } else if (focused) {
        if (focusedChild_ && !isChildInteractive(focusedChild_)) {
            focusChild(nullptr);
        }

        if (!focusedChild_) {
            focusNext(false, false);
        } else {
            focusedChild_->onFocusChanged(true);
        }
    }
    return true;
}

bool View::isFocusable() const {
    return interactive() && !focusableChildren().empty();
}

CursorKind View::cursor(Point point) const {
    if (!interactive() || !hitTest(point)) {
        return CursorKind::Default;
    }

    if (pressedChild_ && pressedChild_->visible() && pressedChild_->hitTest(point)) {
        return pressedChild_->cursor(point);
    }

    if (hoveredChild_ && hoveredChild_->visible() && hoveredChild_->hitTest(point)) {
        return hoveredChild_->cursor(point);
    }

    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        const auto& child = *it;
        if (!child->visible() || !child->paintsAboveSiblings() || !child->hitTest(point)) {
            continue;
        }
        return child->cursor(point);
    }

    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        const auto& child = *it;
        if (!child->visible() || child->paintsAboveSiblings() || !child->hitTest(point)) {
            continue;
        }
        return child->cursor(point);
    }

    return Widget::cursor(point);
}

void View::setFocusVisible(bool visible) {
    Widget::setFocusVisible(visible);
    if (focusedChild_) {
        focusedChild_->setFocusVisible(visible);
    }
}

bool View::tickAnimations(double nowMs) {
    bool running = false;
    for (const auto& child : children_) {
        if (child->visible()) {
            running = child->tickAnimations(nowMs) || running;
        }
    }
    return running;
}

void View::layoutChildren() {}

bool View::hasInteractionState() const {
    return pressedChild_ != nullptr || hoveredChild_ != nullptr || focusedChild_ != nullptr || focused();
}

void View::resetInteractionState() {
    pressedChild_ = nullptr;
    hoveredChild_ = nullptr;
    for (const auto& child : children_) {
        child->clearInteractionState();
    }
}

Widget* View::focusedChild() const {
    return focusedChild_;
}

void View::focusChild(Widget* child, bool focusVisible) {
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

bool View::focusNext(bool reverse, bool focusVisible) {
    const auto focusable = focusableChildren();
    if (focusable.empty()) {
        return false;
    }

    auto it = std::find(focusable.begin(), focusable.end(), focusedChild_);
    int index = it == focusable.end() ? (reverse ? static_cast<int>(focusable.size()) : -1)
                                      : static_cast<int>(it - focusable.begin());
    index += reverse ? -1 : 1;

    // 不回绕：越界即到本层边界，返回 false 交给上层继续冒泡（回绕由焦点作用域负责）。
    if (index < 0 || index >= static_cast<int>(focusable.size())) {
        return false;
    }

    Widget* target = focusable[static_cast<std::size_t>(index)];
    focusChild(target, focusVisible);
    // 进入容器：正向落到其首个叶子（focusChild→onFocusChanged 已处理），反向落到末叶子。
    if (reverse) {
        target->focusLastLeaf();
    }
    return true;
}

bool View::focusFirstLeaf() {
    const auto focusable = focusableChildren();
    if (focusable.empty()) {
        return false;
    }
    focusChild(focusable.front(), true); // 进入首个可聚焦子；容器会递归聚焦其首叶
    return true;
}

bool View::focusLastLeaf() {
    const auto focusable = focusableChildren();
    if (focusable.empty()) {
        return false;
    }
    Widget* last = focusable.back();
    focusChild(last, true);
    last->focusLastLeaf(); // 若是容器，递归到末叶
    return true;
}

bool View::isChildInteractive(const Widget* child) {
    return child && child->visible() && !child->disabled();
}

Widget* View::hitTestChild(Point point) const {
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Widget* child = it->get();
        if (child->visible() && child->paintsAboveSiblings() && child->hitTest(point)) {
            return child;
        }
    }

    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        Widget* child = it->get();
        if (child->visible() && !child->paintsAboveSiblings() && child->hitTest(point)) {
            return child;
        }
    }

    return nullptr;
}

bool View::clearHoveredChildExcept(Widget* child) {
    if (!hoveredChild_ || hoveredChild_ == child) {
        return false;
    }

    if (isChildInteractive(hoveredChild_)) {
        return hoveredChild_->clearInteractionState();
    }

    return false;
}

std::vector<Widget*> View::focusableChildren() const {
    std::vector<Widget*> result;
    for (const auto& child : children_) {
        if (child->visible() && !child->disabled() && child->tabStop() && child->isFocusable()) {
            result.push_back(child.get());
        }
    }
    return result;
}

} // namespace oneui
