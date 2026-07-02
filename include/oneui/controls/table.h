#pragma once

#include "oneui/export.h"
#include "oneui/style.h"
#include "oneui/widget.h"

#include <optional>
#include <string>
#include <vector>

namespace oneui {

struct TableColumn {
    std::wstring header;
    float width = 0.0f;
};

class ONEUI_API Table final : public Widget {
public:
    Table();

    void setColumns(std::vector<TableColumn> columns);
    void setRows(std::vector<std::vector<std::wstring>> rows);
    void setStyleOverride(TableStyleOverride style);
    void clearStyleOverride();

    void paint(Canvas& canvas) override;
    AccessibilityInfo accessibilityInfo() const override;

private:
    TableStyle resolvedStyle() const;
    float columnWidth(int index, float remainingWidth, int flexibleCount) const;

    std::vector<TableColumn> columns_;
    std::vector<std::vector<std::wstring>> rows_;
    std::optional<TableStyleOverride> styleOverride_;
};

} // namespace oneui
