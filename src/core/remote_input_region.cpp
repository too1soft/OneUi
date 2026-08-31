#include "oneui/controls/remote_input_region.h"

#include "oneui/color.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace oneui {
namespace {

float clamp01(float value) {
    return std::max(0.0f, std::min(value, 1.0f));
}

Rect unionRects(Rect left, Rect right) {
    if (left.width <= 0.0f || left.height <= 0.0f) {
        return right;
    }
    if (right.width <= 0.0f || right.height <= 0.0f) {
        return left;
    }
    const float x = std::min(left.x, right.x);
    const float y = std::min(left.y, right.y);
    const float rightEdge = std::max(left.x + left.width, right.x + right.width);
    const float bottomEdge = std::max(left.y + left.height, right.y + right.height);
    return Rect{x, y, rightEdge - x, bottomEdge - y};
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

void RemoteInputRegion::setRemoteCursorMode(RemoteCursorMode mode) {
    if (mode == RemoteCursorMode::Bitmap && remoteCursorBitmap_.pixels.empty()) {
        mode = RemoteCursorMode::Default;
    }
    if (remoteCursorMode_ == mode) {
        return;
    }
    const Rect previousRect = remoteCursorRect();
    remoteCursorMode_ = mode;
    invalidateCursorTransition(previousRect);
}

RemoteCursorMode RemoteInputRegion::remoteCursorMode() const {
    return remoteCursorMode_;
}

void RemoteInputRegion::setRemoteCursorPosition(Point position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
        return;
    }
    const Rect previousRect = remoteCursorRect();
    remoteCursorPosition_.x = std::clamp(position.x, 0.0f, remoteSize_.width);
    remoteCursorPosition_.y = std::clamp(position.y, 0.0f, remoteSize_.height);
    hasRemoteCursorPosition_ = true;
    invalidateCursorTransition(previousRect);
}

Point RemoteInputRegion::remoteCursorPosition() const {
    return remoteCursorPosition_;
}

bool RemoteInputRegion::setRemoteCursorBitmap(
    const std::uint8_t* pixels,
    std::size_t pixelBytes,
    int width,
    int height,
    int stride,
    int hotspotX,
    int hotspotY) {
    constexpr int maxCursorDimension = 512;
    if (!pixels || width <= 0 || height <= 0 || width > maxCursorDimension || height > maxCursorDimension) {
        return false;
    }
    if (width > std::numeric_limits<int>::max() / 4) {
        return false;
    }
    const int rowBytes = width * 4;
    if (stride < rowBytes || hotspotX < 0 || hotspotX >= width || hotspotY < 0 || hotspotY >= height) {
        return false;
    }
    const std::size_t rowsBeforeLast = static_cast<std::size_t>(height - 1);
    const std::size_t strideBytes = static_cast<std::size_t>(stride);
    if (rowsBeforeLast > 0 && strideBytes > (std::numeric_limits<std::size_t>::max() - static_cast<std::size_t>(rowBytes)) / rowsBeforeLast) {
        return false;
    }
    const std::size_t requiredBytes = rowsBeforeLast * strideBytes + static_cast<std::size_t>(rowBytes);
    if (pixelBytes < requiredBytes) {
        return false;
    }

    const Rect previousRect = remoteCursorRect();
    remoteCursorBitmap_.pixels.assign(pixels, pixels + requiredBytes);
    remoteCursorBitmap_.width = width;
    remoteCursorBitmap_.height = height;
    remoteCursorBitmap_.stride = stride;
    remoteCursorBitmap_.hotspotX = hotspotX;
    remoteCursorBitmap_.hotspotY = hotspotY;
    remoteCursorMode_ = RemoteCursorMode::Bitmap;
    invalidateCursorTransition(previousRect);
    return true;
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
    const RemotePointerEvent remoteEvent = makePointerEvent(windowPosition, button, pressed, wheelDeltaX, wheelDeltaY);
    const Rect previousCursorRect = remoteCursorRect();
    lastPointerPosition_ = windowPosition;
    hasPointerPosition_ = true;
    remoteCursorPosition_ = remoteEvent.remotePosition;
    hasRemoteCursorPosition_ = true;
    invalidateCursorTransition(previousCursorRect);

    if (onPointer_) {
        onPointer_(remoteEvent);
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
    if (remoteCursorMode_ != RemoteCursorMode::Bitmap || remoteCursorBitmap_.pixels.empty() || !hasRemoteCursorPosition_) {
        return;
    }
    const Rect cursorRect = remoteCursorRect();
    if (cursorRect.width <= 0.0f || cursorRect.height <= 0.0f) {
        return;
    }
    canvas.save();
    canvas.clipRect(contentRect());
    canvas.drawPixels(
        cursorRect,
        remoteCursorBitmap_.pixels.data(),
        remoteCursorBitmap_.width,
        remoteCursorBitmap_.height,
        remoteCursorBitmap_.stride,
        CanvasPixelFormat::Rgba8888);
    canvas.restore();
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

CursorKind RemoteInputRegion::cursor(Point point) const {
    if (contains(point) && (remoteCursorMode_ == RemoteCursorMode::Hidden || remoteCursorMode_ == RemoteCursorMode::Bitmap)) {
        return CursorKind::Hidden;
    }
    return Widget::cursor(point);
}

Rect RemoteInputRegion::remoteCursorRect() const {
    if (remoteCursorMode_ != RemoteCursorMode::Bitmap || remoteCursorBitmap_.pixels.empty() || !hasRemoteCursorPosition_ ||
        remoteSize_.width <= 0.0f || remoteSize_.height <= 0.0f) {
        return Rect{};
    }
    const Rect content = contentRect();
    if (content.width <= 0.0f || content.height <= 0.0f) {
        return Rect{};
    }
    const float scaleX = content.width / remoteSize_.width;
    const float scaleY = content.height / remoteSize_.height;
    return Rect{
        content.x + remoteCursorPosition_.x * scaleX - static_cast<float>(remoteCursorBitmap_.hotspotX) * scaleX,
        content.y + remoteCursorPosition_.y * scaleY - static_cast<float>(remoteCursorBitmap_.hotspotY) * scaleY,
        static_cast<float>(remoteCursorBitmap_.width) * scaleX,
        static_cast<float>(remoteCursorBitmap_.height) * scaleY};
}

void RemoteInputRegion::invalidateCursorTransition(Rect previousRect) {
    const Rect dirty = unionRects(previousRect, remoteCursorRect());
    if (dirty.width > 0.0f && dirty.height > 0.0f) {
        invalidateRect(Rect{dirty.x - 1.0f, dirty.y - 1.0f, dirty.width + 2.0f, dirty.height + 2.0f});
    }
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
