#pragma once
#include "oneui/layout/stack.h"

namespace oneui {
enum class PanePattern { Sidebar, MasterDetail };
// Two retained panes; Yoga still owns all child geometry. Narrow master/detail
// shows one pane at a time, while navigation reflows above the main content.
class ONEUI_API AdaptivePanes final : public View {
public:
    AdaptivePanes(PanePattern pattern,std::shared_ptr<Widget> first,std::shared_ptr<Widget> second);
    void setBreakpoint(float width);
    void setPaneWidth(float width);
    void setDetailOpen(bool open);
    // Explicit navigation also moves keyboard focus in the two-pane layout.
    bool activatePane(bool detail);
    bool detailOpen() const { return detailOpen_; }
    bool narrow() const { return narrow_; }
    void setStyleBox(StyleBox style);
    Size measure(Size available) const override;
    void paint(Canvas& canvas) override;
    Rect paintBounds() const override;
protected:
    void layoutChildren() override;
private:
    void configure(float width);
    PanePattern pattern_;
    std::shared_ptr<Stack> layout_;
    std::shared_ptr<Widget> first_,second_;
    StyleBox style_;
    float breakpoint_,paneWidth_,configuredPaneWidth_=-1;
    bool detailOpen_=false,narrow_=false,configured_=false;
};
}
