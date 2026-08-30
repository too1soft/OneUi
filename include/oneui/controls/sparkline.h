#pragma once

#include "oneui/export.h"
#include "oneui/style_sheet.h"
#include "oneui/widget.h"

#include <optional>
#include <vector>

namespace oneui {

/// Compact, non-interactive time-series visualization for operational UIs.
///
/// Values are normalized to the [0, 1] range. The control owns its sample
/// buffer and paints only geometry, so worker-produced updates stay cheap.
class ONEUI_API Sparkline final : public Widget {
public:
    Sparkline();

    void setValues(std::vector<double> values);
    const std::vector<double>& values() const;
    void setStyleBox(StyleBox style);
    void clearStyleBox();

    void paint(Canvas& canvas) override;

private:
    std::vector<double> values_;
    std::optional<StyleBox> styleBox_;
};

} // namespace oneui
