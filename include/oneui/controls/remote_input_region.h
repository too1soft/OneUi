#pragma once

#include "oneui/export.h"
#include "oneui/widget.h"

#include <cstdint>
#include <functional>
#include <set>
#include <vector>

namespace oneui {

enum class PointerButton {
    None,
    Left,
    Right,
    Middle,
    X1,
    X2
};

enum class RemoteInputScaleMode {
    ActualSize,
    Fit,
    Fill,
    Stretch
};

enum class RemoteCursorMode {
    Default,
    Hidden,
    Bitmap
};

struct RemotePointerEvent {
    Point windowPosition;
    Point contentPosition;
    Point normalizedPosition;
    Point remotePosition;
    PointerButton button = PointerButton::None;
    bool pressed = false;
    int wheelDeltaX = 0;
    int wheelDeltaY = 0;
};

struct RawKeyEvent {
    std::uint32_t virtualKey = 0;
    std::uint32_t scanCode = 0;
    bool pressed = false;
    bool repeat = false;
    bool extended = false;
    bool alt = false;
    bool ctrl = false;
    bool shift = false;
    bool win = false;
};

class ONEUI_API RemoteInputRegion final : public Widget {
public:
    using PointerCallback = std::function<void(const RemotePointerEvent&)>;
    using RawKeyCallback = std::function<void(const RawKeyEvent&)>;
    using TextInputCallback = std::function<void(const std::wstring&)>;

    RemoteInputRegion();

    void setRemoteSize(Size size);
    Size remoteSize() const;
    void setScaleMode(RemoteInputScaleMode mode);
    RemoteInputScaleMode scaleMode() const;
    Rect contentRect() const;

    void setRemoteCursorMode(RemoteCursorMode mode);
    RemoteCursorMode remoteCursorMode() const;
    void setRemoteCursorPosition(Point position);
    Point remoteCursorPosition() const;
    bool setRemoteCursorBitmap(
        const std::uint8_t* pixels,
        std::size_t pixelBytes,
        int width,
        int height,
        int stride,
        int hotspotX,
        int hotspotY);

    void setOnPointer(PointerCallback callback);
    void setOnRawKey(RawKeyCallback callback);
    void setOnTextInput(TextInputCallback callback);
    bool dispatchPointer(Point windowPosition, PointerButton button, bool pressed, int wheelDeltaX = 0, int wheelDeltaY = 0);
    bool dispatchRawKey(RawKeyEvent event);
    bool dispatchTextInput(const std::wstring& text);
    void releaseAllInputs();

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onKeyUp(const KeyEvent& event) override;
    bool onTextInputText(const std::wstring& text) override;
    Rect textInputCaretRect() const override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    CursorKind cursor(Point point) const override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    struct RawKeyIdentity {
        std::uint32_t virtualKey = 0;
        std::uint32_t scanCode = 0;
        bool extended = false;

        bool operator<(const RawKeyIdentity& other) const;
    };

    RemotePointerEvent makePointerEvent(Point windowPosition, PointerButton button, bool pressed, int wheelDeltaX, int wheelDeltaY) const;
    Rect remoteCursorRect() const;
    void invalidateCursorTransition(Rect previousRect);
    void resetInteractionState() override;

    struct RemoteCursorBitmap {
        std::vector<std::uint8_t> pixels;
        int width = 0;
        int height = 0;
        int stride = 0;
        int hotspotX = 0;
        int hotspotY = 0;
    };

    Size remoteSize_{0.0f, 0.0f};
    RemoteInputScaleMode scaleMode_ = RemoteInputScaleMode::Fit;
    PointerCallback onPointer_;
    RawKeyCallback onRawKey_;
    TextInputCallback onTextInput_;
    std::set<PointerButton> pressedButtons_;
    std::set<RawKeyIdentity> pressedKeys_;
    std::set<RawKeyIdentity> textInputKeys_;
    Point lastPointerPosition_{};
    bool hasPointerPosition_ = false;
    RemoteCursorMode remoteCursorMode_ = RemoteCursorMode::Default;
    Point remoteCursorPosition_{};
    bool hasRemoteCursorPosition_ = false;
    RemoteCursorBitmap remoteCursorBitmap_;
};

} // namespace oneui
