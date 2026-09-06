#pragma once

#include "oneui/canvas.h"
#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/reactive.h"
#include "oneui/command.h"

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
    int clickCount = 1;
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
    PageUp,
    PageDown,
    Delete,
    A,
    C,
    V,
    X,
    F2
};

enum class CursorKind {
    Default,
    Hidden,
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
    // The legacy 'win' field carries the physical Meta/Command modifier on
    // non-Windows backends. Raw Control remains independent.
    bool editShortcut() const {
#ifdef __APPLE__
        return win;
#else
        return control;
#endif
    }
};

struct TextInputState {
    std::wstring text;
    std::size_t anchor = 0;
    std::size_t caret = 0;
    bool editable = false;
    bool sensitive = false;
    const void* identity = nullptr; // Changes when focus moves between editors.
    bool drawsPreedit = false; // Otherwise the backend draws a caret-anchored overlay.
    std::uint64_t session = 0; // Changes on focus transitions, including returning to the same editor.
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
    Widget();
    virtual ~Widget() { lifetime_.reset(); }

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
    /// Installs the callbacks used by the widget that currently owns this
    /// widget in the native composition tree. Each callback channel tracks
    /// its owner independently so propagation remains linear through nested
    /// containers while reparenting stays safe.
    void attachToOwner(
        const void* owner,
        std::function<void()> invalidator,
        std::function<void(Rect)> rectInvalidator,
        std::function<void()> animationScheduler);
    void attachInvalidatorToOwner(const void* owner, std::function<void()> invalidator);
    void attachRectInvalidatorToOwner(
        const void* owner,
        std::function<void(Rect)> invalidator);
    void attachAnimationSchedulerToOwner(
        const void* owner,
        std::function<void()> scheduler);
    /// Clears composition callbacks only when they still belong to `owner`.
    /// This makes destruction of an old parent safe after a child was
    /// reparented into a new native tree.
    void detachFromOwner(const void* owner);

    virtual void paint(Canvas& canvas) = 0;
    virtual bool onMouseMove(const MouseEvent& event);
    virtual bool onMouseDown(const MouseEvent& event);
    virtual bool onMouseUp(const MouseEvent& event);
    virtual bool onMouseWheel(const MouseWheelEvent& event);
    virtual bool onKeyDown(const KeyEvent& event);
    virtual bool onKeyUp(const KeyEvent& event);
    CommandScope& commands() { return commands_; }
    virtual std::shared_ptr<Widget> activeFocusChild() const { return {}; }
    virtual bool isCommandBoundary() const { return false; }
    virtual bool hasTextComposition() const { return false; }
    virtual CommandResult queryBuiltinCommand(const std::string&) const { return CommandResult::NotFound; }
    virtual CommandResult executeBuiltinCommand(const std::string&) { return CommandResult::NotFound; }
    virtual CommandResult dispatchBuiltinCommandKey(const KeyEvent&, const std::string&) { return CommandResult::NotFound; }
    virtual bool onTextInput(wchar_t character);
    /// Delivers one committed Unicode text unit. The default implementation
    /// preserves legacy character handlers by dispatching each native wchar_t.
    virtual bool onTextInputText(const std::wstring& text);
    // Atomic native IME commit. Legacy onTextInputText retains per-unit callbacks.
    virtual bool onTextCommitted(const std::wstring& text) { return onTextInputText(text); }
    virtual TextInputState textInputState() const { return {}; }
    // Preedit is presentation state, not committed text and not an undo entry.
    virtual void setTextComposition(std::wstring text, std::size_t caret) { (void)text; (void)caret; }
    virtual bool replaceTextRange(std::size_t start, std::size_t end, const std::wstring& text) {
        (void)start; (void)end; (void)text; return false;
    }
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
    // Short, visible help shown while the pointer rests on this widget.
    // Tooltip text is presentation metadata and intentionally remains
    // independent from the longer accessibility description.
    void setTooltip(std::wstring tooltip);
    const std::wstring& tooltip() const;
    virtual const std::wstring* tooltipAt(Point point) const;
    void setAccessibleValue(std::wstring value);
    const std::wstring& accessibleValue() const;
    void setAccessibilityState(AccessibilityState state);
    AccessibilityState accessibilityState() const;
    virtual AccessibilityInfo accessibilityInfo() const;
    // Inherited presentation context, installed by the owning native window.
    virtual void setTextEnvironment(std::wstring family, float scale);
    const std::wstring& textFontFamily() const { return textFontFamily_; }
    float textDpiScale() const { return textDpiScale_; }

protected:
    std::uint64_t textInputSession() const { return textInputSession_; }
    std::weak_ptr<int> lifetimeToken() const { return lifetime_; }
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
    CommandScope commands_;
    std::wstring textFontFamily_;
    float textDpiScale_ = 1.0f;
    Rect frame_;
    Size preferredSize_;
    std::function<void()> invalidator_;
    std::function<void(Rect)> rectInvalidator_;
    std::function<void()> animationScheduler_;
    const void* invalidatorOwner_ = nullptr;
    const void* rectInvalidatorOwner_ = nullptr;
    const void* animationSchedulerOwner_ = nullptr;
    std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
    std::uint64_t textInputSession_ = 0;
    bool focused_ = false;
    bool focusVisible_ = false;
    bool disabled_ = false;
    bool visible_ = true;
    bool tabStop_ = true;
    AccessibilityRole accessibilityRole_ = AccessibilityRole::None;
    std::wstring accessibleName_;
    std::wstring accessibleDescription_;
    std::wstring tooltip_;
    std::wstring accessibleValue_;
    AccessibilityState accessibilityState_;
    Binding<bool> disabledBinding_;
    Binding<bool> visibleBinding_;
};

} // namespace oneui
