#include "oneui/controls/interactive_surface.h"

#include "internal/scroll_trace.h"

#include <chrono>
#include <utility>

namespace oneui {
namespace {

double currentTimeMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(now).count();
}

InteractiveSurfaceStateStyle defaultState(Color background, Color border) {
    InteractiveSurfaceStateStyle state;
    state.background = background;
    state.border = border;
    state.borderWidth = 1.0f;
    state.radius = 8.0f;
    return state;
}

} // namespace

InteractiveSurface::InteractiveSurface() {
    style_.normal = defaultState(Color{255, 255, 255}, Color{226, 229, 234});
    style_.hovered = defaultState(Color{248, 250, 252}, Color{203, 213, 225});
    style_.pressed = defaultState(Color{241, 245, 249}, Color{148, 163, 184});
    style_.disabled = defaultState(Color{248, 250, 252}, Color{226, 232, 240});
    style_.focusVisible = defaultState(Color{255, 255, 255}, Color{59, 130, 246});
    style_.focusVisible.borderWidth = 2.0f;
}

void InteractiveSurface::setStyle(InteractiveSurfaceStyle style) {
    const auto previous = resolvedStyle();
    style_ = std::move(style);
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
}

void InteractiveSurface::setPadding(Insets padding) {
    padding_ = padding;
    invalidate();
}

void InteractiveSurface::setContent(std::shared_ptr<Widget> child) {
    clearChildren();
    if (child) {
        add(std::move(child));
    }
}

void InteractiveSurface::setOnClick(std::function<void()> callback) {
    onClick_ = std::move(callback);
    setAccessibleRole(
        onClick_ || onPointerActivated_ ? AccessibilityRole::Button : AccessibilityRole::None);
}

void InteractiveSurface::setOnPointerActivated(
    std::function<void(const MouseEvent&)> callback) {
    onPointerActivated_ = std::move(callback);
    setAccessibleRole(
        onClick_ || onPointerActivated_ ? AccessibilityRole::Button : AccessibilityRole::None);
}

void InteractiveSurface::setOnPointerMoved(
    std::function<void(const MouseEvent&)> callback) {
    onPointerMoved_ = std::move(callback);
}

void InteractiveSurface::setOnHoverChanged(std::function<void(bool)> callback) {
    onHoverChanged_ = std::move(callback);
}

void InteractiveSurface::setOnContextMenuRequested(
    std::function<void(const MouseEvent&)> callback) {
    onContextMenuRequested_ = std::move(callback);
}

void InteractiveSurface::setDisabled(bool disabled) {
    const auto previous = resolvedStyle();
    View::setDisabled(disabled);
    beginVisualTransition(previous, resolvedStyle());
}

void InteractiveSurface::paint(Canvas& canvas) {
    const auto style = visualStyle(resolvedStyle());
    const auto rect = frame();
    canvas.fillRect(rect, style.background, style.radius);
    if (style.borderWidth > 0.0f && style.border.a > 0) {
        canvas.strokeRect(rect, style.border, style.radius, style.borderWidth);
    }
    if (focusVisible() && interactive()) {
        const auto& focus = style_.focusVisible;
        if (focus.borderWidth > 0.0f && focus.border.a > 0) {
            canvas.strokeRect(rect, focus.border, focus.radius, focus.borderWidth);
        }
    }
    View::paint(canvas);
}

bool InteractiveSurface::onMouseMove(const MouseEvent& event) {
    const bool childHandled = View::onMouseMove(event);
    const bool nextHovered = interactive() && contains(event.position);
    if (nextHovered && onPointerMoved_) {
        onPointerMoved_(event);
    }
    if (nextHovered == hovered_) {
        return childHandled;
    }

    const auto previous = resolvedStyle();
    hovered_ = nextHovered;
    if (onHoverChanged_) {
        onHoverChanged_(hovered_);
    }
    if (!hovered_) {
        pressed_ = false;
    }
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
    return true;
}

bool InteractiveSurface::onMouseDown(const MouseEvent& event) {
    const bool childHandled = View::onMouseDown(event);
    if (childHandled || !interactive() || !contains(event.position) ||
        (event.button != MouseButton::Left && event.button != MouseButton::Right)) {
        if (internal::scrollTraceEnabled()) {
            internal::writeScrollTrace(internal::ScrollTraceEvent{
                "interactive_surface",
                childHandled ? "pointer_down_child" : "pointer_down_missed",
                reinterpret_cast<std::uintptr_t>(this),
                static_cast<double>(static_cast<int>(event.button)), 0.0,
                static_cast<double>(event.clickCount), 0.0, 0.0, 0.0, 0.0,
                static_cast<double>(event.position.x), static_cast<double>(event.position.y)});
        }
        return childHandled;
    }

    // A composite content view may tentatively focus its first descendant even
    // when the pointer landed on otherwise unhandled card space. In that case
    // the surface itself is the interaction target; keep nested controls
    // focused only when one of them actually handled the pointer event.
    focusChild(nullptr);

    const auto previous = resolvedStyle();
    pressed_ = event.button == MouseButton::Left;
    pressedButton_ = event.button;
    pressedClickCount_ = event.clickCount;
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
    if (internal::scrollTraceEnabled()) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "interactive_surface", "pointer_down_surface",
            reinterpret_cast<std::uintptr_t>(this),
            static_cast<double>(static_cast<int>(event.button)), 0.0,
            static_cast<double>(event.clickCount), 0.0, 0.0, 0.0, 0.0,
            static_cast<double>(event.position.x), static_cast<double>(event.position.y)});
    }
    return true;
}

