#pragma once

#include "oneui/canvas.h"
#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/reactive.h"

#include <functional>
#include <string>

namespace oneui {

enum class MouseButton {
    None,
    Left,
    Right,
    Middle
};

struct MouseEvent {
    Point position;
    MouseButton button = MouseButton::Left;
};

struct MouseWheelEvent {
    Point position;
    float deltaY = 0.0f;
};

enum class Key {
    Tab,
    Enter,
    Space,
    Backspace,
    Left,
    Right,
    Up,
    Down,
    Escape,
    Other,
    Home,
    End,
    Delete,
    A,
    C,
    V,
    X
};

enum class CursorKind {
    Default,
    Pointer,
    Text,
    Crosshair,
    Grab,
    ResizeHorizontal,
    ResizeVertical
};

struct KeyEvent {
    Key key = Key::Other;
    bool shift = false;
    bool control = false;
    unsigned int virtualKey = 0;
    unsigned int scanCode = 0;
    bool pressed = true;
    bool repeat = false;
    bool extended = false;
    bool alt = false;
    bool win = false;
};

enum class AccessibilityRole {
    None,
    Button,
    Text,
    TextBox,
    CheckBox,
    RadioButton,
    RadioGroup,
    ComboBox,
    Slider,
    ProgressBar,
    Tab,
    TabList,
    List,
    ListItem,
    Table,
    Row,
    Cell,
    Popup,
    Window,
    Custom
};

struct AccessibilityState {
    bool disabled = false;
    bool focused = false;
    bool focusVisible = false;
    bool selected = false;
    bool checked = false;
    bool pressed = false;
    bool expanded = false;
    bool required = false;
    bool invalid = false;
    bool readOnly = false;
};

struct AccessibilityInfo {
    AccessibilityRole role = AccessibilityRole::None;
    std::wstring name;
    std::wstring description;
    std::wstring value;
    AccessibilityState state;
};

class ONEUI_API Widget {
public:
    virtual ~Widget() = default;

    void setFrame(Rect frame);
    Rect frame() const;
    void setPreferredSize(Size size);
    Size preferredSize() const;
    virtual void setDisabled(bool disabled);
    void bindDisabled(State<bool>& state);
    bool disabled() const;
    virtual void setVisible(bool visible);
    void bindVisible(State<bool>& state);
    bool visible() const;
    bool clearInteractionState();
    virtual void setInvalidator(std::function<void()> invalidator);
    virtual void setRectInvalidator(std::function<void(Rect)> invalidator);
    virtual void setAnimationScheduler(std::function<void()> scheduler);

    virtual void paint(Canvas& canvas) = 0;
    virtual bool onMouseMove(const MouseEvent& event);
    virtual bool onMouseDown(const MouseEvent& event);
    virtual bool onMouseUp(const MouseEvent& event);
    virtual bool onMouseWheel(const MouseWheelEvent& event);
    virtual bool onKeyDown(const KeyEvent& event);
    virtual bool onKeyUp(const KeyEvent& event);
    virtual bool onTextInput(wchar_t character);
    virtual bool onFocusChanged(bool focused);
    virtual bool isFocusable() const;
    virtual bool hitTest(Point point) const;
    virtual CursorKind cursor(Point point) const;
    virtual bool paintsAboveSiblings() const;
    virtual bool tickAnimations(double nowMs);

    bool focused() const;
    bool focusVisible() const;
    virtual void setFocusVisible(bool visible);
    void setAccessibleRole(AccessibilityRole role);
    AccessibilityRole accessibleRole() const;
    void setAccessibleName(std::wstring name);
    const std::wstring& accessibleName() const;
    void setAccessibleDescription(std::wstring description);
    const std::wstring& accessibleDescription() const;
    void setAccessibleValue(std::wstring value);
    const std::wstring& accessibleValue() const;
    void setAccessibilityState(AccessibilityState state);
    AccessibilityState accessibilityState() const;
    virtual AccessibilityInfo accessibilityInfo() const;

protected:
    void invalidate();
    void invalidateRect(Rect rect);
    void requestAnimationFrame();
    bool hasAnimationScheduler() const;
    void setFocused(bool focused);
    bool contains(Point point) const;
    bool interactive() const;
    virtual bool hasInteractionState() const;
    virtual void resetInteractionState();

private:
    Rect frame_;
    Size preferredSize_;
    std::function<void()> invalidator_;
    std::function<void(Rect)> rectInvalidator_;
    std::function<void()> animationScheduler_;
    bool focused_ = false;
    bool focusVisible_ = false;
    bool disabled_ = false;
    bool visible_ = true;
    AccessibilityRole accessibilityRole_ = AccessibilityRole::None;
    std::wstring accessibleName_;
    std::wstring accessibleDescription_;
    std::wstring accessibleValue_;
    AccessibilityState accessibilityState_;
    Binding<bool> disabledBinding_;
    Binding<bool> visibleBinding_;
};

} // namespace oneui
