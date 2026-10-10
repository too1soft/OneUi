#include "oneui/widget.h"
namespace oneui {
bool Widget::commitTextComposition() {
    auto child=activeFocusChild();return child && child->commitTextComposition();
}
void Widget::setLayoutVisible(bool value) {
    if(layoutVisible_==value)return;
    if(!value) {
        commitTextComposition();
        onFocusChanged(false);
        clearInteractionState();
    }
    layoutVisible_=value;invalidate();
}
} // namespace oneui
