#pragma once

#include "oneui/export.h"
#include "oneui/reactive.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <optional>

namespace oneui {

class ONEUI_API ProgressBar final : public Widget {
public:
    ProgressBar();

    void setValue(double value);
    double value() const;
    void bindValue(State<double>& state);
    void setStyleOverride(ProgressBarStyleOverride style);
    void clearStyleOverride();

    void paint(Canvas& canvas) override;

private:
    double clampedValue() const;
    ProgressBarStyle resolvedStyle() const;

    double value_ = 0.0;
    Binding<double> valueBinding_;
    std::optional<ProgressBarStyleOverride> styleOverride_;
};

} // namespace oneui
