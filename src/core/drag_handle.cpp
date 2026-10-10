#include "oneui/controls/drag_handle.h"
#include <cmath>
namespace oneui {
bool DragHandle::onMouseDown(const MouseEvent& event) {
    if(!interactive() || event.button!=MouseButton::Left)return false;
    start_=point_=event.position;armed_=true;return true;
}
bool DragHandle::onMouseMove(const MouseEvent& event) {
    if(!armed_)return View::onMouseMove(event);
    point_=event.position;
    if(!dragging_ && std::hypot(point_.x-start_.x,point_.y-start_.y)>=threshold) {
        dragging_=true;if(callback_)callback_(0,point_);
    }
    if(dragging_ && callback_)callback_(1,point_);
    return true;
}
bool DragHandle::onMouseUp(const MouseEvent& event) {
    if(!armed_)return false;
    auto dragging=dragging_;armed_=dragging_=false;
    if(dragging && callback_)callback_(2,event.position);
    return true;
}
bool DragHandle::onKeyDown(const KeyEvent& event) {
    if(armed_ && event.pressed && event.key==Key::Escape){resetInteractionState();return true;}
    return View::onKeyDown(event);
}
void DragHandle::resetInteractionState() {
    auto dragging=dragging_;armed_=dragging_=false;View::resetInteractionState();
    if(dragging && callback_)callback_(3,point_);
}
} // namespace oneui
