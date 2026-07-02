#pragma once

#include "oneui/export.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <optional>
#include <string>

namespace oneui {

enum class BadgeVariant {
    Neutral,
    Success,
    Warning,
    Danger,
    Accent
};

class ONEUI_API Badge final : public Widget {
public:
    explicit Badge(std::wstring text = {}, BadgeVariant variant = BadgeVariant::Neutral);

    void setText(std::wstring text);
    const std::wstring& text() const;
    void bindText(State<std::wstring>& state);
    void setVariant(BadgeVariant variant);
    void setStyleOverride(BadgeStyleOverride style);
    void clearStyleOverride();

    void paint(Canvas& canvas) override;

private:
    BadgeStyle resolvedStyle() const;

    std::wstring text_;
    Binding<std::wstring> textBinding_;
    BadgeVariant variant_ = BadgeVariant::Neutral;
    std::optional<BadgeStyleOverride> styleOverride_;
};

} // namespace oneui
