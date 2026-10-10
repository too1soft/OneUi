#include "oneui/layout/scroll_view.h"
#include "oneui/layout/split_view.h"
#include "oneui/layout/adaptive_panes.h"
namespace oneui {
void ScrollView::syncPreparedContent() noexcept {
    content_=children().empty() ? nullptr : children().front();
}
void SplitView::syncPreparedContent() noexcept {
    // Factory contract: first pane, retained divider, second pane.
    if(children().size()==3) {first_=children()[0];second_=children()[2];}
}
void AdaptivePanes::syncPreparedContent() noexcept {
    if(layout_->children().size()==2) {
        first_=layout_->children()[0];second_=layout_->children()[1];configured_=false;
    }
}
}
