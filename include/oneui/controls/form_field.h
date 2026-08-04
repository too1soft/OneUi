#pragma once

#include "oneui/controls/validation_message.h"
#include "oneui/export.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <memory>
#include <optional>
#include <string>

namespace oneui {

class ONEUI_API FormField final : public Widget {
public:
    FormField();
    ~FormField() override;

    void setChild(std::shared_ptr<Widget> child);
    std::shared_ptr<Widget> child() const;
    void clearChild();
    void setLabel(std::wstring label);
    const std::wstring& label() const;
    void bindLabel(State<std::wstring>& state);
    void setHelperText(std::wstring text);
    const std::wstring& helperText() const;
    void bindHelperText(State<std::wstring>& state);
    void setErrorText(std::wstring text);
    const std::wstring& errorText() const;
    void bindErrorText(State<std::wstring>& state);
    void setRequired(bool required);
    bool required() const;
    void bindRequired(State<bool>& state);
    void setInvalid(bool invalid);
    bool invalid() const;
    void bindInvalid(State<bool>& state);
    void setStyleOverride(FormFieldStyleOverride style);
    void clearStyleOverride();

    void setInvalidator(std::function<void()> invalidator) override;
    void setRectInvalidator(std::function<void(Rect)> invalidator) override;
    void setAnimationScheduler(std::function<void()> scheduler) override;
    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseWheelEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onTextInput(wchar_t character) override;
    bool onTextInputText(const std::wstring& text) override;
    bool onFocusChanged(bool focused) override;
    bool isFocusable() const override;
    CursorKind cursor(Point point) const override;
    void setFocusVisible(bool visible) override;
    bool tickAnimations(double nowMs) override;

private:
    void installChildCallbacks();
    FormFieldStyle resolvedStyle() const;
    void layoutChild();
    void updatePreferredSize();
    const std::wstring& activeMessageText() const;
    bool hasLabel() const;
    bool hasMessage() const;
    void applyChildAccessibility();
    void resetInteractionState() override;

    std::shared_ptr<Widget> child_;
    std::wstring label_;
    std::wstring helperText_;
    std::wstring errorText_;
    bool required_ = false;
    bool invalid_ = false;
    Binding<std::wstring> labelBinding_;
    Binding<std::wstring> helperTextBinding_;
    Binding<std::wstring> errorTextBinding_;
    Binding<bool> requiredBinding_;
    Binding<bool> invalidBinding_;
    std::optional<FormFieldStyleOverride> styleOverride_;
    std::wstring propagatedAccessibleName_;
    std::wstring propagatedAccessibleDescription_;
};

} // namespace oneui
