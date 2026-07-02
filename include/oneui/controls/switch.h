#pragma once

#include "oneui/export.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <functional>
#include <optional>
#include <string>

namespace oneui {

class ONEUI_API Switch final : public Widget {
public:
    explicit Switch(std::wstring text = {});

    void setText(std::wstring text);
    void setChecked(bool checked);
    bool checked() const;
    void bindChecked(State<bool>& state);
    void setOnChanged(std::function<void(bool)> callback);
    void setStyleOverride(SwitchStyleOverride style);
    void clearStyleOverride();

    void paint(Canvas& canvas) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool isFocusable() const override;

private:
    void toggle();
    void assignChecked(bool checked);
    bool hasInteractionState() const override;
    void resetInteractionState() override;

    std::wstring text_;
    bool checked_ = false;
    bool hovered_ = false;
    bool pressed_ = false;
    Binding<bool> checkedBinding_;
    std::function<void(bool)> onChanged_;
    std::optional<SwitchStyleOverride> styleOverride_;
};

} // namespace oneui
