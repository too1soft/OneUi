#pragma once

#include "oneui/export.h"
#include "oneui/geometry.h"
#include "oneui/style_sheet.h"
#include "oneui/view.h"

#include <memory>
#include <optional>

namespace oneui {

class ONEUI_API AppShell final : public View {
public:
    void setSidebar(std::shared_ptr<Widget> child);
    void setHeader(std::shared_ptr<Widget> child);
    void setContent(std::shared_ptr<Widget> child);
    void setFooter(std::shared_ptr<Widget> child);

    void setPadding(Insets padding);
    void setGap(float gap);
    void setSidebarWidth(float width);
    void setHeaderHeight(float height);
    void setFooterHeight(float height);
    /// When enabled, the footer reserves the full shell width before the
    /// sidebar is laid out. This is useful for desktop status bars whose
    /// top separator must not be interrupted by navigation.
    void setFooterSpanSidebar(bool span);
    void setSidebarVisible(bool visible);
    void setStyleBox(StyleBox style);
    void clearStyleBox();

    bool sidebarVisible() const;
    float sidebarWidth() const;
    float headerHeight() const;
    float footerHeight() const;
    bool footerSpansSidebar() const;

    void paint(Canvas& canvas) override;

private:
    void layoutChildren() override;
    void rebuildChildren();

    std::shared_ptr<Widget> sidebar_;
    std::shared_ptr<Widget> header_;
    std::shared_ptr<Widget> content_;
    std::shared_ptr<Widget> footer_;
    Insets padding_;
    float gap_ = 0.0f;
    float sidebarWidth_ = 248.0f;
    float headerHeight_ = 64.0f;
    float footerHeight_ = 28.0f;
    bool footerSpansSidebar_ = false;
    bool sidebarVisible_ = true;
    std::optional<StyleBox> styleBox_;
};

} // namespace oneui
