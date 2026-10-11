#pragma once
#include "oneui/layout/scroll_view.h"
#include "oneui/layout/stack.h"

namespace oneui {
// Rich templated table viewport. The header follows horizontal scrolling but
// stays pinned vertically; body focus, input and clipping remain native.
// Rows are authored widgets: use bounded paging for large result sets. For
// unbounded simple records use DataTable/VirtualList's data virtualization.
class TemplateTable final : public View {
public:
    TemplateTable(std::shared_ptr<Widget> header, std::shared_ptr<Widget> body)
        : header_(std::move(header)), body_(std::move(body)),
          vertical_(std::make_shared<ScrollView>()), horizontal_(std::make_shared<ScrollView>()),
          column_(std::make_shared<Stack>()) {
        setAccessibleRole(AccessibilityRole::Table);
        column_->setGap(0);
        header_->setPreferredSize({0, 34});
        column_->add(header_);
        vertical_->setPreferredSize({0, 0});
        vertical_->setContent(body_);
        column_->add(vertical_);
        horizontal_->setContent(column_);
        add(horizontal_);
    }
    void setMinimumWidth(float value) { horizontal_->setContentWidth(value); }
    void setHeaderHeight(float value) { header_->setPreferredSize({0, value}); invalidate(); }
    void setContentHeight(float value) { vertical_->setContentHeight(value); }
    void setContentWidth(float value) { vertical_->setContentWidth(value); }
    void setWheelStep(float value) { vertical_->setWheelStep(value); }
    void setScrollOffset(float value) { vertical_->setScrollOffset(value); }
    float scrollOffset() const { return vertical_->scrollOffset(); }
    float maxScrollOffset() const { return vertical_->maxScrollOffset(); }
    void setHorizontalScrollOffset(float value) { horizontal_->setHorizontalScrollOffset(value); }
    float horizontalScrollOffset() const { return horizontal_->horizontalScrollOffset(); }
    void setStyleBox(const StyleBox& box) {
        vertical_->setStyleBox(box);
        horizontal_->setStyleBox(box);
        vertical_->setScrollbarStyle(box.scrollbarColor.value_or(box.foreground.value_or(Color{148,163,184,180})),
                                     box.scrollbarWidth.value_or(4));
    }
protected:
    void layoutChildren() override { horizontal_->setFrame(frame()); }
private:
    std::shared_ptr<Widget> header_, body_;
    std::shared_ptr<ScrollView> vertical_, horizontal_;
    std::shared_ptr<Stack> column_;
};
}
