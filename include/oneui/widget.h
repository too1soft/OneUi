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
    bool shift = false;
    bool control = false;
    bool alt = false;
};

struct MouseWheelEvent {
    Point position;
    float deltaY = 0.0f;
    bool shift = false;
    bool control = false;
    bool alt = false;
    double timestampMs = 0.0;
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
    X,
    F2
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
    /// Delivers one committed Unicode text unit. The default implementation
    /// preserves legacy character handlers by dispatching each UTF-16 code unit.
    virtual bool onTextInputText(const std::wstring& text);
    /// Returns the logical client-space rectangle where an IME should place
    /// its composition and candidate UI. Containers forward this to their
    /// focused descendant.
    virtual Rect textInputCaretRect() const;
    virtual bool onFocusChanged(bool focused);
    virtual bool isFocusable() const;
    // tabStop：是否参与 Tab 焦点遍历。窗口按钮、纯装饰性可点区域设 false，排除出 Tab 序。
    // 默认 true；焦点收集（focusableChildren/focusableOverlays）会同时要求 isFocusable && tabStop。
    bool tabStop() const;
    void setTabStop(bool value);
    // focusFirstLeaf/focusLastLeaf：把焦点落到本控件子树内的首/末个可聚焦叶子（用于 Tab 回绕与进入容器）。
    virtual bool focusFirstLeaf();
    virtual bool focusLastLeaf();
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
    bool tabStop_ = true;
    AccessibilityRole accessibilityRole_ = AccessibilityRole::None;
    std::wstring accessibleName_;
    std::wstring accessibleDescription_;
    std::wstring accessibleValue_;
    AccessibilityState accessibilityState_;
    Binding<bool> disabledBinding_;
    Binding<bool> visibleBinding_;
};

} // namespace oneui
