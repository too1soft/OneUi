#pragma once

#include "oneui/export.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <optional>
#include <string>

namespace oneui {

enum class ValidationMessageTone {
    Helper,
    Error
};

class ONEUI_API ValidationMessage final : public Widget {
public:
    explicit ValidationMessage(std::wstring text = {});

    void setText(std::wstring text);
    const std::wstring& text() const;
    void bindText(State<std::wstring>& state);
    void setTone(ValidationMessageTone tone);
    ValidationMessageTone tone() const;
    void setStyleOverride(ValidationMessageStyleOverride style);
    void clearStyleOverride();

    void paint(Canvas& canvas) override;

private:
    ValidationMessageStyle resolvedStyle() const;

    std::wstring text_;
    ValidationMessageTone tone_ = ValidationMessageTone::Helper;
    Binding<std::wstring> textBinding_;
    std::optional<ValidationMessageStyleOverride> styleOverride_;
};

} // namespace oneui
