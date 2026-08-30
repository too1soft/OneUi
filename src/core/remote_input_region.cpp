#include "oneui/controls/remote_input_region.h"

#include "oneui/color.h"

#include <algorithm>
#include <utility>

namespace oneui {
namespace {

float clamp01(float value) {
    return std::max(0.0f, std::min(value, 1.0f));
}

bool prefersCommittedText(const KeyEvent& event) {
    if (event.control || event.alt || event.win) {
        return false;
    }
    const std::uint32_t key = event.virtualKey;
    return key == 0x20
        || (key >= 0x30 && key <= 0x5A)
        || (key >= 0x60 && key <= 0x6F)
        || (key >= 0xBA && key <= 0xE2)
        || key == 0xE5
        || key == 0xE7;
}

} // namespace

bool RemoteInputRegion::RawKeyIdentity::operator<(const RawKeyIdentity& other) const {
    if (virtualKey != other.virtualKey) {
        return virtualKey < other.virtualKey;
    }
    if (scanCode != other.scanCode) {
        return scanCode < other.scanCode;
    }
    return extended < other.extended;
}

RemoteInputRegion::RemoteInputRegion() {
    setPreferredSize(Size{320.0f, 200.0f});
    setAccessibleRole(AccessibilityRole::Custom);
    setAccessibleName(L"Remote input region");
}

void RemoteInputRegion::setRemoteSize(Size size) {
    remoteSize_.width = std::max(0.0f, size.width);
    remoteSize_.height = std::max(0.0f, size.height);
    invalidate();
}

Size RemoteInputRegion::remoteSize() const {
    return remoteSize_;
}

void RemoteInputRegion::setScaleMode(RemoteInputScaleMode mode) {
    if (scaleMode_ == mode) {
        return;
    }
    scaleMode_ = mode;
    invalidate();
}

RemoteInputScaleMode RemoteInputRegion::scaleMode() const {
    return scaleMode_;
}

Rect RemoteInputRegion::contentRect() const {
    const Rect bounds = frame();
    if (bounds.width <= 0.0f || bounds.height <= 0.0f || remoteSize_.width <= 0.0f || remoteSize_.height <= 0.0f) {
        return bounds;
    }

    if (scaleMode_ == RemoteInputScaleMode::Stretch) {
        return bounds;
    }

    float scale = 1.0f;
    if (scaleMode_ == RemoteInputScaleMode::ActualSize) {
        scale = 1.0f;
    } else {
        const float scaleX = bounds.width / remoteSize_.width;
        const float scaleY = bounds.height / remoteSize_.height;
        scale = scaleMode_ == RemoteInputScaleMode::Fill ? std::max(scaleX, scaleY) : std::min(scaleX, scaleY);
    }

    const float width = remoteSize_.width * scale;
    const float height = remoteSize_.height * scale;
    return Rect{bounds.x + (bounds.width - width) / 2.0f, bounds.y + (bounds.height - height) / 2.0f, width, height};
}

void RemoteInputRegion::setOnPointer(PointerCallback callback) {
    onPointer_ = std::move(callback);
}

void RemoteInputRegion::setOnRawKey(RawKeyCallback callback) {
    onRawKey_ = std::move(callback);
}

void RemoteInputRegion::setOnTextInput(TextInputCallback callback) {
    onTextInput_ = std::move(callback);
}

bool RemoteInputRegion::dispatchPointer(Point windowPosition, PointerButton button, bool pressed, int wheelDeltaX, int wheelDeltaY) {
    if (!interactive()) {
        return false;
    }
    if (remoteSize_.width <= 0.0f || remoteSize_.height <= 0.0f) {
        return false;
    }

    if (button != PointerButton::None) {
        if (pressed) {
            pressedButtons_.insert(button);
        } else {
            pressedButtons_.erase(button);
        }
    }
    lastPointerPosition_ = windowPosition;
    hasPointerPosition_ = true;

    if (onPointer_) {
        onPointer_(makePointerEvent(windowPosition, button, pressed, wheelDeltaX, wheelDeltaY));
    }
    return true;
}

bool RemoteInputRegion::dispatchRawKey(RawKeyEvent event) {
    if (!interactive()) {
        return false;
    }

    const RawKeyIdentity identity{event.virtualKey, event.scanCode, event.extended};
    if (event.pressed) {
        pressedKeys_.insert(identity);
    } else {
        pressedKeys_.erase(identity);
    }

    if (onRawKey_) {
        onRawKey_(event);
    }
    return true;
}

bool RemoteInputRegion::dispatchTextInput(const std::wstring& text) {
    if (!interactive() || text.empty() || !onTextInput_) {
        return false;
    }
    onTextInput_(text);
    return true;
}

void RemoteInputRegion::releaseAllInputs() {
    if (onPointer_) {
        for (const PointerButton button : pressedButtons_) {
            onPointer_(makePointerEvent(Point{}, button, false, 0, 0));
        }
    }
    pressedButtons_.clear();

    if (onRawKey_) {
        for (const RawKeyIdentity& key : pressedKeys_) {
            RawKeyEvent event;
            event.virtualKey = key.virtualKey;
            event.scanCode = key.scanCode;
            event.extended = key.extended;
            event.pressed = false;
            onRawKey_(event);
        }
    }
    pressedKeys_.clear();
    textInputKeys_.clear();
}

void RemoteInputRegion::paint(Canvas& canvas) {
    (void)canvas;
}

bool RemoteInputRegion::onMouseMove(const MouseEvent& event) {
    return dispatchPointer(event.position, PointerButton::None, false, 0, 0);
}

static PointerButton toPointerButton(MouseButton button) {
    switch (button) {
        case MouseButton::None:   return PointerButton::None;
        case MouseButton::Left:   return PointerButton::Left;
        case MouseButton::Right:  return PointerButton::Right;
        case MouseButton::Middle: return PointerButton::Middle;
    }
    return PointerButton::None;
}

bool RemoteInputRegion::onMouseDown(const MouseEvent& event) {
    return dispatchPointer(event.position, toPointerButton(event.button), true, 0, 0);
}

bool RemoteInputRegion::onMouseUp(const MouseEvent& event) {
    return dispatchPointer(event.position, toPointerButton(event.button), false, 0, 0);
}

bool RemoteInputRegion::onMouseWheel(const MouseWheelEvent& event) {
    return dispatchPointer(event.position, PointerButton::None, false, 0, static_cast<int>(event.deltaY * 120.0f));
}

bool RemoteInputRegion::onKeyDown(const KeyEvent& event) {
    if (onTextInput_ && interactive() && prefersCommittedText(event)) {
        textInputKeys_.insert(RawKeyIdentity{event.virtualKey, event.scanCode, event.extended});
        return true;
    }
    RawKeyEvent raw;
    raw.virtualKey = event.virtualKey;
    raw.scanCode = event.scanCode;
    raw.pressed = true;
    raw.repeat = event.repeat;
    raw.extended = event.extended;
    raw.alt = event.alt;
    raw.ctrl = event.control;
    raw.shift = event.shift;
    raw.win = event.win;
    return dispatchRawKey(raw);
}

bool RemoteInputRegion::onKeyUp(const KeyEvent& event) {
    const RawKeyIdentity identity{event.virtualKey, event.scanCode, event.extended};
    if (textInputKeys_.erase(identity) > 0) {
        return true;
    }
    RawKeyEvent raw;
    raw.virtualKey = event.virtualKey;
    raw.scanCode = event.scanCode;
    raw.pressed = false;
    raw.repeat = false;
    raw.extended = event.extended;
    raw.alt = event.alt;
    raw.ctrl = event.control;
    raw.shift = event.shift;
    raw.win = event.win;
    return dispatchRawKey(raw);
}

bool RemoteInputRegion::onTextInputText(const std::wstring& text) {
    return dispatchTextInput(text);
}

Rect RemoteInputRegion::textInputCaretRect() const {
    const Rect content = contentRect();
    if (content.width <= 0.0f || content.height <= 0.0f) {
        return Rect{};
    }
    const float fallbackX = content.x + std::min(8.0f, content.width);
    const float fallbackY = content.y + std::max(0.0f, content.height - 24.0f);
    const float x = hasPointerPosition_
        ? std::clamp(lastPointerPosition_.x, content.x, content.x + content.width)
        : fallbackX;
    const float y = hasPointerPosition_
        ? std::clamp(lastPointerPosition_.y, content.y, content.y + content.height)
        : fallbackY;
    return Rect{x, y, 1.0f, std::min(20.0f, content.height)};
}

bool RemoteInputRegion::onFocusChanged(bool focused) {
    const bool changed = Widget::onFocusChanged(focused);
    const bool hadPressedInputs = !pressedButtons_.empty() || !pressedKeys_.empty() || !textInputKeys_.empty();
    if (!focused) {
        releaseAllInputs();
    }
    return changed || hadPressedInputs;
}

bool RemoteInputRegion::isFocusable() const {
    return interactive();
}

AccessibilityInfo RemoteInputRegion::accessibilityInfo() const {
    auto info = Widget::accessibilityInfo();
    if (info.role == AccessibilityRole::None) {
        info.role = AccessibilityRole::Custom;
    }
    if (info.name.empty()) {
        info.name = L"Remote input region";
    }
    return info;
}

RemotePointerEvent RemoteInputRegion::makePointerEvent(Point windowPosition, PointerButton button, bool pressed, int wheelDeltaX, int wheelDeltaY) const {
    const Rect content = contentRect();
    const float localX = content.width <= 0.0f ? 0.0f : windowPosition.x - content.x;
    const float localY = content.height <= 0.0f ? 0.0f : windowPosition.y - content.y;
    const Point normalized{content.width <= 0.0f ? 0.0f : clamp01(localX / content.width), content.height <= 0.0f ? 0.0f : clamp01(localY / content.height)};

    return RemotePointerEvent{
        windowPosition,
        Point{localX, localY},
        normalized,
        Point{normalized.x * remoteSize_.width, normalized.y * remoteSize_.height},
        button,
        pressed,
        wheelDeltaX,
        wheelDeltaY
    };
}

void RemoteInputRegion::resetInteractionState() {
    releaseAllInputs();
}

} // namespace oneui