bool InteractiveSurface::onMouseUp(const MouseEvent& event) {
    const bool childHandled = View::onMouseUp(event);
    if (pressedButton_ == MouseButton::None) {
        return childHandled;
    }

    const auto previous = resolvedStyle();
    const MouseButton pressedButton = pressedButton_;
    const int pressedClickCount = pressedClickCount_;
    const bool activate = contains(event.position) && !childHandled &&
        event.button == pressedButton;
    pressed_ = false;
    pressedButton_ = MouseButton::None;
    pressedClickCount_ = 1;
    beginVisualTransition(previous, resolvedStyle());
    invalidate();
    if (activate && pressedButton == MouseButton::Left) {
        MouseEvent activation = event;
        activation.clickCount = pressedClickCount;
        const auto pointerActivated = onPointerActivated_;
        const auto click = onClick_;
        if (pointerActivated) {
            pointerActivated(activation);
        } else if (click) {
            click();
        }
    } else if (activate && pressedButton == MouseButton::Right && onContextMenuRequested_) {
        onContextMenuRequested_(event);
    }
    if (internal::scrollTraceEnabled()) {
        internal::writeScrollTrace(internal::ScrollTraceEvent{
            "interactive_surface", activate ? "pointer_up_activated" : "pointer_up_cancelled",
            reinterpret_cast<std::uintptr_t>(this),
            static_cast<double>(static_cast<int>(event.button)), 0.0,
            static_cast<double>(pressedClickCount), 0.0, 0.0, 0.0, 0.0,
            static_cast<double>(event.position.x), static_cast<double>(event.position.y)});
    }
    return true;
}

bool InteractiveSurface::onKeyDown(const KeyEvent& event) {
    if (!interactive()) {
        return false;
    }

    if (event.key == Key::Tab) {
        // Forward Tab into nested actions after the surface's own tab stop.
        // Shift+Tab from the surface itself must leave the surface instead of
        // jumping directly to its last descendant.
        if (event.shift && !focusedChild()) {
            return false;
        }
        return View::onKeyDown(event);
    }

    if (focusedChild() && View::onKeyDown(event)) {
        return true;
    }

    if (event.key != Key::Enter && event.key != Key::Space) {
        return false;
    }

    const auto click = onClick_;
    const auto pointerActivated = onPointerActivated_;
    if (click) {
        click();
    } else if (pointerActivated) {
        MouseEvent activation;
        const auto rect = frame();
        activation.position = Point{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
        activation.button = MouseButton::Left;
        activation.shift = event.shift;
        activation.control = event.control;
        activation.alt = event.alt;
        activation.clickCount = 1;
        pointerActivated(activation);
    }
    return true;
}

bool InteractiveSurface::onFocusChanged(bool focused) {
    if (!focused) {
        return View::onFocusChanged(false);
    }

    // InteractiveSurface is both a composite container and an action in its
    // own right. Focusing the surface must not automatically skip its semantic
    // action by descending into the first nested control.
    const bool changed = Widget::onFocusChanged(true);
    if (auto* child = focusedChild()) {
        child->onFocusChanged(true);
    }
    return changed;
}

CursorKind InteractiveSurface::cursor(Point point) const {
    const auto childCursor = View::cursor(point);
    if (childCursor != CursorKind::Default) {
        return childCursor;
    }
    return interactive() && contains(point) ? CursorKind::Pointer : CursorKind::Default;
}

bool InteractiveSurface::isFocusable() const {
    return static_cast<bool>(onClick_ || onPointerActivated_) && interactive();
}

bool InteractiveSurface::tickAnimations(double nowMs) {
    bool running = View::tickAnimations(nowMs);
    running = backgroundTransition_.tick(nowMs) || running;
    running = borderTransition_.tick(nowMs) || running;
    if (running) {
        invalidate();
    }
    return running;
}

void InteractiveSurface::layoutChildren() {
    const Rect content = frame().inset(padding_);
    for (const auto& child : children()) {
        if (child->visible()) {
            child->setFrame(content);
        }
    }
}

bool InteractiveSurface::hasInteractionState() const {
    return hovered_ || pressed_ || View::hasInteractionState();
}

void InteractiveSurface::resetInteractionState() {
    const bool wasHovered = hovered_;
    hovered_ = false;
    pressed_ = false;
    pressedButton_ = MouseButton::None;
    pressedClickCount_ = 1;
    View::resetInteractionState();
    const auto target = resolvedStyle();
    backgroundTransition_.reset(target.background);
    borderTransition_.reset(target.border);
    visualInitialized_ = true;
    if (wasHovered && onHoverChanged_) {
        onHoverChanged_(false);
    }
}

InteractiveSurfaceStateStyle InteractiveSurface::resolvedStyle() const {
    if (disabled()) {
        return style_.disabled;
    }
    if (pressed_) {
        return style_.pressed;
    }
    if (hovered_) {
        return style_.hovered;
    }
    return style_.normal;
}

InteractiveSurfaceStateStyle InteractiveSurface::visualStyle(
    InteractiveSurfaceStateStyle target) const {
    if (!visualInitialized_) {
        return target;
    }
    target.background = backgroundTransition_.value();
    target.border = borderTransition_.value();
    return target;
}

void InteractiveSurface::beginVisualTransition(
    InteractiveSurfaceStateStyle from,
    InteractiveSurfaceStateStyle target) {
    if (!hasAnimationScheduler()) {
        visualInitialized_ = false;
        return;
    }
    if (!visualInitialized_) {
        backgroundTransition_.reset(from.background);
        borderTransition_.reset(from.border);
        visualInitialized_ = true;
    }

    const double nowMs = currentTimeMs();
    backgroundTransition_.animateTo(target.background, nowMs, style_.transition);
    borderTransition_.animateTo(target.border, nowMs, style_.transition);
    if (backgroundTransition_.running() || borderTransition_.running()) {
        requestAnimationFrame();
    }
}

} // namespace oneui
